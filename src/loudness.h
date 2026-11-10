/* ITU-R BS.1770 / EBU R128 loudness meter (momentary 400 ms, short-term 3 s).
 * Pure C, no OBS dependency. */
#pragma once
#include <stddef.h>

#define LM_MAX_CH 8
#define LM_RING 30 /* 30 x 100 ms blocks = 3 s */

typedef struct {
	double b0, b1, b2, a1, a2, z1, z2;
} lm_biquad_t;

typedef struct {
	int channels;
	double sample_rate;
	size_t block_len, block_pos;
	lm_biquad_t shelf[LM_MAX_CH], hpf[LM_MAX_CH];
	double sumsq[LM_MAX_CH];
	double weight[LM_MAX_CH];
	double ring[LM_RING]; /* mean-square energy per 100 ms block */
	int ring_pos, ring_count;
} lufs_meter_t;

void lufs_meter_init(lufs_meter_t *m, int channels, double sample_rate);
/* data: planar float channels */
void lufs_meter_process(lufs_meter_t *m, float *const *data, size_t frames);
/* Return LUFS, or -200 when there is no data / pure silence. */
double lufs_meter_momentary(const lufs_meter_t *m);
double lufs_meter_short_term(const lufs_meter_t *m);
