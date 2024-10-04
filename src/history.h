#pragma once

// #define DEBUG 1
#include <boost/circular_buffer.hpp>

extern "C" {
#include "fork_requests.h"
#undef max
#undef min
}
// #include "../include/archived_event.h"

/* (100) */
// not value-semantics
typedef boost::circular_buffer<archived_event> last_events_type;

// why extern?
int dump_last_events_to_client(PluginInstance* plugin, ClientPtr client, int n);
