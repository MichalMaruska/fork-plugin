#pragma once

// #define DEBUG 1
#include <boost/circular_buffer.hpp>

extern "C" {
#include "fork_requests.h"
}
// #include "../include/archived_event.h"

class PlatformEvent;

struct key_event {
  PlatformEvent*  p_event;
  KeyCode forked; /* if forked to (another keycode), this is the original key */
};

/* (100) */
// not value-semantics
typedef boost::circular_buffer<archived_event*> last_events_type;

// why extern?
archived_event* make_archived_event(const key_event *ev);
int dump_last_events_to_client(PluginInstance* plugin, ClientPtr client, int n);
void dump_last_events(PluginInstance* plugin);
