#ifndef DUMP_GPU_METRICS_H
#define DUMP_GPU_METRICS_H

/*
 * kgd_pp_interface.h depends on some types from Linux headers.
 * Just use those from C standard library here.
 */
#include <stdbool.h>
#include <stdint.h>
#define u8 uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define u64 uint64_t

#include "../vendor/kgd_pp_interface.h"

int dump_gpu_metrics(const void *);

#endif /* DUMP_GPU_METRICS_H */
