#ifndef ARCANA_PLATFORM_H
#define ARCANA_PLATFORM_H

#include "../common/arcana_common.h"

/*
 * Platform abstraction layer.
 *
 * OS-specific behavior is centralized here so compiler and VM code
 * remain portable without #ifdef scattering.
 */

/* --- String duplication (POSIX strdup vs MSVC _strdup) --- */
char* arc_platform_strdup(const char* s);

/* --- File I/O --- */
/* Read entire file into malloc'd buffer. Caller must free. Returns NULL on error. */
uint8_t* arc_platform_read_file(const char* path, size_t* out_len);

/* Write buffer to file. Returns ARC_OK or ARC_ERR_IO. */
ArcStatus arc_platform_write_file(const char* path, const uint8_t* data, size_t len);

/* --- Monotonic clock (seconds) --- */
double arc_platform_clock(void);

/* --- Terminal --- */
/* Returns true if the given FILE* is an interactive terminal. */
bool arc_platform_is_terminal(FILE* f);

#endif /* ARCANA_PLATFORM_H */
