/*
 * HWMON driver for AMDGPU gpu_metrics
 *
 * Copyright (C) 2025  Rongrong <i@rong.moe>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see
 * <https://www.gnu.org/licenses/>.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/cleanup.h>
#include <linux/device.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/rwsem.h>
#include <linux/uaccess.h>

#include "amdgpu_metrics.h"

#define MODULE_NAME	"amdgpu_metrics"

/*
 * It is not a good idea to open files from kernel space, but this is the least
 * hacky approach to achieve the goal as an out-of-tree module.
 *
 * Other approaches:
 *   - Get the address of amdgpu_dpm_get_gpu_metrics somehow, and call it
 *     directly. However, we still need to get a pointer to the corresponding
 *     device struct.
 *   - Hack the SMU table directly.
 */
#define MAX_PATH_SIZE 256
#define DEFAULT_GPU_METRICS_PATH "/sys/class/drm/renderD128/device/gpu_metrics"
static char gpu_metrics_path[MAX_PATH_SIZE] = DEFAULT_GPU_METRICS_PATH;
module_param_string(gpu_metrics, gpu_metrics_path, MAX_PATH_SIZE, 0444);
MODULE_PARM_DESC(gpu_metrics,
	"Path to gpu_metrics. "
	"(Empty): ERROR! "
	"Default: " DEFAULT_GPU_METRICS_PATH);

#define MAX_HWMON_NAME 32
#define DEFAULT_PER_CORE_HWMON_NAME "cpu_thermal"
static char per_core_hwmon_name[MAX_HWMON_NAME] = DEFAULT_PER_CORE_HWMON_NAME;
module_param_string(per_core_hwmon, per_core_hwmon_name, MAX_HWMON_NAME, 0444);
MODULE_PARM_DESC(per_core_hwmon,
	"Name of per-CPU-core HWMON device. "
	"(Empty): Merge into the main HWMON device. "
	"Default: " DEFAULT_PER_CORE_HWMON_NAME);

#define UPDATE_INTERVAL_MS 100
#define UPDATE_INTERVAL_JIFFIES (UPDATE_INTERVAL_MS * HZ / 1000)

struct amdgpu_metrics_private {
	struct amdgpu_metrics_private_common common;
	const char *path;
	/*  */
	remap_t per_core_channel_remap[NCORES];

	struct rw_semaphore metrics_lock;
	unsigned long last_update_jiffies;
};

/* A magical thief stole something from HWMON... */
#define hwmon_magic_freq	/* enum hwmon_sensor_types */	hwmon_intrusion
#define hwmon_magic_freq_input		/* u32 */		0x8D8D8D8D
#define hwmon_magic_freq_label		/* u32 */		0x0D0D0D0D
#define hwmon_magic_freq_idx_main	/* u8 */		0x8D
#define hwmon_magic_freq_idx_per_core	/* u8 */		0x0D

static umode_t amdgpu_metrics_hwmon_is_visible(const void *drvdata,
					       enum hwmon_sensor_types type,
					       u32 attr, int channel)
{
	struct amdgpu_metrics_private *priv = (struct amdgpu_metrics_private *)drvdata;
	bool visible = false;

	if (type == hwmon_temp)
		visible = (channel < NCHANNELS_TEMP &&
			   priv->common.remap.temp.data[channel].valid &&
			   !priv->common.remap.temp.data[channel].ext);
	else if (type == hwmon_power)
		visible = (channel < NCHANNELS_POWER &&
			   priv->common.remap.power.data[channel].valid &&
			   !priv->common.remap.power.data[channel].ext);
	else if (type == hwmon_magic_freq)
		visible = (channel < NCHANNELS_FREQ &&
			   priv->common.remap.freq.data[channel].valid &&
			   !priv->common.remap.freq.data[channel].ext);

	return visible ? 0444 : 0;
}

static umode_t amdgpu_metrics_per_core_is_visible(const void *drvdata,
						  enum hwmon_sensor_types type,
						  u32 attr, int channel)
{
	struct amdgpu_metrics_private *priv = (struct amdgpu_metrics_private *)drvdata;
	bool visible = false;

	if (WARN_ON(channel >= NCORES) ||
	    !priv->per_core_channel_remap[channel].valid)
		goto out;

	channel = priv->per_core_channel_remap[channel].idx;

	if (type == hwmon_temp)
		visible = priv->common.remap.temp.core[channel].valid;
	else if (type == hwmon_power)
		visible = priv->common.remap.power.core[channel].valid;
	else if (type == hwmon_magic_freq)
		visible = priv->common.remap.freq.coreclk[channel].valid;

out:
	return visible ? 0444 : 0;
}

static umode_t amdgpu_metrics_hwmon_visible_shim(struct kobject *kobj,
						 struct attribute *attr, int index)
{
	const void *drvdata = dev_get_drvdata(kobj_to_dev(kobj));
	struct device_attribute *dev_attr = container_of(attr, struct device_attribute, attr);
	struct sensor_device_attribute_2 *sensor_attr = to_sensor_dev_attr_2(dev_attr);
	bool visible;

	visible = sensor_attr->index == hwmon_magic_freq_idx_per_core
		? amdgpu_metrics_per_core_is_visible(drvdata, hwmon_magic_freq,
						     0, sensor_attr->nr - 1)
		: amdgpu_metrics_hwmon_is_visible(drvdata, hwmon_magic_freq,
						  0, sensor_attr->nr - 1);

	return visible ? attr->mode : 0;
}

static int amdgpu_metrics_hwmon_read_string(struct device *dev, enum hwmon_sensor_types type,
					    u32 attr, int channel, const char **str)
{
	struct amdgpu_metrics_private *priv = dev_get_drvdata(dev);

	if (type == hwmon_temp && attr == hwmon_temp_label)
		*str = amdgpu_metrics_labels_temp[priv->common.remap.temp.data[channel].idx];
	else if (type == hwmon_power && attr == hwmon_power_label)
		*str = amdgpu_metrics_labels_power[priv->common.remap.power.data[channel].idx];
	else if (type == hwmon_magic_freq && attr == hwmon_magic_freq_label)
		*str = amdgpu_metrics_labels_freq[priv->common.remap.freq.data[channel].idx];
	else
		return -EOPNOTSUPP;

	return 0;
}

static ssize_t amdgpu_metrics_hwmon_label_shim(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	struct sensor_device_attribute_2 *sensor_attr = to_sensor_dev_attr_2(attr);
	const char *label;
	int err;

	err = amdgpu_metrics_hwmon_read_string(dev, hwmon_magic_freq, hwmon_magic_freq_label,
					       sensor_attr->nr - 1, &label);

	return err ?: sysfs_emit(buf, "%s\n", label);
}

static ssize_t amdgpu_metrics_read_gpu_metrics(const char *path,
					       struct metrics_table_header *metrics,
					       size_t buf_size)
{
	struct file *filp;
	ssize_t ret;

	filp = filp_open(path, O_RDONLY, 0);
	if (IS_ERR(filp)) {
		pr_err("Failed to open %s\n", path);
		return PTR_ERR(filp);
	}

	ret = kernel_read(filp, metrics, buf_size, NULL);
	filp_close(filp, NULL);

	if (ret < 0) {
		pr_err("Failed to read GPU metrics: %zd\n", ret);
		return ret;
	}

	if (ret < sizeof(struct metrics_table_header)) {
		pr_err("Invalid GPU metrics size: %zd < %lu\n",
		       ret, sizeof(struct metrics_table_header));
		return -EIO;
	}

	if (ret != metrics->structure_size) {
		pr_err("GPU metrics size mismatch: read %zd, declared %d\n",
		       ret, metrics->structure_size);
		return -EIO;
	}

	return ret;
}

/*
 * <0: error
 * 0: no need to update
 * >0: updated
 */
static int amdgpu_metrics_update_gpu_metrics(struct amdgpu_metrics_private *priv)
{
	ssize_t size;

	if (time_before(jiffies, priv->last_update_jiffies + UPDATE_INTERVAL_JIFFIES))
		return 0;

	guard(rwsem_write)(&priv->metrics_lock);

	size = amdgpu_metrics_read_gpu_metrics(priv->path, &priv->common.metrics.header,
					       priv->common.channels->metrics_size);
	if (size < 0)
		return size;

	if (size != priv->common.channels->metrics_size)
		return -EIO;

	priv->last_update_jiffies = jiffies;

	return 1;
}

/*
 * Temp: centi-Celsius to milli-Celsius
 * Power: mW to uW
 * Freq: MHz to Hz
 */
#define GET_MULTIPLIER(_hwmon_type)			\
	((_hwmon_type) == hwmon_temp ? 10 :		\
	 (_hwmon_type) == hwmon_power ? 1000 :		\
	 (_hwmon_type) == hwmon_magic_freq ? 1000000 :	\
	 0)

static int amdgpu_metrics_hwmon_read(struct device *dev, enum hwmon_sensor_types type,
				     u32 attr, int channel, long *val)
{
	struct amdgpu_metrics_private *priv = dev_get_drvdata(dev);
	int err = -EOPNOTSUPP;
	uint64_t raw;
	uint32_t multiplier = GET_MULTIPLIER(type);

	if (WARN_ON(multiplier == 0))
		return err;

	if (amdgpu_metrics_update_gpu_metrics(priv) < 0)
		return -EIO;

	guard(rwsem_read)(&priv->metrics_lock);

	if (type == hwmon_temp && attr == hwmon_temp_input)
		err = GET_TEMP(&priv->common, channel, &raw);
	else if (type == hwmon_power && attr == hwmon_power_input)
		err = GET_POWER(&priv->common, channel, &raw);
	else if (type == hwmon_magic_freq && attr == hwmon_magic_freq_input)
		err = GET_FREQ(&priv->common, channel, &raw);

	if (err)
		return err;

	*val = raw * multiplier;
	return 0;
}

static int amdgpu_metrics_per_core_read(struct device *dev, enum hwmon_sensor_types type,
					u32 attr, int channel, long *val)
{
	struct amdgpu_metrics_private *priv = dev_get_drvdata(dev);
	int err = -EOPNOTSUPP;
	uint64_t raw;
	uint32_t multiplier = GET_MULTIPLIER(type);

	if (WARN_ON(multiplier == 0 || channel >= NCORES))
		return err;

	if (amdgpu_metrics_update_gpu_metrics(priv) < 0)
		return -EIO;

	channel = priv->per_core_channel_remap[channel].idx;

	guard(rwsem_read)(&priv->metrics_lock);

	if (type == hwmon_temp && attr == hwmon_temp_input)
		err = GET_CORE_TEMP(&priv->common, channel, &raw);
	else if (type == hwmon_power && attr == hwmon_power_input)
		err = GET_CORE_POWER(&priv->common, channel, &raw);
	else if (type == hwmon_magic_freq && attr == hwmon_magic_freq_input)
		err = GET_CORE_FREQ(&priv->common, channel, &raw);

	if (err)
		return err;

	*val = raw * multiplier;
	return 0;
}

static ssize_t amdgpu_metrics_hwmon_input_shim(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	struct sensor_device_attribute_2 *sensor_attr = to_sensor_dev_attr_2(attr);
	long val;
	int err;

	err = sensor_attr->index == hwmon_magic_freq_idx_per_core
		? amdgpu_metrics_per_core_read(dev, hwmon_magic_freq, hwmon_magic_freq_input,
					       sensor_attr->nr - 1, &val)
		: amdgpu_metrics_hwmon_read(dev, hwmon_magic_freq, hwmon_magic_freq_input,
					    sensor_attr->nr - 1, &val);

	return err ?: sysfs_emit(buf, "%ld\n", val);
}

#define PREFIXED_SENSOR_DEVICE_ATTR_2_RO(_prefix, _name, _func, _nr, _index)	\
struct sensor_device_attribute_2 sensor_dev_attr_ ##_prefix ##_ ##_name		\
	= SENSOR_ATTR_2(_name, 0444, _func, NULL, _nr, _index)

#define MAIN_SENSOR_DEVICE_ATTR(_name, _nr)					\
static PREFIXED_SENSOR_DEVICE_ATTR_2_RO(hwmon, _name ##_nr ##_label,		\
	amdgpu_metrics_hwmon_label_shim, _nr, hwmon_magic_freq_idx_main);	\
static PREFIXED_SENSOR_DEVICE_ATTR_2_RO(hwmon, _name ##_nr ##_input,		\
	amdgpu_metrics_hwmon_input_shim, _nr, hwmon_magic_freq_idx_main)

#define REF_MAIN_SENSOR_DEVICE_ATTR(_name, _nr)					\
	&sensor_dev_attr_hwmon_ ##_name ##_nr ##_label.dev_attr.attr,		\
	&sensor_dev_attr_hwmon_ ##_name ##_nr ##_input.dev_attr.attr

MAIN_SENSOR_DEVICE_ATTR(freq, 1);
MAIN_SENSOR_DEVICE_ATTR(freq, 2);
MAIN_SENSOR_DEVICE_ATTR(freq, 3);
MAIN_SENSOR_DEVICE_ATTR(freq, 4);
MAIN_SENSOR_DEVICE_ATTR(freq, 5);
MAIN_SENSOR_DEVICE_ATTR(freq, 6);
MAIN_SENSOR_DEVICE_ATTR(freq, 7);
MAIN_SENSOR_DEVICE_ATTR(freq, 8);
MAIN_SENSOR_DEVICE_ATTR(freq, 9);
MAIN_SENSOR_DEVICE_ATTR(freq, 10);
MAIN_SENSOR_DEVICE_ATTR(freq, 11);
MAIN_SENSOR_DEVICE_ATTR(freq, 12);
MAIN_SENSOR_DEVICE_ATTR(freq, 13);
MAIN_SENSOR_DEVICE_ATTR(freq, 14);
MAIN_SENSOR_DEVICE_ATTR(freq, 15);
MAIN_SENSOR_DEVICE_ATTR(freq, 16);
MAIN_SENSOR_DEVICE_ATTR(freq, 17);
MAIN_SENSOR_DEVICE_ATTR(freq, 18);
MAIN_SENSOR_DEVICE_ATTR(freq, 19);
MAIN_SENSOR_DEVICE_ATTR(freq, 20);
MAIN_SENSOR_DEVICE_ATTR(freq, 21);
MAIN_SENSOR_DEVICE_ATTR(freq, 22);
MAIN_SENSOR_DEVICE_ATTR(freq, 23);
MAIN_SENSOR_DEVICE_ATTR(freq, 24);
MAIN_SENSOR_DEVICE_ATTR(freq, 25);
MAIN_SENSOR_DEVICE_ATTR(freq, 26);
MAIN_SENSOR_DEVICE_ATTR(freq, 27);
MAIN_SENSOR_DEVICE_ATTR(freq, 28);
MAIN_SENSOR_DEVICE_ATTR(freq, 29);
MAIN_SENSOR_DEVICE_ATTR(freq, 30);
MAIN_SENSOR_DEVICE_ATTR(freq, 31);
MAIN_SENSOR_DEVICE_ATTR(freq, 32);
MAIN_SENSOR_DEVICE_ATTR(freq, 33);
MAIN_SENSOR_DEVICE_ATTR(freq, 34);
MAIN_SENSOR_DEVICE_ATTR(freq, 35);
MAIN_SENSOR_DEVICE_ATTR(freq, 36);
MAIN_SENSOR_DEVICE_ATTR(freq, 37);
MAIN_SENSOR_DEVICE_ATTR(freq, 38);
MAIN_SENSOR_DEVICE_ATTR(freq, 39);
MAIN_SENSOR_DEVICE_ATTR(freq, 40);
MAIN_SENSOR_DEVICE_ATTR(freq, 41);
MAIN_SENSOR_DEVICE_ATTR(freq, 42);
MAIN_SENSOR_DEVICE_ATTR(freq, 43);

static const struct hwmon_channel_info *const amdgpu_metrics_hwmon_info[] = {
	HWMON_CHANNEL_INFO(temp, REPEAT_NCHANNELS_TEMP(HWMON_T_INPUT | HWMON_T_LABEL)),
	HWMON_CHANNEL_INFO(power, REPEAT_NCHANNELS_POWER(HWMON_P_INPUT | HWMON_P_LABEL)),
	NULL
};

static struct attribute *amdgpu_metrics_hwmon_attributes[] = {
	REF_MAIN_SENSOR_DEVICE_ATTR(freq, 1),
	REF_MAIN_SENSOR_DEVICE_ATTR(freq, 2),
	REF_MAIN_SENSOR_DEVICE_ATTR(freq, 3),
	REF_MAIN_SENSOR_DEVICE_ATTR(freq, 4),
	REF_MAIN_SENSOR_DEVICE_ATTR(freq, 5),
	REF_MAIN_SENSOR_DEVICE_ATTR(freq, 6),
	REF_MAIN_SENSOR_DEVICE_ATTR(freq, 7),
	REF_MAIN_SENSOR_DEVICE_ATTR(freq, 8),
	REF_MAIN_SENSOR_DEVICE_ATTR(freq, 9),
	REF_MAIN_SENSOR_DEVICE_ATTR(freq, 10),
	REF_MAIN_SENSOR_DEVICE_ATTR(freq, 11),
	REF_MAIN_SENSOR_DEVICE_ATTR(freq, 12),
	REF_MAIN_SENSOR_DEVICE_ATTR(freq, 13),
	REF_MAIN_SENSOR_DEVICE_ATTR(freq, 14),
	REF_MAIN_SENSOR_DEVICE_ATTR(freq, 15),
	REF_MAIN_SENSOR_DEVICE_ATTR(freq, 16),
	REF_MAIN_SENSOR_DEVICE_ATTR(freq, 17),
	REF_MAIN_SENSOR_DEVICE_ATTR(freq, 18),
	REF_MAIN_SENSOR_DEVICE_ATTR(freq, 19),
	REF_MAIN_SENSOR_DEVICE_ATTR(freq, 20),
	REF_MAIN_SENSOR_DEVICE_ATTR(freq, 21),
	REF_MAIN_SENSOR_DEVICE_ATTR(freq, 22),
	REF_MAIN_SENSOR_DEVICE_ATTR(freq, 23),
	REF_MAIN_SENSOR_DEVICE_ATTR(freq, 24),
	REF_MAIN_SENSOR_DEVICE_ATTR(freq, 25),
	REF_MAIN_SENSOR_DEVICE_ATTR(freq, 26),
	REF_MAIN_SENSOR_DEVICE_ATTR(freq, 27),
	REF_MAIN_SENSOR_DEVICE_ATTR(freq, 28),
	REF_MAIN_SENSOR_DEVICE_ATTR(freq, 29),
	REF_MAIN_SENSOR_DEVICE_ATTR(freq, 30),
	REF_MAIN_SENSOR_DEVICE_ATTR(freq, 31),
	REF_MAIN_SENSOR_DEVICE_ATTR(freq, 32),
	REF_MAIN_SENSOR_DEVICE_ATTR(freq, 33),
	REF_MAIN_SENSOR_DEVICE_ATTR(freq, 34),
	REF_MAIN_SENSOR_DEVICE_ATTR(freq, 35),
	REF_MAIN_SENSOR_DEVICE_ATTR(freq, 36),
	REF_MAIN_SENSOR_DEVICE_ATTR(freq, 37),
	REF_MAIN_SENSOR_DEVICE_ATTR(freq, 38),
	REF_MAIN_SENSOR_DEVICE_ATTR(freq, 39),
	REF_MAIN_SENSOR_DEVICE_ATTR(freq, 40),
	REF_MAIN_SENSOR_DEVICE_ATTR(freq, 41),
	REF_MAIN_SENSOR_DEVICE_ATTR(freq, 42),
	REF_MAIN_SENSOR_DEVICE_ATTR(freq, 43),
	NULL
};

static const struct attribute_group amdgpu_metrics_hwmon_attrgroup = {
	.attrs = amdgpu_metrics_hwmon_attributes,
	.is_visible = amdgpu_metrics_hwmon_visible_shim,
};

static const struct attribute_group *amdgpu_metrics_hwmon_attrgroups[] = {
	&amdgpu_metrics_hwmon_attrgroup,
	NULL
};

static const struct hwmon_ops amdgpu_metrics_hwmon_ops = {
	.is_visible = amdgpu_metrics_hwmon_is_visible,
	.read = amdgpu_metrics_hwmon_read,
	.read_string = amdgpu_metrics_hwmon_read_string,
};

static const struct hwmon_chip_info amdgpu_metrics_hwmon_chip_info = {
	.ops = &amdgpu_metrics_hwmon_ops,
	.info = amdgpu_metrics_hwmon_info,
};

#define PER_CORE_SENSOR_DEVICE_ATTR(_name, _nr)					\
static PREFIXED_SENSOR_DEVICE_ATTR_2_RO(per_core, _name ##_nr ##_input,		\
	amdgpu_metrics_hwmon_input_shim, _nr, hwmon_magic_freq_idx_per_core)

#define REF_PRE_CORE_SENSOR_DEVICE_ATTR(_name, _nr)				\
	&sensor_dev_attr_per_core_ ##_name ##_nr ##_input.dev_attr.attr

PER_CORE_SENSOR_DEVICE_ATTR(freq, 1);
PER_CORE_SENSOR_DEVICE_ATTR(freq, 2);
PER_CORE_SENSOR_DEVICE_ATTR(freq, 3);
PER_CORE_SENSOR_DEVICE_ATTR(freq, 4);
PER_CORE_SENSOR_DEVICE_ATTR(freq, 5);
PER_CORE_SENSOR_DEVICE_ATTR(freq, 6);
PER_CORE_SENSOR_DEVICE_ATTR(freq, 7);
PER_CORE_SENSOR_DEVICE_ATTR(freq, 8);
PER_CORE_SENSOR_DEVICE_ATTR(freq, 9);
PER_CORE_SENSOR_DEVICE_ATTR(freq, 10);
PER_CORE_SENSOR_DEVICE_ATTR(freq, 11);
PER_CORE_SENSOR_DEVICE_ATTR(freq, 12);
PER_CORE_SENSOR_DEVICE_ATTR(freq, 13);
PER_CORE_SENSOR_DEVICE_ATTR(freq, 14);
PER_CORE_SENSOR_DEVICE_ATTR(freq, 15);
PER_CORE_SENSOR_DEVICE_ATTR(freq, 16);

static struct attribute *amdgpu_metrics_per_core_attributes[] = {
	REF_PRE_CORE_SENSOR_DEVICE_ATTR(freq, 1),
	REF_PRE_CORE_SENSOR_DEVICE_ATTR(freq, 2),
	REF_PRE_CORE_SENSOR_DEVICE_ATTR(freq, 3),
	REF_PRE_CORE_SENSOR_DEVICE_ATTR(freq, 4),
	REF_PRE_CORE_SENSOR_DEVICE_ATTR(freq, 5),
	REF_PRE_CORE_SENSOR_DEVICE_ATTR(freq, 6),
	REF_PRE_CORE_SENSOR_DEVICE_ATTR(freq, 7),
	REF_PRE_CORE_SENSOR_DEVICE_ATTR(freq, 8),
	REF_PRE_CORE_SENSOR_DEVICE_ATTR(freq, 9),
	REF_PRE_CORE_SENSOR_DEVICE_ATTR(freq, 10),
	REF_PRE_CORE_SENSOR_DEVICE_ATTR(freq, 11),
	REF_PRE_CORE_SENSOR_DEVICE_ATTR(freq, 12),
	REF_PRE_CORE_SENSOR_DEVICE_ATTR(freq, 13),
	REF_PRE_CORE_SENSOR_DEVICE_ATTR(freq, 14),
	REF_PRE_CORE_SENSOR_DEVICE_ATTR(freq, 15),
	REF_PRE_CORE_SENSOR_DEVICE_ATTR(freq, 16),
	NULL
};

static const struct attribute_group amdgpu_metrics_per_core_attrgroup = {
	.attrs = amdgpu_metrics_per_core_attributes,
	.is_visible = amdgpu_metrics_hwmon_visible_shim,
};

static const struct attribute_group *amdgpu_metrics_per_core_attrgroups[] = {
	&amdgpu_metrics_per_core_attrgroup,
	NULL
};

/*
 * Labels are intentional not exported, in order that HTOP can correctly
 * display core temperature on multi cluster (e.g., big.LITTLE) CPUs.
 */
static const struct hwmon_channel_info *const amdgpu_metrics_per_core_info[] = {
	HWMON_CHANNEL_INFO(temp, REPEAT_NCORES(HWMON_T_INPUT)),
	HWMON_CHANNEL_INFO(power, REPEAT_NCORES(HWMON_P_INPUT)),
	NULL
};

static const struct hwmon_ops amdgpu_metrics_per_core_ops = {
	.is_visible = amdgpu_metrics_per_core_is_visible,
	.read = amdgpu_metrics_per_core_read,
};

static const struct hwmon_chip_info amdgpu_metrics_per_core_chip_info = {
	.ops = &amdgpu_metrics_per_core_ops,
	.info = amdgpu_metrics_per_core_info,
};

/*
 * An HWMON device must be registered with a parent.
 * We have no choice but to create a dummy one.
 */
static struct class *amdgpu_metrics_class;
static struct device *amdgpu_metrics_device;

static int __init amdgpu_metrics_init_priv(struct amdgpu_metrics_private *priv,
					   bool separate_per_core)
{
	int err, i, core = 0;

	err = amdgpu_metrics_init_priv_common(&priv->common);
	if (err)
		return err;

	if (!separate_per_core || !priv->common.has_per_core)
		return 0;

	/* Separate per-core channels into a dedicated HWMON device. */
	for (i = 0; i < NCORES; i++) {
		if (!priv->common.remap.temp.core[i].valid &&
		    !priv->common.remap.power.core[i].valid &&
		    !priv->common.remap.freq.coreclk[i].valid)
			continue;

		priv->common.remap.temp.core[i].ext = true;
		priv->common.remap.power.core[i].ext = true;
		priv->common.remap.freq.coreclk[i].ext = true;

		priv->per_core_channel_remap[core++] = (remap_t) {
			.valid = true,
			.idx = i,
		};
	}

	while (core < NCORES)
		priv->per_core_channel_remap[core++] = (remap_t) { .valid = false };

	return 0;
}

static int __init amdgpu_metrics_register_path(const char *path)
{
	bool separate_per_core = per_core_hwmon_name[0] != '\0';
	struct amdgpu_metrics_private *priv;
	struct device *dev;
	ssize_t size;
	int err;

	priv = devm_kzalloc(amdgpu_metrics_device, sizeof(*priv), GFP_KERNEL);
	if (priv == NULL)
		return -ENOMEM;

	size = amdgpu_metrics_read_gpu_metrics(path,
					       &priv->common.metrics.header,
					       sizeof(priv->common.metrics));
	if (size < 0) {
		err = size;
		goto out_free;
	}

	err = amdgpu_metrics_init_priv(priv, separate_per_core);
	if (err)
		goto out_free;

	priv->path = path;
	init_rwsem(&priv->metrics_lock);

	dev = devm_hwmon_device_register_with_info(amdgpu_metrics_device, MODULE_NAME,
						   priv, &amdgpu_metrics_hwmon_chip_info,
						   amdgpu_metrics_hwmon_attrgroups);
	err = PTR_ERR_OR_ZERO(dev);
	if (err)
		goto out_register_fail;

	if (!separate_per_core || !priv->common.has_per_core)
		return 0;

	dev = devm_hwmon_device_register_with_info(amdgpu_metrics_device, per_core_hwmon_name,
						   priv, &amdgpu_metrics_per_core_chip_info,
						   amdgpu_metrics_per_core_attrgroups);
	err = PTR_ERR_OR_ZERO(dev);
	if (err)
		goto out_register_fail;

	return 0;

out_register_fail:
	pr_err("Failed to register HWMON device: %d\n", err);
	goto out_free;

out_free:
	devm_kfree(amdgpu_metrics_device, priv);
	return err;
}

static int __init amdgpu_metrics_init(void)
{
	int err;

	if (gpu_metrics_path[0] == '\0') {
		pr_err("Invalid gpu_metrics path\n");
		return -EINVAL;
	}

	amdgpu_metrics_class = class_create(MODULE_NAME);
	err = PTR_ERR_OR_ZERO(amdgpu_metrics_class);
	if (err) {
		pr_err("Failed to create amdgpu_metrics class\n");
		goto out;
	}

	amdgpu_metrics_device = device_create(amdgpu_metrics_class, NULL, MKDEV(0, 0),
					      NULL, MODULE_NAME);
	err = PTR_ERR_OR_ZERO(amdgpu_metrics_device);
	if (err) {
		pr_err("Failed to create amdgpu_metrics device\n");
		goto out_class;
	}

	err = amdgpu_metrics_register_path(gpu_metrics_path);
	if (err) {
		pr_err("Failed to register gpu_metrics path: %s\n", gpu_metrics_path);
		goto out_device;
	}

	return 0;

out_device:
	device_destroy(amdgpu_metrics_class, MKDEV(0, 0));
out_class:
	class_destroy(amdgpu_metrics_class);
out:
	return err;
}

static void __exit amdgpu_metrics_exit(void) {
	if (!PTR_ERR_OR_ZERO(amdgpu_metrics_device))
		device_destroy(amdgpu_metrics_class, MKDEV(0, 0));
	if (!PTR_ERR_OR_ZERO(amdgpu_metrics_class))
		class_destroy(amdgpu_metrics_class);
}

module_init(amdgpu_metrics_init);
module_exit(amdgpu_metrics_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rongrong <i@rong.moe>");
MODULE_DESCRIPTION("HWMON driver for AMDGPU gpu_metrics");
