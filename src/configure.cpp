// In this file: processing requests to configure the plugin.
// see  struct _DevicePluginRec in ./fork-plugin.cpp

// It accesses: ->config and ->max_last

#include "config.h"
#include "debug.h"


// bug these 2 ^ v
#include "xmachine.h"

#include "configure.h"
#include "history.h"

/* something to define NULL */
extern "C"
{
#include "fork_requests.h"
#undef max
#undef min
}


/* fixme:  where is the documentation: fork_requests.h ? */
static int
machine_configure_twins (const machineRec* machine, int type, KeyCode key, KeyCode twin,
                         int value, Bool set)
{
   switch (type) {
      case fork_configure_total_limit:
         if (set)
            machine->config->verification_interval[key][twin] = value;
         else
            return machine->config->verification_interval[key][twin];
         break;
      case fork_configure_overlap_limit:
         if (set)
            machine->config->overlap_tolerance[key][twin] = value;
         else return machine->config->overlap_tolerance[key][twin];
         break;
      default:
         machine->mdb("%s: invalid type %d\n", __func__, type);;
   }
   return 0;
}


static int
machine_configure_key(const machineRec* machine, int type, KeyCode key, int value, Bool set)
{
   machine->mdb("%s: keycode %d -> value %d, function %d\n", __func__, key, value, type);

   switch (type)
      {
      case fork_configure_key_fork:
         if (set)
            machine->config->fork_keycode[key] = value;
         else return machine->config->fork_keycode[key];
         break;
      case fork_configure_key_fork_repeat:
         if (set)
            machine->config->fork_repeatable[key] = value;
         else return machine->config->fork_repeatable[key];
         break;
      default:
         machine->mdb("%s: invalid option %d\n", __func__, value);
         ;
      }
   return 0;
}


static int
machine_configure_global(PluginInstance* plugin, machineRec* machine, int type,
                         int value, Bool set)
{
   const auto fork_configuration = machine->config;

   switch (type){
   case fork_configure_overlap_limit:
      if (set)
         fork_configuration->overlap_tolerance[0][0] = value;
      else
         return fork_configuration->overlap_tolerance[0][0];
      break;

   case fork_configure_total_limit:
      if (set)
         fork_configuration->verification_interval[0][0] = value;
      else return fork_configuration->verification_interval[0][0];
      break;

   case fork_configure_clear_interval:
      if (set)
         fork_configuration->clear_interval = value;
      else return fork_configuration->clear_interval;
      break;

   case fork_configure_repeat_limit:
      if (set)
         fork_configuration->repeat_max = value;
      else
         return fork_configuration->repeat_max;
      break;

   case fork_configure_repeat_consider_forks:
      if (set)
         fork_configuration->consider_forks_for_repeat = value;
      return fork_configuration->consider_forks_for_repeat;
   case fork_configure_last_events:
      if (set)
         machine->set_last_events_count(value);
      else
         return machine->max_last; // mmc!
      break;

   case fork_configure_debug:
      if (set)
         {
            //  here we force, rather than using MDB !
            DB("fork_configure_debug set: %d -> %d\n", machine->config->debug,
                value);
            fork_configuration->debug = value;
         }
      else
         {
            machine->mdb("fork_configure_debug get: %d\n", fork_configuration->debug);
            return fork_configuration->debug; // (Bool) ?True:FALSE
         }
      break;

   case fork_server_dump_keys:
       machine->dump_last_events(xorg_event_dumper(plugin));
      break;

      // mmc: this is special:
   case fork_configure_switch:
      assert (set);

      machine->mdb("fork_configure_switch: %d\n", value);
      machine->switch_config(value);
      break;

   default:
      machine->mdb("%s: invalid option %d\n", __func__, value);
      ;
   }
   return 0;
}


// todo: make it inline functions
#define subtype_n_args(t)   (t & 3)
#define type_subtype(t)     (t >> 2)


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
           return_config[0]= machine_configure_global(plugin, machine,
                                                      type_subtype(type), 0, 0);
           break;
   case 1:
           return_config[0]= machine_configure_key(machine, type_subtype(type),
                                                   values[1], 0, 0);
           break;
   case 2:
           return_config[0]= machine_configure_twins(machine, type_subtype(type),
                                                     values[1], values[2], 0, 0);
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
      machine_configure_global(plugin, machine, type_subtype(type), values[1], 1);
      break;

   case 1:
      machine_configure_key(machine, type_subtype(type), values[1], values[2], 1);
      break;

   case 2:
      machine_configure_twins(machine, type_subtype(type), values[1], values[2],
                              values[3], 1);
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
