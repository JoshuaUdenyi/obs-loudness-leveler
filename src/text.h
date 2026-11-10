#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Looks the key up in the locale file; if that file is missing (or has no such
 * key) the built-in English text is returned, so labels always read properly. */
const char *ll_text(const char *key);

#ifdef __cplusplus
}
#endif
