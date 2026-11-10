#include <obs-module.h>
#include <string.h>
#include "text.h"

struct kv {
	const char *key;
	const char *text;
};

static const struct kv table[] = {
	{"Filter.Name", "Loudness Leveler + Limiter"},
	{"Bypass", "Bypass (turn everything off)"},
	{"Meter.Group", "Live meter"},
	{"Meter.Input", "Input loudness"},
	{"Meter.Output", "Output loudness"},
	{"Meter.Gain", "Gain being applied"},
	{"Meter.Limiter", "Limiter reduction"},
	{"Meter.Live", "Update this window live"},
	{"Meter.Live.Tip",
	 "Redraws this window a few times per second while you speak. Turn it off while dragging sliders. "
	 "The Loudness Leveler panel (View > Docks) is the smoother meter."},
	{"Meter.Refresh", "Refresh meter"},
	{"AutoLevel", "Auto-level to target loudness"},
	{"Target", "Target loudness"},
	{"Window", "Measurement window"},
	{"Window.Momentary", "Momentary (400 ms) - reacts fast"},
	{"Window.ShortTerm", "Short-term (3 s) - smoother"},
	{"MaxBoost", "Max boost (when too quiet)"},
	{"MaxCut", "Max reduction (when too loud)"},
	{"Gate", "Gate: ignore quieter than"},
	{"Tolerance", "Tolerance (no change within)"},
	{"Attack", "Reduce speed (attack)"},
	{"Release", "Raise speed (release)"},
	{"Limiter", "Limiter"},
	{"Ceiling", "Limiter ceiling"},
	{"Lookahead", "Lookahead"},
	{"LimiterRelease", "Limiter release"},
	{"Dock.Title", "Loudness Leveler"},
	{"Dock.Empty",
	 "No Loudness Leveler filter found.\n\nAdd \"Loudness Leveler + Limiter\" to a mic's filters and its meter appears here."},
	{"Dock.Bypassed", "BYPASSED"},
	{"Dock.In", "IN"},
	{"Dock.Out", "OUT"},
	{"Dock.Gain", "Gain"},
	{"Dock.Limiter", "Limiter"},
};

const char *ll_text(const char *key)
{
	const char *t = obs_module_text(key);
	if (t && strcmp(t, key) != 0)
		return t; /* found in the locale file */

	for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++)
		if (strcmp(table[i].key, key) == 0)
			return table[i].text;
	return key;
}
