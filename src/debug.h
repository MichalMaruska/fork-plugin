#pragma once

#include "config.h"

/*
 * Logging for code which does not yet have the `machine'.
 * X specific.
 */

// #ifndef NDEBUG
// #define DEBUG 1

#if DEBUG
# define DB(fmt, ...)     ErrorF(fmt, ##__VA_ARGS__)
# else  /* DEBUG */
#define DB(fmt, ...)  do { ; } while (0)
#endif /* DEBUG */
