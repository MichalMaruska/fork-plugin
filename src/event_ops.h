/* I use it only to print out the keysym in debugging stuff*/

#include <xorg-server.h>
// to include the below files *correctly*, I need the the config!
#include <X11/X.h>

// #include <X11/Xproto.h>
// specific keysyms:
#include <X11/keysym.h>

// only after ..
// events:
#include <xorg/eventstr.h>

// only after ..
#include <xorg/xkbsrv.h>


inline Bool
release_p(const InternalEvent* event)
{
   assert(event);
   return (event->any.type == ET_KeyRelease);
}

inline Bool
press_p(const InternalEvent* event)
{
   assert(event);
   return (event->any.type == ET_KeyPress);
}

inline Time
time_of(const InternalEvent* event)
{
   return event->any.time;
}

inline
KeyCode
detail_of(const InternalEvent* event)
{
   assert(event);
   return event->device_event.detail.key;
}

// (printable) Name of the event
inline const char*
event_type_brief(InternalEvent *event)
{
    return (press_p(event)?"down":
            (release_p(event)?"up":"??"));
}
