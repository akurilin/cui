#pragma once

#include <stddef.h>

/*
 * Heap-allocate a copy of source (including the NUL terminator).
 *
 * Behavior:
 * - Returns NULL when source is NULL or when malloc fails.
 * - Caller owns the returned pointer and must free() it.
 */
char *duplicate_string(const char *source);
