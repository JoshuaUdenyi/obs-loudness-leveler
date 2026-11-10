#include "loudness.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* K-weighting filter coefficients valid for any sample rate (same
 * derivation as libebur128). */
static void make_shelf(lm_biquad_t *b, double fs)
{
	const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
	double K = tan(M_PI * f0 / fs);
	double Vh = pow(10.0, G / 20.0);
	double Vb = pow(Vh, 0.4996667741545416);
	double a0 = 1.0 + K / Q + K * K;
	b->b0 = (Vh + Vb * K / Q + K * K) / a0;
	b->b1 = 2.0 * (K * K - Vh) / a0;
	b->b2 = (Vh - Vb * K / Q + K * K) / a0;
	b->a1 = 2.0 * (K * K - 1.0) / a0;
	b->a2 = (1.0 - K / Q + K * K) / a0;
	b->z1 = b->z2 = 0.0;
}

static void make_hpf(lm_biquad_t *b, double fs)
{
	const double f0 = 38.13547087602444, Q = 0.5003270373238773;
	double K = tan(M_PI * f0 / fs);
	double a0 = 1.0 + K / Q + K * K;
	b->b0 = 1.0;
	b->b1 = -2.0;
	b->b2 = 1.0;
	b->a1 = 2.0 * (K * K - 1.0) / a0;
	b->a2 = (1.0 - K / Q + K * K) / a0;
	b->z1 = b->z2 = 0.0;
}

static inline double bq(lm_biquad_t *b, double x)
{
	double y = b->b0 * x + b->z1;
	b->z1 = b->b1 * x - b->a1 * y + b->z2;
	b->z2 = b->b2 * x - b->a2 * y;
	return y;
}

void lufs_meter_init(lufs_meter_t *m, int channels, double sr)
{
	memset(m, 0, sizeof(*m));
	if (channels > LM_MAX_CH)
		channels = LM_MAX_CH;
	m->channels = channels;
	m->sample_rate = sr;
	m->block_len = (size_t)(sr * 0.1);
	if (m->block_len < 1)
		m->block_len = 1;
	for (int c = 0; c < channels; c++) {
		make_shelf(&m->shelf[c], sr);
		make_hpf(&m->hpf[c], sr);
		/* OBS layouts: 0 FL, 1 FR, 2 FC, 3 LFE, 4/5 surround, 6/7 side */
		m->weight[c] = (c == 3) ? 0.0 : (c >= 4 ? 1.41253754462275 : 1.0);
	}
}

void lufs_meter_process(lufs_meter_t *m, float *const *data, size_t frames)
{
	for (size_t i = 0; i < frames; i++) {
		for (int c = 0; c < m->channels; c++) {
			double y = bq(&m->hpf[c], bq(&m->shelf[c], data[c][i]));
			m->sumsq[c] += y * y;
		}
		if (++m->block_pos >= m->block_len) {
			double e = 0.0;
			for (int c = 0; c < m->channels; c++) {
				e += m->weight[c] * m->sumsq[c] / (double)m->block_len;
				m->sumsq[c] = 0.0;
			}
			m->block_pos = 0;
			m->ring[m->ring_pos] = e;
			m->ring_pos = (m->ring_pos + 1) % LM_RING;
			if (m->ring_count < LM_RING)
				m->ring_count++;
		}
	}
}

static double window(const lufs_meter_t *m, int n)
{
	if (m->ring_count == 0)
		return -200.0;
	if (n > m->ring_count)
		n = m->ring_count;
	double sum = 0.0;
	for (int k = 1; k <= n; k++)
		sum += m->ring[(m->ring_pos - k + LM_RING * 2) % LM_RING];
	double mean = sum / n;
	if (mean < 1e-20)
		return -200.0;
	return -0.691 + 10.0 * log10(mean);
}

double lufs_meter_momentary(const lufs_meter_t *m)
{
	return window(m, 4);
}
double lufs_meter_short_term(const lufs_meter_t *m)
{
	return window(m, LM_RING);
}
