/* OBS audio filter: "Loudness Leveler + Limiter" with a live loudness meter */
#include <obs-module.h>
#include <media-io/audio-io.h>
#include <util/threading.h>
#include <util/platform.h>
#include <math.h>
#include <plugin-support.h>
#include "leveler.h"
#include "meter-api.h"
#include "text.h"

#define T(s) ll_text(s)

/* How often the meter checks whether the window needs refreshing. */
#define METER_POLL_MS 250
/* Minimum change (LU / dB) before the filter window is redrawn. */
#define METER_MIN_CHANGE 0.4

struct leveler_filter {
	obs_source_t *ctx;
	leveler_t *lv;
	int channels;
	uint32_t sample_rate;
	bool bypass;
	pthread_mutex_t mutex;

	/* live meter */
	pthread_t thread;
	os_event_t *stop_evt;
	bool thread_started;
	volatile bool live;    /* "Live meter" checkbox */
	volatile bool ui_seen; /* the settings window has been opened at least once */
	double shown[4];       /* values currently displayed: in, out, gain, limiter */

	/* copies of settings the dock panel needs */
	volatile double target;
	volatile bool agc;
};

static const char *lf_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return T("Filter.Name");
}

/* ---------- registry of live filters (read by the dock panel) ---------- */

#define LL_MAX_REGISTRY 32

static pthread_mutex_t g_reg_mutex;
static bool g_reg_ready;
static struct leveler_filter *g_reg[LL_MAX_REGISTRY];

void ll_registry_init(void)
{
	pthread_mutex_init(&g_reg_mutex, NULL);
	g_reg_ready = true;
}

void ll_registry_free(void)
{
	g_reg_ready = false;
	pthread_mutex_destroy(&g_reg_mutex);
}

static void registry_add(struct leveler_filter *f)
{
	if (!g_reg_ready)
		return;
	pthread_mutex_lock(&g_reg_mutex);
	for (int i = 0; i < LL_MAX_REGISTRY; i++) {
		if (!g_reg[i]) {
			g_reg[i] = f;
			break;
		}
	}
	pthread_mutex_unlock(&g_reg_mutex);
}

static void registry_remove(struct leveler_filter *f)
{
	if (!g_reg_ready)
		return;
	pthread_mutex_lock(&g_reg_mutex);
	for (int i = 0; i < LL_MAX_REGISTRY; i++)
		if (g_reg[i] == f)
			g_reg[i] = NULL;
	pthread_mutex_unlock(&g_reg_mutex);
}

int ll_meter_snapshot(struct ll_meter_row *rows, int max)
{
	int n = 0;
	if (!g_reg_ready)
		return 0;

	pthread_mutex_lock(&g_reg_mutex);
	for (int i = 0; i < LL_MAX_REGISTRY && n < max; i++) {
		struct leveler_filter *f = g_reg[i];
		if (!f || !f->lv)
			continue;
		struct ll_meter_row *r = &rows[n];
		obs_source_t *parent = obs_filter_get_parent(f->ctx);
		const char *name = parent ? obs_source_get_name(parent) : NULL;
		snprintf(r->source, sizeof(r->source), "%s", name ? name : "(not attached)");
		r->in_lufs = leveler_input_lufs(f->lv);
		r->out_lufs = leveler_output_lufs(f->lv);
		r->gain_db = leveler_gain_db(f->lv);
		r->limiter_db = leveler_limiter_gr_db(f->lv);
		r->target_lufs = f->target;
		r->bypass = f->bypass;
		r->agc = f->agc;
		n++;
	}
	pthread_mutex_unlock(&g_reg_mutex);
	return n;
}

/* ---------- live meter ---------- */

static void meter_snapshot(const struct leveler_filter *f, double v[4])
{
	v[0] = leveler_input_lufs(f->lv);
	v[1] = leveler_output_lufs(f->lv);
	v[2] = leveler_gain_db(f->lv);
	v[3] = leveler_limiter_gr_db(f->lv);
}

static void *meter_thread(void *arg)
{
	struct leveler_filter *f = arg;
	os_set_thread_name("loudness-leveler-meter");

	while (os_event_timedwait(f->stop_evt, METER_POLL_MS) == ETIMEDOUT) {
		if (!f->lv || !f->live || !f->ui_seen)
			continue;

		double now[4];
		meter_snapshot(f, now);
		bool changed = false;
		for (int i = 0; i < 4; i++)
			if (fabs(now[i] - f->shown[i]) >= METER_MIN_CHANGE)
				changed = true;

		/* Asks the settings window (if one is open) to rebuild itself;
		 * it then calls lf_properties() again, which reads fresh values. */
		if (changed)
			obs_source_update_properties(f->ctx);
	}
	return NULL;
}

static double clampd(double v, double lo, double hi)
{
	if (!(v == v)) /* NaN */
		return lo;
	return v < lo ? lo : (v > hi ? hi : v);
}

/* ---------- settings ---------- */

static void lf_read_params(obs_data_t *s, leveler_params_t *p)
{
	p->agc_enabled = obs_data_get_bool(s, "agc");
	p->target_lufs = obs_data_get_double(s, "target");
	p->window = (int)obs_data_get_int(s, "window");
	p->max_boost_db = obs_data_get_double(s, "max_boost");
	p->max_cut_db = obs_data_get_double(s, "max_cut");
	p->gate_lufs = obs_data_get_double(s, "gate");
	p->tolerance_lu = obs_data_get_double(s, "tolerance");
	p->attack_ms = obs_data_get_double(s, "attack");
	p->release_ms = obs_data_get_double(s, "release");
	p->limiter_enabled = obs_data_get_bool(s, "limiter");
	p->ceiling_db = obs_data_get_double(s, "ceiling");
	p->lookahead_ms = obs_data_get_double(s, "lookahead");
	p->limiter_release_ms = obs_data_get_double(s, "lim_release");
}

static void lf_update(void *data, obs_data_t *settings)
{
	struct leveler_filter *f = data;
	leveler_params_t p;
	lf_read_params(settings, &p);

	pthread_mutex_lock(&f->mutex);
	f->bypass = obs_data_get_bool(settings, "bypass");
	f->live = obs_data_get_bool(settings, "live_meter");
	f->target = p.target_lufs;
	f->agc = p.agc_enabled;
	if (f->lv)
		leveler_set_params(f->lv, &p);
	pthread_mutex_unlock(&f->mutex);
}

static void *lf_create(obs_data_t *settings, obs_source_t *source)
{
	struct leveler_filter *f = bzalloc(sizeof(*f));
	f->ctx = source;
	pthread_mutex_init(&f->mutex, NULL);

	audio_t *audio = obs_get_audio();
	f->channels = (int)audio_output_get_channels(audio);
	f->sample_rate = audio_output_get_sample_rate(audio);
	f->lv = leveler_create(f->channels, f->sample_rate);
	if (!f->lv)
		obs_log(LOG_ERROR, "unsupported audio format (%d ch @ %u Hz)", f->channels, f->sample_rate);

	lf_update(f, settings);

	if (os_event_init(&f->stop_evt, OS_EVENT_TYPE_MANUAL) == 0 &&
	    pthread_create(&f->thread, NULL, meter_thread, f) == 0)
		f->thread_started = true;

	registry_add(f);
	return f;
}

static void lf_destroy(void *data)
{
	struct leveler_filter *f = data;
	registry_remove(f); /* before anything is freed: the dock may be reading */
	if (f->thread_started) {
		os_event_signal(f->stop_evt);
		pthread_join(f->thread, NULL);
	}
	os_event_destroy(f->stop_evt);
	leveler_destroy(f->lv);
	pthread_mutex_destroy(&f->mutex);
	bfree(f);
}

static void lf_defaults(obs_data_t *s)
{
	leveler_params_t d;
	leveler_default_params(&d);
	obs_data_set_default_bool(s, "bypass", false);
	/* off by default: the dock panel is the main meter now */
	obs_data_set_default_bool(s, "live_meter", false);
	obs_data_set_default_bool(s, "agc", d.agc_enabled);
	obs_data_set_default_double(s, "target", d.target_lufs);
	obs_data_set_default_int(s, "window", d.window);
	obs_data_set_default_double(s, "max_boost", d.max_boost_db);
	obs_data_set_default_double(s, "max_cut", d.max_cut_db);
	obs_data_set_default_double(s, "gate", d.gate_lufs);
	obs_data_set_default_double(s, "tolerance", d.tolerance_lu);
	obs_data_set_default_double(s, "attack", d.attack_ms);
	obs_data_set_default_double(s, "release", d.release_ms);
	obs_data_set_default_bool(s, "limiter", d.limiter_enabled);
	obs_data_set_default_double(s, "ceiling", d.ceiling_db);
	obs_data_set_default_double(s, "lookahead", d.lookahead_ms);
	obs_data_set_default_double(s, "lim_release", d.limiter_release_ms);
	/* meter read-outs (display only) */
	obs_data_set_default_double(s, "m_in", -60.0);
	obs_data_set_default_double(s, "m_out", -60.0);
	obs_data_set_default_double(s, "m_gain", 0.0);
	obs_data_set_default_double(s, "m_gr", 0.0);
}

/* ---------- properties window ---------- */

/* Returning true makes OBS rebuild the window, which re-reads the meter. */
static bool refresh_clicked(obs_properties_t *props, obs_property_t *prop, void *data)
{
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(prop);
	UNUSED_PARAMETER(data);
	return true;
}

static bool live_toggled(obs_properties_t *props, obs_property_t *prop, obs_data_t *settings)
{
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(prop);
	UNUSED_PARAMETER(settings);
	return true;
}

static obs_property_t *add_meter_slider(obs_properties_t *g, const char *key, const char *label, double lo, double hi,
					const char *suffix)
{
	obs_property_t *p = obs_properties_add_float_slider(g, key, label, lo, hi, 0.1);
	obs_property_float_set_suffix(p, suffix);
	obs_property_set_enabled(p, false); /* read-only: it is a meter */
	return p;
}

static obs_properties_t *lf_properties(void *data)
{
	struct leveler_filter *f = data; /* NULL when OBS only lists defaults */
	obs_properties_t *props = obs_properties_create();
	obs_property_t *p;

	/* Put the newest readings into the (display-only) meter settings. */
	if (f && f->lv) {
		double v[4];
		meter_snapshot(f, v);
		obs_data_t *s = obs_source_get_settings(f->ctx);
		obs_data_set_double(s, "m_in", clampd(v[0], -60.0, 0.0));
		obs_data_set_double(s, "m_out", clampd(v[1], -60.0, 0.0));
		obs_data_set_double(s, "m_gain", clampd(v[2], -40.0, 30.0));
		obs_data_set_double(s, "m_gr", clampd(v[3], -20.0, 0.0));
		obs_data_release(s);
		for (int i = 0; i < 4; i++)
			f->shown[i] = v[i];
		f->ui_seen = true;
	}

	obs_properties_add_bool(props, "bypass", T("Bypass"));

	/* --- meter --- */
	obs_properties_t *mg = obs_properties_create();
	add_meter_slider(mg, "m_in", T("Meter.Input"), -60.0, 0.0, " LUFS");
	add_meter_slider(mg, "m_out", T("Meter.Output"), -60.0, 0.0, " LUFS");
	add_meter_slider(mg, "m_gain", T("Meter.Gain"), -40.0, 30.0, " dB");
	add_meter_slider(mg, "m_gr", T("Meter.Limiter"), -20.0, 0.0, " dB");
	p = obs_properties_add_bool(mg, "live_meter", T("Meter.Live"));
	obs_property_set_long_description(p, T("Meter.Live.Tip"));
	obs_property_set_modified_callback(p, live_toggled);
	obs_properties_add_button(mg, "meter_refresh", T("Meter.Refresh"), refresh_clicked);
	obs_properties_add_group(props, "meter_group", T("Meter.Group"), OBS_GROUP_NORMAL, mg);

	/* --- auto level --- */
	obs_properties_t *ag = obs_properties_create();
	p = obs_properties_add_float_slider(ag, "target", T("Target"), -40.0, -6.0, 0.5);
	obs_property_float_set_suffix(p, " LUFS");
	p = obs_properties_add_list(ag, "window", T("Window"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, T("Window.Momentary"), LV_WINDOW_MOMENTARY);
	obs_property_list_add_int(p, T("Window.ShortTerm"), LV_WINDOW_SHORT_TERM);
	p = obs_properties_add_float_slider(ag, "max_boost", T("MaxBoost"), 0.0, 30.0, 0.5);
	obs_property_float_set_suffix(p, " dB");
	p = obs_properties_add_float_slider(ag, "max_cut", T("MaxCut"), 0.0, 40.0, 0.5);
	obs_property_float_set_suffix(p, " dB");
	p = obs_properties_add_float_slider(ag, "gate", T("Gate"), -70.0, -20.0, 1.0);
	obs_property_float_set_suffix(p, " LUFS");
	p = obs_properties_add_float_slider(ag, "tolerance", T("Tolerance"), 0.0, 6.0, 0.1);
	obs_property_float_set_suffix(p, " LU");
	p = obs_properties_add_float_slider(ag, "attack", T("Attack"), 5.0, 2000.0, 5.0);
	obs_property_float_set_suffix(p, " ms");
	p = obs_properties_add_float_slider(ag, "release", T("Release"), 50.0, 10000.0, 10.0);
	obs_property_float_set_suffix(p, " ms");
	obs_properties_add_group(props, "agc", T("AutoLevel"), OBS_GROUP_CHECKABLE, ag);

	/* --- limiter --- */
	obs_properties_t *lg = obs_properties_create();
	p = obs_properties_add_float_slider(lg, "ceiling", T("Ceiling"), -20.0, 0.0, 0.1);
	obs_property_float_set_suffix(p, " dBFS");
	p = obs_properties_add_float_slider(lg, "lookahead", T("Lookahead"), 1.0, 20.0, 0.5);
	obs_property_float_set_suffix(p, " ms");
	p = obs_properties_add_float_slider(lg, "lim_release", T("LimiterRelease"), 5.0, 1000.0, 5.0);
	obs_property_float_set_suffix(p, " ms");
	obs_properties_add_group(props, "limiter", T("Limiter"), OBS_GROUP_CHECKABLE, lg);

	return props;
}

/* ---------- audio ---------- */

static struct obs_audio_data *lf_filter_audio(void *data, struct obs_audio_data *audio)
{
	struct leveler_filter *f = data;
	if (!f->lv || f->bypass || !audio->frames)
		return audio;

	for (int c = 0; c < f->channels; c++)
		if (!audio->data[c])
			return audio;

	pthread_mutex_lock(&f->mutex);
	leveler_process(f->lv, (float *const *)audio->data, audio->frames);
	pthread_mutex_unlock(&f->mutex);
	return audio;
}

struct obs_source_info loudness_leveler_filter = {
	.id = "loudness_leveler_filter",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_AUDIO,
	.get_name = lf_get_name,
	.create = lf_create,
	.destroy = lf_destroy,
	.update = lf_update,
	.get_defaults = lf_defaults,
	.get_properties = lf_properties,
	.filter_audio = lf_filter_audio,
};
