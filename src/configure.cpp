// In this file: processing requests to configure the plugin.
// see  struct _DevicePluginRec in ./fork-plugin.cpp

// It accesses: ->config and ->max_last

#include "config.h"
#include "debug.h"


// bug these 2 ^ v
#include "xmachine.h"
#include "xorg.h"

#include "configure.h"
#include "history.h"
#include <memory>

/* something to define NULL */
extern "C"
{
#include "fork_requests.h"
#undef max
#undef min
}


/* fixme:  where is the documentation: fork_requests.h ? */



// todo: make it inline functions
inline int subtype_n_args(int t) { return  (t & 3);}
inline int type_subtype(int t) { return (t >> 2);}



/* Return a value requested, or 0 on error.*/
int
machine_configure_get(PluginInstance* plugin, int values[5], int return_config[3])
{
   assert (strcmp (PLUGIN_NAME(plugin), FORK_PLUGIN_NAME) == 0);

   machineRec* machine = plugin_machine(plugin);

   int type = values[0];

   /* fixme: why is type int?  shouldn't CARD8 be enough?
      <int type>
      <int keycode or time value>
      <keycode or time value>
      <timevalue>

      type: local & global
   */

   machine->mdb("%s: %d operands, command %d: %d %d\n", __func__, subtype_n_args(type),
                type_subtype(type), values[1], values[2]);

   switch (subtype_n_args(type)){
   case 0:
           return_config[0]= machine->configure_global(type_subtype(type), 0, 0);
           break;
   case 1:
           return_config[0]= machine->configure_key(type_subtype(type), values[1], 0, 0);
           break;
   case 2:
           return_config[0]= machine->configure_twins(type_subtype(type), values[1], values[2], 0, 0);
           break;
   case 3:
           return 0;
        default:
      machine->mdb("%s: invalid option %d\n", __func__, subtype_n_args(type));
           ;
   }
   return 0;
}


/* Scan the DATA (of given length), and translate into configuration commands,
   and execute on plugin's machine */
int
machine_configure(PluginInstance* plugin, int values[5])
{
   assert (strcmp (PLUGIN_NAME(plugin), FORK_PLUGIN_NAME) == 0);

   machineRec* machine = plugin_machine(plugin);

   int type = values[0];
   machine->mdb("%s: %d operands, command %d: %d %d %d\n", __func__,
                subtype_n_args(type), type_subtype(type),
                values[1], values[2],values[3]);

   switch (subtype_n_args(type)) {
   case 0:
      machine->configure_global(type_subtype(type), values[1], 1);
      break;

   case 1:
      machine->configure_key(type_subtype(type), values[1], values[2], 1);
      break;

   case 2:
      machine->configure_twins(type_subtype(type), values[1], values[2], values[3], 1);
   case 3:
      // special requests ....
      break;
   default:
      machine->mdb("%s: invalid option %d\n", __func__, subtype_n_args(type));
      ;
   }
   /* return client->noClientException; */
   return 0;
}


/*todo: int*/
void
machine_command(ClientPtr client, PluginInstance* plugin, int cmd, int data1,
                int data2, int data3, int data4)
{
  DB("%s cmd %d, data %d ...\n", __func__, cmd, data1);
  switch (cmd)
    {
    case fork_client_dump_keys:
      /* DB("%s %d %.3s\n", __func__, len, data); */
      dump_last_events_to_client(plugin, client, data1);
      break;
    default:
      DB("%s Unknown command!\n", __func__);
      break;
      /* What XReply to send?? */
    }
}
