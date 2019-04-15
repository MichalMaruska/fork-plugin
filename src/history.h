#ifndef _HISTORY_H_
#define _HISTORY_H_


extern "C" {

#include <X11/keysym.h>
#include <xorg/inputstr.h>

#include "fork_requests.h"
}

#include "circular.h"


typedef struct {
  InternalEvent* event;
  KeyCode forked; /* if forked to (another keycode), this is the original key */
} key_event;

#if 0 // for now from fork_requests.h
typedef struct
{
        time_t time;
        KeyCode key;
        KeyCode forked;
        bool press;
} archived_event;
#endif


typedef circular_buffer<archived_event*> last_events_type; /* (100) */

extern archived_event* make_archived_events(key_event* ev);
extern int dump_last_events_to_client(PluginInstance* plugin, ClientPtr client, int n);

void dump_last_events(PluginInstance* plugin);



#endif
