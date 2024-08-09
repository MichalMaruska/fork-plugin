#ifndef _DEBUG_H_
#define _DEBUG_H_

#include "config.h"

// #ifndef NDEBUG
#define DEBUG 1
#if DEBUG

#if USE_COLORS
#define escape_sequence "\x1b"
#define info_color "\x1b[47;31m"
#define fork_color "\x1b[47;30m"
#define key_color "\x1b[01;32;41m"
#define keysym_color "\x1b[33;01m"
#define warning_color "\x1b[38;5;160m"
#define key_io_color "\x1b[38;5;226m"
#define color_reset "\x1b[0m"
# else // USE_COLORS
#define info_color ""
#define key_color  ""
#define warning_color  ""
#define color_reset ""
#define escape_sequence ""
// ....
#endif // USE_COLORS

#define DB(fmt, ...)     ErrorF(fmt, ##__VA_ARGS__)

# else  /* DEBUG */
#define DB(x)  do { ; } while (0)
#endif /* DEBUG */


extern "C" {
#include <xorg-server.h>
#include <xorg/inputstr.h>
}

const char*
describe_key(DeviceIntPtr keybd, InternalEvent *event);

#endif /* _DEBUG_H_ */
