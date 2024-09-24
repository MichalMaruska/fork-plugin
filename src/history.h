#pragma once

// #define DEBUG 1
#include "circular.h"
#include "platform.h"

extern "C" {
#include "fork_requests.h"
}
// #include "../include/archived_event.h"

typedef struct {
  PlatformEvent*  p_event;
  KeyCode forked; /* if forked to (another keycode), this is the original key */
} key_event;

/* (100) */
// not value-semantics
typedef circular_buffer<archived_event*> last_events_type;

// why extern?
archived_event* make_archived_event(const key_event *ev);
int dump_last_events_to_client(PluginInstance* plugin, ClientPtr client, int n);
void dump_last_events(PluginInstance* plugin);
