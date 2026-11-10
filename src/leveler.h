/* Loudness leveler (auto gain toward a LUFS target) + lookahead limiter.
 * Pure C, no OBS dependency, so it can be unit-tested standalone. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "loudness.h"

enum { LV_WINDOW_MOMENTARY = 0, LV_WINDOW_SHORT_TERM = 1 };

typedef struct {
	/* auto gain */
	bool agc_enabled;
	double target_lufs;  /* e.g. -16 */
	int window;          /* LV_WINDOW_* */
	double max_boost_db; /* max gain increase, e.g. 12 */
	double max_cut_db;   /* max gain reduction, e.g. 18 */
	double gate_lufs;    /* below this, gain is frozen (avoids boosting noise) */
	double tolerance_lu; /* dead-band: no change while within +/- this */
	double attack_ms;    /* how fast gain goes DOWN */
	double release_ms;   /* how fast gain goes UP */
	/* limiter */
	bool limiter_enabled;
	double ceiling_db;   /* e.g. -1.0 */
	double lookahead_ms; /* 1..20 */
	double limiter_release_ms;
} leveler_params_t;

typedef struct leveler leveler_t;

void leveler_default_params(leveler_params_t *p);
leveler_t *leveler_create(int channels, uint32_t sample_rate);
void leveler_destroy(leveler_t *lv);
void leveler_set_params(leveler_t *lv, const leveler_params_t *p);
/* In-place processing of planar float audio. */
void leveler_process(leveler_t *lv, float *const *data, size_t frames);

/* Metering (valid after process) */
double leveler_input_lufs(const leveler_t *lv);
double leveler_output_lufs(const leveler_t *lv); /* after gain + limiter */
double leveler_gain_db(const leveler_t *lv);
double leveler_limiter_gr_db(const leveler_t *lv); /* <= 0 */
