#ifndef _HISTORY_H_
#define _HISTORY_H_


// #include "fork.h"

// fixme: this should include:
extern "C" {
#include <xorg-server.h>

#include <X11/X.h>
#include <X11/Xproto.h>
#include <X11/keysym.h>
#include <xorg/inputstr.h>
  // Bool:
  // /usr/include/X11/Xdefs.h:typedef

#include <X11/Xdefs.h>
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
