/*
 *  Keep a history (= list of most recent key events):
 */

#include "config.h"
#include "debug.h"

#include "history.h"
#include "machine.h"

extern "C" {
#include "fork_requests.h"
}

#include "event_ops.h"
#include "xmachine.h"

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
