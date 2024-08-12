#ifndef _HISTORY_H_
#define _HISTORY_H_


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
typedef circular_buffer<archived_event*> last_events_type;

extern archived_event* make_archived_event(const key_event *ev);
extern int dump_last_events_to_client(PluginInstance* plugin, ClientPtr client, int n);
void dump_last_events(PluginInstance* plugin);

#endif
