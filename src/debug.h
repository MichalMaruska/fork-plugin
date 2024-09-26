#ifndef _DEBUG_H_
#define _DEBUG_H_

#include "config.h"

// #ifndef NDEBUG
// #define DEBUG 1

#if DEBUG
# define DB(fmt, ...)     ErrorF(fmt, ##__VA_ARGS__)
# else  /* DEBUG */
#define DB(fmt, ...)  do { ; } while (0)
#endif /* DEBUG */


extern "C" {
#include <xorg-server.h>
#include <xorg/inputstr.h>

#undef max
#undef min
}

#endif /* _DEBUG_H_ */
