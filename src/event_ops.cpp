#include "event_ops.h"

// specific keysyms:
#include <X11/keysym.h>

Bool
release_p(const InternalEvent* event)
{
   assert(event);
   return (event->any.type == ET_KeyRelease);
}

Bool
press_p(const InternalEvent* event)
{
   assert(event);
   return (event->any.type == ET_KeyPress);
}

Time
time_of(const InternalEvent* event)
{
   return event->any.time;
}

KeyCode
detail_of(const InternalEvent* event)
{
   assert(event);
   return event->device_event.detail.key;
}

// (printable) Name of the event
const char*
event_type_brief(InternalEvent *event)
{
    return (press_p(event)?"down":
            (release_p(event)?"up":"??"));
}
