/* Standalone DSP tests (no OBS needed).
 * gcc -O2 -Wall dsp_test.c leveler.c loudness.c -lm -o dsp_test */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "leveler.h"

#define FS 48000
#define BUF 480

static int fails;
#define CHECK(cond, ...)                                  \
	do {                                              \
		if (!(cond)) {                            \
			printf("  FAIL: " __VA_ARGS__);     \
			printf("\n");                     \
			fails++;                          \
		} else {                                  \
			printf("  ok:   " __VA_ARGS__);   \
			printf("\n");                     \
		}                                         \
	} while (0)

/* run `seconds` of a stereo 1 kHz sine through lv; returns final output LUFS */
static double run_sine(leveler_t *lv, double amp, double seconds, double *peak_out, double start_s)
{
	lufs_meter_t out;
	lufs_meter_init(&out, 2, FS);
	float l[BUF], r[BUF];
	float *ch[2] = {l, r};
	double peak = 0;
	long total = (long)(seconds * FS);
	for (long n = 0; n < total; n += BUF) {
		for (int i = 0; i < BUF; i++) {
			double t = (double)(n + i) / FS;
			float v = (t < start_s) ? 0.0f : (float)(amp * sin(2 * M_PI * 1000.0 * t));
			l[i] = r[i] = v;
		}
		leveler_process(lv, ch, BUF);
		lufs_meter_process(&out, ch, BUF);
		for (int i = 0; i < BUF; i++)
			if (fabs(l[i]) > peak)
				peak = fabs(l[i]);
	}
	if (peak_out)
		*peak_out = peak;
	return lufs_meter_short_term(&out);
}

int main(void)
{
	leveler_params_t p;

	printf("Test 1: quiet signal is boosted to target\n");
	{
		leveler_t *lv = leveler_create(2, FS);
		leveler_default_params(&p);
		p.target_lufs = -20;
		p.limiter_enabled = false;
		leveler_set_params(lv, &p);
		double out = run_sine(lv, 0.05, 40, NULL, 0);
		printf("  in %.2f LUFS -> out %.2f LUFS (gain %+.2f dB)\n", leveler_input_lufs(lv), out,
		       leveler_gain_db(lv));
		CHECK(fabs(out - (-20.0)) < 1.0, "output within 1 LU of -20");
		CHECK(fabs(leveler_output_lufs(lv) - (-20.0)) < 1.0, "built-in output meter reads %.2f LUFS",
		      leveler_output_lufs(lv));
		leveler_destroy(lv);
	}

	printf("Test 2: loud signal is reduced to target\n");
	{
		leveler_t *lv = leveler_create(2, FS);
		leveler_default_params(&p);
		p.target_lufs = -20;
		p.limiter_enabled = false;
		leveler_set_params(lv, &p);
		double out = run_sine(lv, 0.5, 40, NULL, 0);
		printf("  out %.2f LUFS (gain %+.2f dB)\n", out, leveler_gain_db(lv));
		CHECK(fabs(out - (-20.0)) < 1.0, "output within 1 LU of -20");
		CHECK(leveler_gain_db(lv) < -4.0, "gain was reduced");
		leveler_destroy(lv);
	}

	printf("Test 3: boost never exceeds max boost; silence is gated\n");
	{
		leveler_t *lv = leveler_create(2, FS);
		leveler_default_params(&p);
		p.target_lufs = -10;
		p.max_boost_db = 6;
		p.limiter_enabled = false;
		leveler_set_params(lv, &p);
		run_sine(lv, 0.01, 40, NULL, 0);
		CHECK(leveler_gain_db(lv) <= 6.05, "gain %.2f dB <= +6 dB cap", leveler_gain_db(lv));
		leveler_destroy(lv);

		lv = leveler_create(2, FS);
		leveler_set_params(lv, &p);
		run_sine(lv, 0.0, 10, NULL, 0);
		CHECK(fabs(leveler_gain_db(lv)) < 0.01, "silence does not raise gain (%.3f dB)", leveler_gain_db(lv));
		leveler_destroy(lv);
	}

	printf("Test 4: limiter holds ceiling, even on a sudden full-scale onset\n");
	{
		leveler_t *lv = leveler_create(2, FS);
		leveler_default_params(&p);
		p.agc_enabled = false;
		p.ceiling_db = -6.0;
		p.lookahead_ms = 5;
		leveler_set_params(lv, &p);
		double peak;
		run_sine(lv, 1.0, 3, &peak, 0.5);
		double ceil = pow(10, -6.0 / 20.0);
		printf("  peak %.4f (ceiling %.4f), GR %.2f dB\n", peak, ceil, leveler_limiter_gr_db(lv));
		CHECK(peak <= ceil * 1.001, "peak never above ceiling");
		leveler_destroy(lv);
	}

	printf("Test 5: limiter is transparent below ceiling\n");
	{
		leveler_t *lv = leveler_create(2, FS);
		leveler_default_params(&p);
		p.agc_enabled = false;
		leveler_set_params(lv, &p);
		double peak;
		run_sine(lv, 0.3, 2, &peak, 0);
		CHECK(fabs(peak - 0.3) < 0.002, "peak %.4f ~ 0.3", peak);
		leveler_destroy(lv);
	}

	printf(fails ? "\n%d FAILED\n" : "\nALL PASSED\n", fails);
	return fails != 0;
}
