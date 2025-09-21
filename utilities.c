/*
 * utilities test utility for amdgpu_metrics
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

#define _GNU_SOURCE

#include <assert.h>
#include <glob.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "amdgpu_metrics.h"
#include "dumper/dump_gpu_metrics.h"

#define BUF_SIZE 1024

static const char gpu_metrics_glob[] = "/sys/class/drm/render*/device/gpu_metrics";

static int read_gpu_metrics(const char *path, struct metrics_table_header *metrics, size_t size)
{
	FILE *file = fopen(path, "rb");
	if (file == NULL) {
		pr_err("Failed to open %s: %s\n", path, strerror(errno));
		return -errno;
	}

	size_t bytes_read = fread(metrics, 1, size, file);
	if (ferror(file)) {
		pr_err("Failed to read %s: %s\n", path, strerror(errno));
		fclose(file);
		return 1;
	}

	assert(feof(file));
	fclose(file);
	assert(bytes_read >= sizeof(*metrics));
	assert(bytes_read == metrics->structure_size);
	return 0;
}

#define SHOW_CHANNELS(_priv_p, _idx_max, _channels_group, _labels, _get_val)	\
do {										\
	printf("| ========= [ %-18s |     ] ========= |\n", #_channels_group);	\
	for (unsigned int i = 0; i < _idx_max; i++) {				\
		const char *label;						\
		uint64_t val;							\
		int err;							\
		if (!(_priv_p)->remap._channels_group.data[i].valid)		\
			continue;						\
		label = _labels[(_priv_p)->remap._channels_group.data[i].idx];	\
		err = _get_val((_priv_p), i, &val);				\
		if (err)							\
			continue;						\
		printf("| %-30s | %15lu |\n", label, val);			\
	}									\
} while (0)

static int test_path(const char *path)
{
	struct amdgpu_metrics_private_common priv = { 0 };
	int err;

	pr_info("Testing against '%s'\n", path);

	err = read_gpu_metrics(path, &priv.metrics.header, sizeof(priv.metrics));
	if (err)
		return err;

	err = amdgpu_metrics_init_priv_common(&priv);
	if (err)
		return err;

	printf("|              Name              |      Value      |\n"
	       "|--------------------------------|-----------------|\n");

	SHOW_CHANNELS(&priv, NCHANNELS_TEMP, temp, amdgpu_metrics_labels_temp, GET_TEMP);
	SHOW_CHANNELS(&priv, NCHANNELS_POWER, power, amdgpu_metrics_labels_power, GET_POWER);
	SHOW_CHANNELS(&priv, NCHANNELS_FREQ, freq, amdgpu_metrics_labels_freq, GET_FREQ);

	return err;
}

static int dump_path(const char *path)
{
	union gpu_metrics metrics = { 0 };
	int err;

	pr_info("Dumping '%s'\n", path);

	err = read_gpu_metrics(path, &metrics.header, sizeof(metrics));
	if (err)
		return err;

	err = dump_gpu_metrics(&metrics);
	if (err) {
		pr_err("Failed to dump '%s': v%u.%u, size=%uB", path,
		       (unsigned int)metrics.header.format_revision,
		       (unsigned int)metrics.header.content_revision,
		       (unsigned int)metrics.header.structure_size);
		return err;
	}

	return 0;
}

static int for_all_gpu_metrics(int (*callback)(const char *), bool fail_fast)
{
	glob_t globbuf;
	int err = 0;

	if ((err = glob(gpu_metrics_glob, 0, NULL, &globbuf))) {
		if (err == GLOB_NOMATCH) {
			pr_warn("No gpu_metrics is exported. Did you install an AMD GPU/APU?\n");
			pr_info("Hint: glob path: '%s', error: %d\n", gpu_metrics_glob, err);
			err = 0;
		} else {
			pr_err("Failed to glob '%s': %d", gpu_metrics_glob, err);
		}
		goto out;
	}

	for (size_t i = 0; i < globbuf.gl_pathc; i++) {
		err = callback(globbuf.gl_pathv[i]) || err;
		if (err && fail_fast)
			goto out;
	}

out:
	globfree(&globbuf);
	return err;
}

int main(int argc, char *argv[])
{
	int i, opt, err = 0;
	bool test = false, dump = false, fail_fast = false;

	while ((opt = getopt(argc, argv, "tdfh")) != -1) {
		switch (opt)
		{
		case 't':
			test = true;
			break;
		case 'd':
			dump = true;
			break;
		case 'f':
			fail_fast = true;
			break;
		case 'h':
		default:
			fprintf(stderr,
				"Usage: %s [-t] [-d] [-f] FILE...\n\n"
				"  -t\tTest against the specified files (default)\n"
				"  -d\tDump everything from the specified files\n"
				"  -f\tFail fast\n",
				argv[0]);
			return 1;
		}
	}

	if (!test && !dump)
		test = true;

	if (optind >= argc) {
		if (test)
			err = for_all_gpu_metrics(test_path, fail_fast);

		if (dump && !(err && fail_fast))
			err = for_all_gpu_metrics(dump_path, fail_fast);

		goto out;
	}

	if (test) {
		for (i = optind; i < argc; i++) {
			err = test_path(argv[i]) || err;
			if (err && fail_fast)
				goto out;
		}
	}

	if (dump) {
		for (i = optind; i < argc; i++) {
			err = dump_path(argv[i]) || err;
			if (err && fail_fast)
				goto out;
		}
	}
out:
	if (err)
		pr_err("Error(s) occurred. Please check.\n");
	return err;
}
