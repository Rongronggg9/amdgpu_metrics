/*
 * Common code for amdgpu_metrics
 *
 * This is not a common header file, but some common code shared
 * by both kernel module and utilities utility.
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

#ifndef AMDGPU_METRICS_H
#define AMDGPU_METRICS_H

#ifdef __KERNEL__

#include <linux/compiler.h>
#include <linux/errname.h>
#include <linux/limits.h>
#include <linux/kernel.h>

#else /* !__KERNEL__ */

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define u8 uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define u64 uint64_t
#define U8_MAX UINT8_MAX
#define U16_MAX UINT16_MAX
#define U32_MAX UINT32_MAX
#define U64_MAX UINT64_MAX

#define __init
#define __exit

#ifndef likely
# define likely(x)	__builtin_expect(!!(x), 1)
#endif

#ifndef unlikely
# define unlikely(x)	__builtin_expect(!!(x), 0)
#endif

#ifndef unreachable
# define unreachable()	__builtin_unreachable()
#endif

#define ARRAY_SIZE(x)	(sizeof(x) / sizeof((x)[0]))

#define printk(fmt, ...)	fprintf(stderr, fmt, ##__VA_ARGS__)
#define pr_debug(fmt, ...)	printk("DEBUG:   " fmt, ##__VA_ARGS__)
#define pr_info(fmt, ...)	printk("INFO:    " fmt, ##__VA_ARGS__)
#define pr_notice(fmt, ...)	printk("NOTICE:  " fmt, ##__VA_ARGS__)
#define pr_warn(fmt, ...)	printk("WARNING: " fmt, ##__VA_ARGS__)
#define pr_err(fmt, ...)	printk("ERROR:   " fmt, ##__VA_ARGS__)

#define WARN_ON(x) ({									\
	int cond = !!(x);								\
	if (unlikely(cond))								\
		fprintf(stderr, "WARN_ON(%s) at %s:%d\n", #x, __FILE__, __LINE__);	\
	unlikely(cond);									\
})

#ifdef __USE_GNU
# define errname(err) strerrorname_np(-err)
#else /* !__USE_GNU */
/*
 * SONIC SAYS...
 * If your libc isn't glibc,
 * ...
 */
# define errname(err) strerror(-err)
#endif

#endif /* __KERNEL__ */

#include "vendor/kgd_pp_interface.h"

/*
 * _Static_assert() can't be used in all scopes.
 * Turn it into an expression that returns 0 (see static_assert(3)).
 */
#define must_be(e, msg) \
	(0 * (int)sizeof(struct { _Static_assert(e, msg); char dummy; }))

#define NHBM 4
#define NCORES 16
#define NL3 2

#define REPEAT_4(x)	x, x, x, x
#define REPEAT_8(x)	REPEAT_4(x), REPEAT_4(x)
#define REPEAT_16(x)	REPEAT_8(x), REPEAT_8(x)
#define REPEAT_32(x)	REPEAT_16(x), REPEAT_16(x)

#define REPEAT_NCORES(x)	REPEAT_16(x)

static const char *amdgpu_metrics_labels_temp[] = {
	"Edge", "Hotspot", "Mem",
	"VRGFX", "VRSoC", "VRMem",
	"HBM 0", "HBM 1", "HBM 2", "HBM 3",
	"GFX", "SoC",
	"Core 0", "Core 1", "Core 2", "Core 3",
	"Core 4", "Core 5", "Core 6", "Core 7",
	"Core 8", "Core 9", "Core 10", "Core 11",
	"Core 12", "Core 13", "Core 14", "Core 15",
	"L3 0", "L3 1",
	"Skin",
};
#define NCHANNELS_TEMP (ARRAY_SIZE(amdgpu_metrics_labels_temp)) /* 31 */
#define REPEAT_NCHANNELS_TEMP(x) REPEAT_16(x), REPEAT_8(x), REPEAT_4(x), x, x, x

#define DEF_CHANNELS_TEMP(_t)		\
union {					\
	struct {			\
		_t edge;		\
		_t hotspot;		\
		_t mem;			\
		_t vrgfx;		\
		_t vrsoc;		\
		_t vrmem;		\
		_t hbm[NHBM];		\
		_t gfx;			\
		_t soc;			\
		_t core[NCORES];	\
		_t l3[NL3];		\
		_t skin;		\
	};				\
	_t data[NCHANNELS_TEMP];	\
}

static const char *amdgpu_metrics_labels_power[] = {
	"Socket", "CPU", "SoC",	"GFX",
	"Core 0", "Core 1", "Core 2", "Core 3",
	"Core 4", "Core 5", "Core 6", "Core 7",
	"Core 8", "Core 9", "Core 10", "Core 11",
	"Core 12", "Core 13", "Core 14", "Core 15",
	"IPU", "APU", "dGPU", "Sys",
};
#define NCHANNELS_POWER (ARRAY_SIZE(amdgpu_metrics_labels_power)) /* 24 */
#define REPEAT_NCHANNELS_POWER(x) REPEAT_16(x), REPEAT_8(x)

#define DEF_CHANNELS_POWER(_t)		\
union {					\
	struct {			\
		_t socket;		\
		_t cpu;			\
		_t soc;			\
		_t gfx;			\
		_t core[NCORES];	\
		_t ipu;			\
		_t apu;			\
		_t dgpu;		\
		_t sys;			\
	};				\
	_t data[NCHANNELS_POWER];	\
}

#define NGFXCLK 8
#define NSOCCLK 4
#define NVCLK 4
#define NDCLK 4

static const char *amdgpu_metrics_labels_freq[] = {
	"GFXCLK 0", "GFXCLK 1", "GFXCLK 2", "GFXCLK 3",
	"GFXCLK 4", "GFXCLK 5", "GFXCLK 6", "GFXCLK 7",
	"SoCCLK 0", "SoCCLK 1", "SoCCLK 2", "SoCCLK 3",
	"UCLK",
	"VCLK 0", "VCLK 1", "VCLK 2", "VCLK 3",
	"DCLK 0", "DCLK 1", "DCLK 2", "DCLK 3",
	"FCLK",
	"CoreCLK 0", "CoreCLK 1", "CoreCLK 2", "CoreCLK 3",
	"CoreCLK 4", "CoreCLK 5", "CoreCLK 6", "CoreCLK 7",
	"CoreCLK 8", "CoreCLK 9", "CoreCLK 10", "CoreCLK 11",
	"CoreCLK 12", "CoreCLK 13", "CoreCLK 14", "CoreCLK 15",
	"L3CLK 0", "L3CLK 1",
	"VPECLK", "IPUCLK", "MPIPUCLK",
};
#define NCHANNELS_FREQ (ARRAY_SIZE(amdgpu_metrics_labels_freq)) /* 43 */
#define REPEAT_NCHANNELS_FREQ(x) REPEAT_32(x), REPEAT_8(x), x, x, x

#define DEF_CHANNELS_FREQ(_t)		\
union {					\
	struct {			\
		_t gfxclk[NGFXCLK];	\
		_t socclk[NSOCCLK];	\
		_t uclk;		\
		_t vclk[NVCLK];		\
		_t dclk[NDCLK];		\
		_t fclk;		\
		_t coreclk[NCORES];	\
		_t l3clk[NL3];		\
		_t vpeclk;		\
		_t ipuclk;		\
		_t mpipuclk;		\
	};				\
	_t data[NCHANNELS_FREQ];	\
}

enum channel_data_type {
	channel_null,
	channel_u8,
	channel_u16,
	channel_u32,
	channel_u64,
	channel_invalid, /* 5 */
};

#define data_type_enum_to_size(e)	\
(					\
	(e) == channel_u8  ? 1 :	\
	(e) == channel_u16 ? 2 :	\
	(e) == channel_u32 ? 4 :	\
	(e) == channel_u64 ? 8 :	\
	(WARN_ON(1), 0)			\
)

#define to_data_type_enum(e)							\
(										\
	__builtin_types_compatible_p(typeof(e), uint8_t)  ? channel_u8  :	\
	__builtin_types_compatible_p(typeof(e), uint16_t) ? channel_u16 :	\
	__builtin_types_compatible_p(typeof(e), uint32_t) ? channel_u32 :	\
	__builtin_types_compatible_p(typeof(e), uint64_t) ? channel_u64 :	\
	channel_invalid								\
)

typedef struct {
	/* sizeof(struct gpu_metrics_v1_8) == 3872 */
	uint16_t offset : 13; /* max 8191 */
	/*
	 * The underlying type of enum is int,
	 * resulting in sizeof(channel_t) == 4.
	 */
	uint8_t type : 3; /* max 7 */
	struct {
		uint16_t offset : 13;
		uint8_t type : 3;
	} fb; /* Fallback, for current_* and average_* */
} channel_t; /* sizeof(channel_t) == 2 */

#define is_channel_valid(e) (channel_null < (e).type && (e).type < channel_invalid)

struct amdgpu_metrics_def {
	uint16_t metrics_size;
	DEF_CHANNELS_TEMP(channel_t) temp;
	DEF_CHANNELS_POWER(channel_t) power;
	DEF_CHANNELS_FREQ(channel_t) freq;
};

typedef struct {
	bool valid : 1;
	bool ext : 1;
	uint8_t idx : 6;
} remap_t;

struct amdgpu_metrics_labels_remap {
	DEF_CHANNELS_TEMP(remap_t) temp;
	DEF_CHANNELS_POWER(remap_t) power;
	DEF_CHANNELS_FREQ(remap_t) freq;
};

#define _mbr_to_data_type_enum(_t, _mbr) \
	to_data_type_enum(((_t *)0)->_mbr)

#define mbr_to_data_type_enum(_t, _mbr)					\
(									\
	_mbr_to_data_type_enum(_t, _mbr) +				\
	must_be(_mbr_to_data_type_enum(_t, _mbr) != channel_invalid,	\
		"Unsupported data type")				\
)

#define _DEF_CHANNEL(_channel, _t, _mbr)				\
	._channel.offset = offsetof(_t, _mbr),				\
	._channel.type   = mbr_to_data_type_enum(_t, _mbr)

#define _DEF_CHANNEL_FB(_channel, _t, _mbr, _fb_mbr)			\
	_DEF_CHANNEL(_channel, _t, _mbr),				\
	._channel.fb.offset = offsetof(_t, _fb_mbr),			\
	._channel.fb.type   = mbr_to_data_type_enum(_t, _fb_mbr)

#define DEF_CHANNEL(_v, _channel, _mbr) \
	_DEF_CHANNEL(_channel, struct gpu_metrics_##_v, _mbr)

#define DEF_CHANNEL_FB(_v, _channel, _mbr, _fb_mbr) \
	_DEF_CHANNEL_FB(_channel, struct gpu_metrics_##_v, _mbr, _fb_mbr)

#define DEF_CHANNEL_ARR_ELEM(_v, _channel, _mbr, _n) \
	DEF_CHANNEL(_v, _channel[_n], _mbr[_n])

#define DEF_CHANNEL_ARR2(_v, _channel, _mbr, _off)		\
	DEF_CHANNEL_ARR_ELEM(_v, _channel, _mbr, 0 + (_off)),	\
	DEF_CHANNEL_ARR_ELEM(_v, _channel, _mbr, 1 + (_off))

#define DEF_CHANNEL_ARR4(_v, _channel, _mbr, _off)		\
	DEF_CHANNEL_ARR2(_v, _channel, _mbr, 0 + (_off)),	\
	DEF_CHANNEL_ARR2(_v, _channel, _mbr, 2 + (_off))

#define DEF_CHANNEL_ARR8(_v, _channel, _mbr, _off)		\
	DEF_CHANNEL_ARR4(_v, _channel, _mbr, 0 + (_off)),	\
	DEF_CHANNEL_ARR4(_v, _channel, _mbr, 4 + (_off))

#define DEF_CHANNEL_ARR16(_v, _channel, _mbr, _off)		\
	DEF_CHANNEL_ARR8(_v, _channel, _mbr, 0 + (_off)),	\
	DEF_CHANNEL_ARR8(_v, _channel, _mbr, 8 + (_off))

#define DEF_CHANNELS(_v, _temp, _power, _freq)				\
	{								\
		.metrics_size = sizeof(struct gpu_metrics_##_v),	\
		.temp = { _temp(_v), },					\
		.power = { _power(_v), },				\
		.freq = { _freq(_v), },					\
	}

#define DEF_CHANNEL_TEMP_V1_COMMON1(_v)			\
	DEF_CHANNEL(_v, hotspot, temperature_hotspot),	\
	DEF_CHANNEL(_v, mem, temperature_mem),		\
	DEF_CHANNEL(_v, vrsoc, temperature_vrsoc)

#define DEF_CHANNEL_TEMP_V1_COMMON2(_v)			\
	DEF_CHANNEL(_v, edge, temperature_edge),	\
	DEF_CHANNEL(_v, vrgfx, temperature_vrgfx),	\
	DEF_CHANNEL(_v, vrmem, temperature_vrmem)

#define DEF_CHANNEL_TEMP_V1_0(_v)		\
	DEF_CHANNEL_TEMP_V1_COMMON1(_v),	\
	DEF_CHANNEL_TEMP_V1_COMMON2(_v)

#define DEF_CHANNEL_POWER_V1_0(_v) \
	DEF_CHANNEL(_v, socket, average_socket_power)

#define DEF_CHANNEL_FREQ_V1_0(_v)							\
	DEF_CHANNEL_FB(_v, gfxclk[0], current_gfxclk, average_gfxclk_frequency),	\
	DEF_CHANNEL_FB(_v, socclk[0], current_socclk, average_socclk_frequency),	\
	DEF_CHANNEL_FB(_v, uclk, current_uclk, average_uclk_frequency),			\
	DEF_CHANNEL_FB(_v, vclk[0], current_vclk0, average_vclk0_frequency),		\
	DEF_CHANNEL_FB(_v, vclk[1], current_vclk1, average_dclk0_frequency),		\
	DEF_CHANNEL_FB(_v, dclk[0], current_dclk0, average_vclk1_frequency),		\
	DEF_CHANNEL_FB(_v, dclk[1], current_dclk1, average_dclk1_frequency)

#define DEF_CHANNELS_V1_0(_v)				\
	DEF_CHANNELS(_v, DEF_CHANNEL_TEMP_V1_0,		\
			 DEF_CHANNEL_POWER_V1_0,	\
			 DEF_CHANNEL_FREQ_V1_0)

#define DEF_CHANNEL_TEMP_V1_1(_v)				\
	DEF_CHANNEL_TEMP_V1_0(_v),				\
	DEF_CHANNEL_ARR4(_v, hbm, temperature_hbm, 0)

#define DEF_CHANNELS_V1_1(_v)				\
	DEF_CHANNELS(_v, DEF_CHANNEL_TEMP_V1_1,		\
			 DEF_CHANNEL_POWER_V1_0,	\
			 DEF_CHANNEL_FREQ_V1_0)

#define DEF_CHANNEL_POWER_V1_4(_v) \
	DEF_CHANNEL(_v, socket, curr_socket_power)

#define DEF_CHANNEL_FREQ_V1_4(_v)				\
	DEF_CHANNEL_ARR8(_v, gfxclk, current_gfxclk, 0),	\
	DEF_CHANNEL_ARR4(_v, socclk, current_socclk, 0),	\
	DEF_CHANNEL(_v, uclk, current_uclk),			\
	DEF_CHANNEL_ARR4(_v, vclk, current_vclk0, 0),		\
	DEF_CHANNEL_ARR4(_v, dclk, current_dclk0, 0)

#define DEF_CHANNELS_V1_4(_v)				\
	DEF_CHANNELS(_v, DEF_CHANNEL_TEMP_V1_COMMON1,	\
			 DEF_CHANNEL_POWER_V1_4,	\
			 DEF_CHANNEL_FREQ_V1_4)

#define DEF_CHANNEL_TEMP_V2(_v)					\
	DEF_CHANNEL(_v, gfx, temperature_gfx),			\
	DEF_CHANNEL(_v, soc, temperature_soc),			\
	DEF_CHANNEL_ARR8(_v, core, temperature_core, 0),	\
	DEF_CHANNEL_ARR2(_v, l3, temperature_l3, 0)

#define DEF_CHANNEL_POWER_V2(_v)				\
	DEF_CHANNEL(_v, socket, average_socket_power),		\
	DEF_CHANNEL(_v, cpu, average_cpu_power),		\
	DEF_CHANNEL(_v, soc, average_soc_power),		\
	DEF_CHANNEL(_v, gfx, average_gfx_power),		\
	DEF_CHANNEL_ARR8(_v, core, average_core_power, 0)

#define DEF_CHANNEL_FREQ_V2(_v)								\
	DEF_CHANNEL_FB(_v, gfxclk[0], current_gfxclk, average_gfxclk_frequency),	\
	DEF_CHANNEL_FB(_v, socclk[0], current_socclk, average_socclk_frequency),	\
	DEF_CHANNEL_FB(_v, uclk, current_uclk, average_uclk_frequency),			\
	DEF_CHANNEL_FB(_v, fclk, current_fclk, average_fclk_frequency),			\
	DEF_CHANNEL_FB(_v, vclk[0], current_vclk, average_vclk_frequency),		\
	DEF_CHANNEL_FB(_v, dclk[0], current_dclk, average_dclk_frequency),		\
	DEF_CHANNEL_ARR8(_v, coreclk, current_coreclk, 0),				\
	DEF_CHANNEL_ARR2(_v, l3clk, current_l3clk, 0)

#define DEF_CHANNELS_V2_0(_v)			\
	DEF_CHANNELS(_v, DEF_CHANNEL_TEMP_V2,	\
			 DEF_CHANNEL_POWER_V2,	\
			 DEF_CHANNEL_FREQ_V2)

#define DEF_CHANNEL_TEMP_V3(_v)					\
	DEF_CHANNEL(_v, gfx, temperature_gfx),			\
	DEF_CHANNEL(_v, soc, temperature_soc),			\
	DEF_CHANNEL_ARR16(_v, core, temperature_core, 0),	\
	DEF_CHANNEL(_v, skin, temperature_skin)

#define DEF_CHANNEL_POWER_V3(_v)				\
	DEF_CHANNEL(_v, socket, average_socket_power),		\
	DEF_CHANNEL(_v, ipu, average_ipu_power),		\
	DEF_CHANNEL(_v, apu, average_apu_power),		\
	DEF_CHANNEL(_v, gfx, average_gfx_power),		\
	DEF_CHANNEL(_v, dgpu, average_dgpu_power),		\
	DEF_CHANNEL(_v, cpu, average_all_core_power),		\
	DEF_CHANNEL_ARR16(_v, core, average_core_power, 0),	\
	DEF_CHANNEL(_v, sys, average_sys_power)

#define DEF_CHANNEL_FREQ_V3(_v)					\
	DEF_CHANNEL(_v, gfxclk[0], average_gfxclk_frequency),	\
	DEF_CHANNEL(_v, socclk[0], average_socclk_frequency),	\
	DEF_CHANNEL(_v, vpeclk, average_vpeclk_frequency),	\
	DEF_CHANNEL(_v, ipuclk, average_ipuclk_frequency),	\
	DEF_CHANNEL(_v, fclk, average_fclk_frequency),		\
	DEF_CHANNEL(_v, vclk[0], average_vclk_frequency),	\
	DEF_CHANNEL(_v, uclk, average_uclk_frequency),		\
	DEF_CHANNEL_ARR16(_v, coreclk, current_coreclk, 0),	\
	DEF_CHANNEL(_v, mpipuclk, average_mpipu_frequency)

#define DEF_CHANNELS_V3_0(_v)			\
	DEF_CHANNELS(_v, DEF_CHANNEL_TEMP_V3,	\
			 DEF_CHANNEL_POWER_V3,	\
			 DEF_CHANNEL_FREQ_V3)

static const struct amdgpu_metrics_def amdgpu_metric_def_table_v1[] = {
	[0] = DEF_CHANNELS_V1_0(v1_0),
	[1] = DEF_CHANNELS_V1_1(v1_1),
	[2] = DEF_CHANNELS_V1_1(v1_2),
	[3] = DEF_CHANNELS_V1_1(v1_3),
	[4] = DEF_CHANNELS_V1_4(v1_4),
	[5] = DEF_CHANNELS_V1_4(v1_5),
	[6] = DEF_CHANNELS_V1_4(v1_6),
	[7] = DEF_CHANNELS_V1_4(v1_7),
	[8] = DEF_CHANNELS_V1_4(v1_8),
};

static const struct amdgpu_metrics_def amdgpu_metric_def_table_v2[] = {
	[0] = DEF_CHANNELS_V2_0(v2_0),
	[1] = DEF_CHANNELS_V2_0(v2_1),
	[2] = DEF_CHANNELS_V2_0(v2_2),
	[3] = DEF_CHANNELS_V2_0(v2_3),
	[4] = DEF_CHANNELS_V2_0(v2_4),
};

static const struct amdgpu_metrics_def amdgpu_metric_def_table_v3[] = {
	[0] = DEF_CHANNELS_V3_0(v3_0),
};

static int amdgpu_metrics_get_channels(unsigned int fr, unsigned int cr,
				       const struct amdgpu_metrics_def **channels)
{
	switch (fr) {
	case 1:
		if (cr < ARRAY_SIZE(amdgpu_metric_def_table_v1)) {
			*channels = &amdgpu_metric_def_table_v1[cr];
			return 0;
		}
		break;
	case 2:
		if (cr < ARRAY_SIZE(amdgpu_metric_def_table_v2)) {
			*channels = &amdgpu_metric_def_table_v2[cr];
			return 0;
		}
		break;
	case 3:
		if (cr < ARRAY_SIZE(amdgpu_metric_def_table_v3)) {
			*channels = &amdgpu_metric_def_table_v3[cr];
			return 0;
		}
		break;
	}

	return -ENOSYS;
}

union gpu_metrics {
	struct metrics_table_header header;
	struct gpu_metrics_v1_0 v1_0;
	struct gpu_metrics_v1_1 v1_1;
	struct gpu_metrics_v1_2 v1_2;
	struct gpu_metrics_v1_3 v1_3;
	struct gpu_metrics_v1_4 v1_4;
	struct gpu_metrics_v1_5 v1_5;
	struct gpu_metrics_v1_6 v1_6;
	struct gpu_metrics_v1_7 v1_7;
	struct gpu_metrics_v1_8 v1_8;
	struct gpu_metrics_v2_0 v2_0;
	struct gpu_metrics_v2_1 v2_1;
	struct gpu_metrics_v2_2 v2_2;
	struct gpu_metrics_v2_3 v2_3;
	struct gpu_metrics_v2_4 v2_4;
	struct gpu_metrics_v3_0 v3_0;
};

struct amdgpu_metrics_private_common {
	const struct amdgpu_metrics_def *channels;
	struct amdgpu_metrics_labels_remap remap;
	bool has_per_core;

	union gpu_metrics metrics;
};

/* All gpu_metrics_v*_* members are unsigned. */
static int amdgpu_metrics_get_val(const struct amdgpu_metrics_private_common *priv,
				  channel_t channel, uint64_t *val)
{
	uint16_t offset = channel.offset;
	uint8_t type = channel.type;
	bool retrying = false;
	uint64_t raw, max;
	void *p;

	if (WARN_ON(!val))
		return -EINVAL;

	while (1) {
		if (WARN_ON(offset + data_type_enum_to_size(type) > priv->channels->metrics_size))
			return -EINVAL;

		p = (void *)&priv->metrics + offset;

		switch (type) {
		case channel_u8:
			raw = *(uint8_t *)p;
			max = U8_MAX;
			break;
		case channel_u16:
			raw = *(uint16_t *)p;
			max = U16_MAX;
			break;
		case channel_u32:
			raw = *(uint32_t *)p;
			max = U32_MAX;
			break;
		case channel_u64:
			raw = *(uint64_t *)p;
			max = U64_MAX;
			break;
		default:
			WARN_ON(1);
			return -EINVAL;
		}

		/* raw == U*_MAX means no such a HW block or measurement. */
		if (raw < max) {
			*val = raw;
			return 0;
		}

		if (retrying || channel.fb.type == channel_null)
			return -ENODEV;

		offset = channel.fb.offset;
		type = channel.fb.type;
		retrying = true;
	}
}

#define _GET_VAL(_priv_p, _idx, _idx_max, _channel_group, _channel, _val_p)	\
	_idx >= _idx_max ? -EINVAL : amdgpu_metrics_get_val			\
		((_priv_p),							\
		 (_priv_p)->channels->_channel_group._channel[_idx],		\
		 _val_p)

#define GET_TEMP(_priv_p, _idx, _val_p) \
	_GET_VAL(_priv_p, _idx, NCHANNELS_TEMP, temp, data, _val_p)

#define GET_CORE_TEMP(_priv_p, _idx, _val_p) \
	_GET_VAL(_priv_p, _idx, NCORES, temp, core, _val_p)

#define GET_POWER(_priv_p, _idx, _val_p) \
	_GET_VAL(_priv_p, _idx, NCHANNELS_POWER, power, data, _val_p)

#define GET_CORE_POWER(_priv_p, _idx, _val_p) \
	_GET_VAL(_priv_p, _idx, NCORES, power, core, _val_p)

#define GET_FREQ(_priv_p, _idx, _val_p) \
	_GET_VAL(_priv_p, _idx, NCHANNELS_FREQ, freq, data, _val_p)

#define GET_CORE_FREQ(_priv_p, _idx, _val_p) \
	_GET_VAL(_priv_p, _idx, NCORES, freq, coreclk, _val_p)

static int __init _amdgpu_metrics_validate_core(struct amdgpu_metrics_private_common *priv)
{
	bool core_functional;
	/* Core labels are consecutive. We can safely do self-increment later. */
	uint8_t temp_label_i = priv->remap.temp.core[0].idx;
	uint8_t power_label_i = priv->remap.power.core[0].idx;
	uint8_t freq_label_i = priv->remap.freq.coreclk[0].idx;
	uint8_t functional_cores = 0, dummy_cores = 0;
	unsigned int i;
	uint64_t power, freq;
	int err_power, err_freq;

	for (i = 0; i < NCORES; i++) {
		if (!priv->remap.temp.core[i].valid &&
		    !priv->remap.power.core[i].valid &&
		    !priv->remap.freq.coreclk[i].valid)
			continue;

		err_power = !priv->remap.power.core[i].valid || GET_CORE_POWER(priv, i, &power);
		err_freq = !priv->remap.freq.coreclk[i].valid || GET_CORE_FREQ(priv, i, &freq);

		/* Factory-disabled/dummy cores are power&clock gated (0W&0Hz). */
		core_functional = (
			(!err_power && power > 0) && (!err_freq && freq > 0)
		) || (
			(!err_power && power > 0) && err_freq
		) || (
			err_power && (!err_freq && freq > 0)
		) || (
			/* 0 in temperature channels has been handled before. */
			err_power && err_freq
		);

		if (core_functional) {
			/* This intentionally doesn't change the validity of channels. */
			priv->remap.temp.core[i].idx = temp_label_i++;
			priv->remap.power.core[i].idx = power_label_i++;
			priv->remap.freq.coreclk[i].idx = freq_label_i++;
			functional_cores++;
		} else {
			priv->remap.temp.core[i].valid = false;
			priv->remap.power.core[i].valid = false;
			priv->remap.freq.coreclk[i].valid = false;
			dummy_cores++;
		}
	}

	if (functional_cores || dummy_cores)
		/* Some SoC has */
		pr_debug("This APU has %hhu functional CPU cores and %hhu dummy cores\n",
			 functional_cores, dummy_cores);

	return functional_cores ? 0 : -ENODEV;
}

static void __init __amdgpu_metrics_validate_channels(
	struct amdgpu_metrics_private_common *priv, const char *channel_group,
	const char **labels, const channel_t *channels, remap_t *remaps,
	size_t size, bool zero_is_invalid)
{
	uint64_t val, i;
	bool valid;
	int err;

	for (i = 0; i < size; i++) {
		err = 0;
		val = 0;

		if (!is_channel_valid(channels[i])) {
			valid = false;
		} else if ((err = amdgpu_metrics_get_val(priv, channels[i], &val))) {
			pr_debug("'%s' (%s) unavailable: %s\n",
				 labels[i], channel_group, errname(err));
			valid = false;
		} else if (zero_is_invalid && val == 0) {
			pr_debug("'%s' (%s) unavailable: value is 0\n",
				 labels[i], channel_group);
			valid = false;
		} else {
			valid = true;
		}

		remaps[i] = (remap_t) {
			.valid = valid,
			.idx = i,
		};
	}
}

#define _amdgpu_metrics_validate_channels(_priv_p, _channel_group, _size, _zero_is_invalid)	\
	__amdgpu_metrics_validate_channels(							\
		_priv_p, #_channel_group, amdgpu_metrics_labels_##_channel_group,		\
		(_priv_p)->channels->_channel_group.data, (_priv_p)->remap._channel_group.data,	\
		_size, _zero_is_invalid)

static int __init amdgpu_metrics_init_priv_common(struct amdgpu_metrics_private_common *priv)
{
	int err;

	pr_info("gpu_metrics v%u.%u, size=%uB\n",
		(unsigned int)priv->metrics.header.format_revision,
		(unsigned int)priv->metrics.header.content_revision,
		(unsigned int)priv->metrics.header.structure_size);

	err = amdgpu_metrics_get_channels(priv->metrics.header.format_revision,
					  priv->metrics.header.content_revision,
					  &priv->channels);
	if (err) {
		pr_err("Unsupported gpu_metrics revision\n");
		return err;
	}

	/* Temperatures are (unfortunately) unsigned. Let's assume 0 implies ENODEV. */
	_amdgpu_metrics_validate_channels(priv, temp, NCHANNELS_TEMP, true);
	/* 0 in per-core channels implies ENODEV, otherwise it may be valid. */
	_amdgpu_metrics_validate_channels(priv, power, NCHANNELS_POWER, false);
	_amdgpu_metrics_validate_channels(priv, freq, NCHANNELS_FREQ, false);

	/* We handle 0 in per-core power/freq channels here. */
	err = _amdgpu_metrics_validate_core(priv);
	if (err == -ENODEV) {
		priv->has_per_core = false;
		pr_debug("Per-CPU-core channels unavailable\n");
	} else if (err) {
		return err;
	} else {
		priv->has_per_core = true;
	}

	return 0;
}

#endif /* AMDGPU_METRICS_H */
