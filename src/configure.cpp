// In this file: processing requests to configure the plugin.
// see  struct _DevicePluginRec in ./fork-plugin.cpp

// It accesses: ->config and ->max_last

#include "config.h"
#include "debug.h"


// bug these 2 ^ v
#include "xmachine.h"
#include "xorg.h"

#include "configure.h"
#include "history.h"
#include <memory>

/* something to define NULL */
extern "C"
{
#include "fork_requests.h"
#undef max
#undef min
}


/* fixme:  where is the documentation: fork_requests.h ? */

enum twin_relationship {
    total_limit = fork_configure_total_limit,
    overlap_limit = fork_configure_overlap_limit,
};
