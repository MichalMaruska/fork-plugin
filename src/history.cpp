/*
 *  Keep a history (= list of most recent key events):
 */

#include "config.h"
#include "debug.h"

#include "history.h"
#include "fork.h"

extern "C" {
#include "fork_requests.h"

#include <xorg-server.h>
#include <xorg/xkbsrv.h>
#include <X11/X.h>
#include <X11/Xproto.h>

/* `probably' I use it only to print out the keysym in debugging stuff*/
#if 0
#include <xorg/eventstr.h>
#endif

#undef max
#undef min
}

#include "event_ops.h"

using machineRec = forkingMachine<Time, KeyCode>;

archived_event*
make_archived_event(const key_event* const ev)
{
  auto *event = MALLOC(archived_event);
  if (event == nullptr)
    return NULL;
  DB("%s: %d %p %p\n", __func__, __LINE__, ev, ev->p_event);
  DB("%s:%d into %p\n", __func__, __LINE__, event);


  DB("%s: end\n", __func__);
  return event;
}

/* ---------------------
 * Return the message to send as Xreply, len is filled with the length.
 * length<0  means error!
 *
 * or ?? just invoke `plugin_send_reply'(ClientPtr client, char* message, int length)
 * --------------------
 */
int
dump_last_events_to_client(PluginInstance* plugin, ClientPtr client, int max_requested)
{
   machineRec* machine = plugin_machine(plugin);
   int queue_count = machine->max_last;           // I don't need to count them! last_events_count

#if 0
   if (queue_count > machine->max_last)
       queue_count = machine->max_last;
#endif

   // how many in the store?
   // upper bound
   // trim/clamp?
   if (max_requested > queue_count) {
       max_requested = queue_count;
   };

   // allocate the appendix buffer:
   size_t appendix_len = sizeof(fork_events_reply) + (max_requested * sizeof(archived_event));
   /* no alignment! */

   /* fork_events_reply; */

#if 0 /* useless? */
   int remainder = appendix_len  % 4;
   appendix_len += (remainder?(4 - remainder):0);
/* endi */
#endif

   const auto start = static_cast<char *>(alloca(appendix_len));
   auto *buf = reinterpret_cast<fork_events_reply *>(start);

   buf->count = max_requested;              /* fixme: BYTE SWAP if needed! */

#if 0
   // todo:
   // fixme: we need to increase an iterator .. pointer .... to the C array!
   last_events.for_each(begin(),
                        end(),
                        function);
#endif

   DB("sending %d events: + %zd!\n", max_requested, appendix_len);

// BUG -- at least for the test?
# if 0
   int r = xkb_plugin_send_reply(client, plugin, start, appendix_len);
   if (r == 0)
      return client->noClientException;
   return r;
#else
   return 0;
#endif
}


// prints into the Xorg.*.log
static void
dump_event(KeyCode key, KeyCode fork, bool press, Time event_time,
           XkbDescPtr xkb, XkbSrvInfoPtr xkbi, Time prev_time)
{
    char* ksname = xkb->names->keys[key].name;
    DB("%d %.4s\n", key, ksname);

    // 0.1   keysym bound to the key:
    KeySym* sym= XkbKeySymsPtr(xkbi->desc,key); // mmc: is this enough ?
    char* sname = nullptr;

    if (sym){
#if 0
        // todo: fixme!
        sname = XkbKeysymText(*sym,XkbCFile); // doesn't work inside server !!
#endif
        // my ascii hack
        if (! isalpha(* reinterpret_cast<unsigned char *>(sym))){
            sym = (KeySym*) " ";
        } else {
            static char keysymname[15];
            sprintf(keysymname, "%c",(* (char*)sym)); // fixme!
            sname = keysymname;
        };
    };
    /*  Format:
        keycode
        press/release
        [  57 4 18500973        112
        ] 33   18502021        1048
    */

    DB("%s %d (%d)" ,(press?" ]":"[ "),
           static_cast<int>(key), static_cast<int>(fork));
    DB(" %.4s (%5.5s) %" TIME_FMT "\t%" TIME_FMT "\n",
           ksname, sname,
           event_time,
           event_time - prev_time);
}


// Closure
class event_dumper
{
private:
  DeviceIntPtr keybd;
  XkbSrvInfoPtr xkbi;
  XkbDescPtr xkb;

  int index;
  Time previous_time;

public:
  void operator() (archived_event*& event)
  {
    dump_event(event->key,
               event->forked,
               event->press,
               event->time,
               xkb, xkbi, previous_time);
    previous_time = event->time;
  };

  explicit event_dumper(const PluginInstance* plugin, int i = 0) : index(i), previous_time(0)
  {
    keybd = plugin->device;
    xkbi = keybd->key->xkbInfo;
    xkb = xkbi->desc;
  };
};


void
dump_last_events(PluginInstance* plugin)
{
  machineRec* machine = plugin_machine(plugin);
  DB("%s(%s) %" SIZE_FMT "\n", __FUNCTION__,
         plugin->device->name,
         machine->last_events->size());

  for_each(machine->last_events->begin(),
           machine->last_events->end(),
           event_dumper(plugin));
}
