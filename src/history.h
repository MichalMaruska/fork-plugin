#ifndef _HISTORY_H_
#define _HISTORY_H_

extern "C" {
#include <xorg/inputstr.h>
#include "fork_requests.h"
}

#define DEBUG 1
#include "circular.h"

typedef struct {
  // InternalEvent*
  PlatformEvent*  p_event;
  KeyCode forked; /* if forked to (another keycode), this is the original key */
} key_event;

/* (100) */
typedef circular_buffer<archived_event*> last_events_type;

extern archived_event* make_archived_event(const key_event *ev);
extern int dump_last_events_to_client(PluginInstance* plugin, ClientPtr client, int n);

void dump_last_events(PluginInstance* plugin);

#endif
