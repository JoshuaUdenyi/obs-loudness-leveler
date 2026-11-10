#include "leveler.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LOOKAHEAD_MS 20.0
#define LN10_OVER_20 0.11512925464970229

struct leveler {
	leveler_params_t p;
	int ch;
	double fs;
	lufs_meter_t meter;     /* measures the input (drives the gain) */
	lufs_meter_t out_meter; /* measures the result (for the UI) */

	/* auto gain state */
	double gain_db, target_db, att, rel, in_lufs, out_lufs;

	/* limiter */
	double ceil_lin, lim_up;
	int Lmax, L;
	bool lim_prev;
	float *dl[LM_MAX_CH]; /* delay lines (L-1 samples) */
	int dpos;
	double *dq_val; /* monotonic deque for sliding minimum */
	uint64_t *dq_idx;
	int dq_cap, dq_head, dq_count;
	uint64_t n;
	double e;    /* release-limited envelope */
	double *box; /* moving-average ring */
	int box_pos;
	double box_sum;
	double gr_db;
};

void leveler_default_params(leveler_params_t *p)
{
	p->agc_enabled = true;
	p->target_lufs = -16.0;
	p->window = LV_WINDOW_MOMENTARY;
	p->max_boost_db = 12.0;
	p->max_cut_db = 18.0;
	p->gate_lufs = -50.0;
	p->tolerance_lu = 1.0;
	p->attack_ms = 150.0;
	p->release_ms = 1500.0;
	p->limiter_enabled = true;
	p->ceiling_db = -1.0;
	p->lookahead_ms = 5.0;
	p->limiter_release_ms = 80.0;
}

static void limiter_reset(leveler_t *lv)
{
	for (int c = 0; c < lv->ch; c++)
		memset(lv->dl[c], 0, sizeof(float) * lv->Lmax);
	lv->dpos = 0;
	lv->dq_head = lv->dq_count = 0;
	lv->e = 1.0;
	for (int i = 0; i < lv->Lmax; i++)
		lv->box[i] = 1.0;
	lv->box_pos = 0;
	lv->box_sum = (double)lv->L;
}

leveler_t *leveler_create(int channels, uint32_t sample_rate)
{
	if (channels < 1 || channels > LM_MAX_CH || sample_rate < 8000)
		return NULL;
	leveler_t *lv = calloc(1, sizeof(*lv));
	if (!lv)
		return NULL;
	lv->ch = channels;
	lv->fs = sample_rate;
	lufs_meter_init(&lv->meter, channels, lv->fs);
	lufs_meter_init(&lv->out_meter, channels, lv->fs);
	lv->in_lufs = lv->out_lufs = -200.0;
	lv->Lmax = (int)(lv->fs * MAX_LOOKAHEAD_MS / 1000.0) + 2;
	lv->dq_cap = lv->Lmax + 2;
	lv->dq_val = calloc(lv->dq_cap, sizeof(double));
	lv->dq_idx = calloc(lv->dq_cap, sizeof(uint64_t));
	lv->box = calloc(lv->Lmax, sizeof(double));
	for (int c = 0; c < channels; c++)
		lv->dl[c] = calloc(lv->Lmax, sizeof(float));
	bool ok = lv->dq_val && lv->dq_idx && lv->box;
	for (int c = 0; c < channels; c++)
		ok = ok && lv->dl[c];
	if (!ok) {
		leveler_destroy(lv);
		return NULL;
	}
	leveler_params_t p;
	leveler_default_params(&p);
	lv->L = 2;
	leveler_set_params(lv, &p);
	return lv;
}

void leveler_destroy(leveler_t *lv)
{
	if (!lv)
		return;
	for (int c = 0; c < lv->ch; c++)
		free(lv->dl[c]);
	free(lv->dq_val);
	free(lv->dq_idx);
	free(lv->box);
	free(lv);
}

static double clampd(double v, double lo, double hi)
{
	return v < lo ? lo : (v > hi ? hi : v);
}

void leveler_set_params(leveler_t *lv, const leveler_params_t *in)
{
	leveler_params_t p = *in;
	p.max_boost_db = clampd(p.max_boost_db, 0.0, 30.0);
	p.max_cut_db = clampd(p.max_cut_db, 0.0, 40.0);
	p.attack_ms = clampd(p.attack_ms, 5.0, 5000.0);
	p.release_ms = clampd(p.release_ms, 20.0, 30000.0);
	p.ceiling_db = clampd(p.ceiling_db, -30.0, 0.0);
	p.lookahead_ms = clampd(p.lookahead_ms, 1.0, MAX_LOOKAHEAD_MS);
	p.limiter_release_ms = clampd(p.limiter_release_ms, 5.0, 2000.0);
	lv->p = p;

	lv->att = exp(-1.0 / (p.attack_ms * 0.001 * lv->fs));
	lv->rel = exp(-1.0 / (p.release_ms * 0.001 * lv->fs));
	lv->ceil_lin = pow(10.0, p.ceiling_db / 20.0);
	lv->lim_up = 1.0 - exp(-1.0 / (p.limiter_release_ms * 0.001 * lv->fs));

	int newL = (int)lround(p.lookahead_ms * lv->fs / 1000.0);
	if (newL < 2)
		newL = 2;
	if (newL > lv->Lmax)
		newL = lv->Lmax;
	if (newL != lv->L || (p.limiter_enabled && !lv->lim_prev)) {
		lv->L = newL;
		limiter_reset(lv);
	}
	lv->lim_prev = p.limiter_enabled;
}

void leveler_process(leveler_t *lv, float *const *data, size_t frames)
{
	const leveler_params_t *p = &lv->p;
	const int nch = lv->ch;

	/* --- measure the incoming signal (feed-forward) --- */
	lufs_meter_process(&lv->meter, data, frames);
	double L_in = (p->window == LV_WINDOW_SHORT_TERM) ? lufs_meter_short_term(&lv->meter)
							  : lufs_meter_momentary(&lv->meter);
	lv->in_lufs = L_in;

	if (p->agc_enabled) {
		if (L_in > p->gate_lufs) {
			double want = clampd(p->target_lufs - L_in, -p->max_cut_db, p->max_boost_db);
			if (fabs(want - lv->target_db) > p->tolerance_lu)
				lv->target_db = want;
		} /* else: hold the current gain through silence/pauses */
	} else {
		lv->target_db = 0.0;
	}

	double min_a = 1.0;
	const int L = lv->L;

	for (size_t i = 0; i < frames; i++) {
		/* smoothed gain (attack = going down, release = going up) */
		double coef = (lv->target_db < lv->gain_db) ? lv->att : lv->rel;
		lv->gain_db = lv->target_db + (lv->gain_db - lv->target_db) * coef;
		float g = (float)exp(lv->gain_db * LN10_OVER_20);

		float xs[LM_MAX_CH];
		float peak = 0.0f;
		for (int c = 0; c < nch; c++) {
			float x = data[c][i] * g;
			xs[c] = x;
			float ax = fabsf(x);
			if (ax > peak)
				peak = ax;
		}

		if (!p->limiter_enabled) {
			for (int c = 0; c < nch; c++)
				data[c][i] = xs[c];
			continue;
		}

		/* required gain for this sample */
		double t = (peak > lv->ceil_lin) ? (lv->ceil_lin / peak) : 1.0;

		/* sliding minimum over the last L samples */
		while (lv->dq_count > 0) {
			int back = (lv->dq_head + lv->dq_count - 1) % lv->dq_cap;
			if (lv->dq_val[back] >= t)
				lv->dq_count--;
			else
				break;
		}
		int slot = (lv->dq_head + lv->dq_count) % lv->dq_cap;
		lv->dq_val[slot] = t;
		lv->dq_idx[slot] = lv->n;
		lv->dq_count++;
		while (lv->dq_idx[lv->dq_head] + (uint64_t)L <= lv->n) {
			lv->dq_head = (lv->dq_head + 1) % lv->dq_cap;
			lv->dq_count--;
		}
		double m = lv->dq_val[lv->dq_head];

		/* instant attack, exponential release */
		double e = lv->e + (1.0 - lv->e) * lv->lim_up;
		if (m < e)
			e = m;
		lv->e = e;

		/* moving average over L samples => smooth, click-free ramps;
		 * together with the (L-1) sample delay this guarantees that
		 * the gain applied to a sample is <= its required gain. */
		lv->box_sum += e - lv->box[lv->box_pos];
		lv->box[lv->box_pos] = e;
		lv->box_pos = (lv->box_pos + 1) % L;
		double a = lv->box_sum / (double)L;
		if (a > 1.0)
			a = 1.0;
		if (a < min_a)
			min_a = a;

		int D = L - 1;
		for (int c = 0; c < nch; c++) {
			float o = lv->dl[c][lv->dpos];
			lv->dl[c][lv->dpos] = xs[c];
			data[c][i] = o * (float)a;
		}
		lv->dpos = (lv->dpos + 1) % D;
		lv->n++;
	}

	lv->gr_db = 20.0 * log10(min_a);

	/* measure what leaves the filter, using the same window as the input */
	lufs_meter_process(&lv->out_meter, data, frames);
	lv->out_lufs = (p->window == LV_WINDOW_SHORT_TERM) ? lufs_meter_short_term(&lv->out_meter)
							   : lufs_meter_momentary(&lv->out_meter);
}

double leveler_input_lufs(const leveler_t *lv)
{
	return lv->in_lufs;
}
double leveler_output_lufs(const leveler_t *lv)
{
	return lv->out_lufs;
}
double leveler_gain_db(const leveler_t *lv)
{
	return lv->gain_db;
}
double leveler_limiter_gr_db(const leveler_t *lv)
{
	return lv->gr_db;
}
