/* Shared between the audio filter (C) and the dock panel (C++/Qt). */
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LL_MAX_METERS 16

struct ll_meter_row {
	char source[128];  /* name of the source (mic) the filter is on */
	double in_lufs;    /* loudness going in; -200 means silence */
	double out_lufs;   /* loudness coming out */
	double gain_db;    /* gain the leveler is applying */
	double limiter_db; /* limiter reduction, 0 or negative */
	double target_lufs;
	bool bypass;
	bool agc;
};

/* Registry of active filters (created/destroyed with the plugin). */
void ll_registry_init(void);
void ll_registry_free(void);

/* Copies the current readings of every active filter. Returns the count. */
int ll_meter_snapshot(struct ll_meter_row *rows, int max);

/* The dock panel next to the Audio Mixer. */
void ll_dock_init(void);
void ll_dock_free(void);

#ifdef __cplusplus
}
#endif
