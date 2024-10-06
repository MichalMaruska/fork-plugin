#pragma once

// #define DEBUG 1
extern "C" {
#include "fork_requests.h"
#undef max
#undef min
}

int dump_last_events_to_client(PluginInstance* plugin, ClientPtr client, int n);
