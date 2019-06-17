#ifndef _HISTORY_H_
#define _HISTORY_H_


extern "C" {
#include <xorg-server.h>

#include <X11/keysym.h>

// ^ for safety: before v
#include <xorg/inputstr.h>

#include "fork_requests.h"
}

#include "circular.h"

typedef struct {
  InternalEvent* event;
  KeyCode forked; /* if forked to (another keycode), this is the original key */
} key_event;


/* (100) */
typedef circular_buffer<archived_event*> last_events_type;

extern archived_event* make_archived_events(key_event* ev);
extern int dump_last_events_to_client(PluginInstance* plugin, ClientPtr client, int n);

void dump_last_events(PluginInstance* plugin);


#endif
