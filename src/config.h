#ifndef CONFIG_H
#define CONFIG_H

// I want the version string settable from the command line!!
#ifndef VERSION_STRING
# define VERSION_STRING "(unknown version)"
#endif

#define PLUGIN_VERSION 35


// use -DNDEBUG to avoid asserts!
// Trace + debug:
#define DEBUG 1

// I like colored tracing (in 256-color xterm)
#define USE_COLORS 1

#define STATIC_LAST 1


/** What does the lock protect?  ... access to the  queues,state
 * mouse signal handler cannot just make "fork", while a key event is being analyzed.
 *
 * Locking is broken: but it's not used now:
 *
 *   ---> keyevent ->  xkb action -> mouse
 *                                     |
 *   prev   <----                 <---  thaw
 *        ->   process  \
 *               exits  /
 *             unlocks!
 *
 *
 *  lock is gone!! <- action */
#define USE_LOCKING 1



// inside the X server: #define TIME_FORMAT PRIu32
#define TIME_FMT  "u"
#define SIZE_FMT  "lu"

#endif
