/*
 * Copyright (C) 2003-2005-2010 Michal Maruska <mmaruska@gmail.com>
 * License:  Creative Commons Attribution-ShareAlike 3.0 Unported License
 */


/* How we operate:
 *
 *   ProcessEvent ->                                      /  step_by_key
 *                    change the `queue'   -> try_to_play --- step_by_time
 *                                                |       \  step_by_force
 *   Accept_time  ->                              |
 *                                               restart
 *   mouse_call_back
 *
 *   Thaw-notify:
 */


#define USE_LOCKING 1

/* What does the lock protect?  ... access to the  queues,state
 * mouse signal handler cannot just make "fork", while a key event is being analyzed.
 */


/* Locking is broken: but it's not used now:
 *
 *   ---> keyevent ->  xkb action -> mouse
 *                                     |
 *   prev   <----                 <---  thaw
 *        ->   process  \
 *               exits  /
 *             unlocks!
 *
 *
 *  lock is gone!! <- action */



/* This is how it works:
 * We have a `state' and 3 queues:
 *
 *  output Q  |   internal Q    | input Q
 *  waits for |
 *  thaw         Oxxxxxx        |  yyyyy
 *               ^ forked?
 *
 * We push at the end of input Q.  Then we pop from that Q and push on
 * Internal when we determine for 1 event, if forked/non-forked.
 *
 * Then we push on the output Q. At that moment, we also restart: all
 * from internal Q is returned/prepended to the input Q.
 */


#include "config.h"
#include "debug.h"

#include "configure.h"
#include "history.h"
#include "fork.h"
#include "memory.h"

extern "C" {
#include "event_ops.h"
#include <xorg/xkbsrv.h>
#include <xorg/xf86Module.h>
}

// is it available from somewhere else?
const char* event_names[] = {
    "KeyPress",
    "KeyRelease",
    "ButtonPress",
    "ButtonRelease",
    "Motion",
    "Enter",
    "Leave",
    // 9
    "FocusIn",
    "FocusOut",
    "ProximityIn",
    "ProximityOut",
    // 13
    "DeviceChanged",
    "Hierarchy",
    "DGAEvent",
    // 16
    "RawKeyPress",
    "RawKeyRelease",
    "RawButtonPress",
    "RawButtonRelease",
    "RawMotion",
    "XQuartz"
};


/* memory problems tracking: (used with mxfree & mmalloc) I observe this value. */
size_t memory_balance = 0;

/* Push the event to the next plugin. Ownership is handed over! */
void
hand_over_event_to_next_plugin(InternalEvent *event, PluginInstance* const nextPlugin)
{
    assert (!plugin_frozen(nextPlugin));
    memory_balance -= event->any.length;
    PluginClass(nextPlugin)->ProcessEvent(nextPlugin, event, TRUE); // we always own the event (up to now)
}

/*
 *  react to some `hot_keys':
 *  Pause  Pause  -> dump
 */
static int                      // return, if config-mode continues.
filter_config_key(PluginInstance* plugin,const InternalEvent *event)
{
    static KeyCode key_to_fork = 0;         //  what key we want to configure
    machineRec* machine;

    if (press_p(event))
        switch (detail_of(event)) {
            case 110:
                machine = plugin_machine(plugin);
                machine->lock();
                dump_last_events(plugin);
                machine->unlock();
                break;
            case 19:
                machine = plugin_machine(plugin);
                machine->lock();
                machine->switch_config(0); // current ->toggle ?
                machine->unlock();

                /* fixme: but this is default! */
                machine->forkActive[detail_of(event)] = 0; /* ignore the release as well. */
                break;
            case 10:
                machine = plugin_machine(plugin);

                machine->lock();
                machine->switch_config(1); // current ->toggle ?
                machine->unlock();
                machine->forkActive[detail_of(event)] = 0;
                break;
            default:            /* todo: remove this: */
                if (key_to_fork == 0){
                    key_to_fork = detail_of(event);
                } else {
                    machineRec* machine = plugin_machine(plugin);
                    machine->config->fork_keycode[key_to_fork] = detail_of(event);
                    key_to_fork = 0;
                }
            };
    // should we update the XKB `down' array, to signal that the key is up/down?
    return -1;
}

static int                             // return:  0  nothing  -1  skip it
filter_config_key_maybe(PluginInstance* plugin,const InternalEvent *event)
{
    static unsigned char config_mode = 0; // While the Pause key is down.
    static Time last_press_time = 0;

    if (config_mode)
    {
        static int latch = 0;
        // [21/10/04]  I noticed, that some (non-plain ps/2) keyboard generate
        // the release event at the same time as press.
        // So, to overcome this limitation, I detect this short-lasting `down' &
        // take the `next' event as in `config_mode'   (latch)

        if ((detail_of(event) == PAUSE_KEYCODE) && release_p(event)) { //  fake ?
            if ( (time_of(event) - last_press_time) < 30) // fixme: configurable!
            {
                ErrorF("the key seems buggy, tolerating %" TIME_FMT ": %d! .. & latching config mode\n",
                       time_of(event), (int)(time_of(event) - last_press_time));
                latch = 1;
                return -1;
            }
            config_mode = 0;
            // fixme: key_to_fork = 0;
            ErrorF("dumping (%s) %" TIME_FMT ": %d!\n",
                   plugin->device->name,
                   time_of(event), (int)(time_of(event) - last_press_time));
            // todo: send a message to listening clients.
            dump_last_events(plugin);
        }
        else {
            last_press_time = 0;
            if (latch) {
                config_mode = latch = 0;
            };
            config_mode = filter_config_key (plugin, event);
        }
    }
    // `Dump'
    if ((detail_of(event) == PAUSE_KEYCODE) && press_p(event))
        /* wait for the next and act ? but start w/ printing the last events: */
    {
        last_press_time = time_of(event);
        ErrorF("entering config_mode & discarding the event: %" TIME_FMT "!\n", last_press_time);
        config_mode = 1;

        /* fixme: should I update the ->down bitarray? */
        return -1;
    } else
        last_press_time = 0;
    return 0;
}

static Time
first_non_zero(Time a, Time b)
{
    return (a==0)?b:a;
}

// set plugin->wakeup_time
static void
set_wakeup_time(PluginInstance* plugin)
{
    machineRec* machine = plugin_machine(plugin);
    machine->check_locked();

    plugin->wakeup_time =
        // fixme:  but ZERO has certain meaning!
        // this is wrong: if machine waits, it cannot pass to the next-plugin!
        first_non_zero(machine->next_decision_time(), plugin->next->wakeup_time);

    // || ( DEBUG > 1 )
    if (plugin->wakeup_time != 0) {
        machine->mdb("%s %s wakeup_time = %d, next wants: %u, we %" TIME_FMT "\n", FORK_PLUGIN_NAME, __FUNCTION__,
                     (int)plugin->wakeup_time, (int)plugin->next->wakeup_time, machine->next_decision_time());
    }
}


static key_event*
create_handle_for_event(InternalEvent *event, bool owner)
{
    InternalEvent* qe;
    if (owner)
        qe = event;
    else {
        qe = (InternalEvent*)malloc(event->any.length);
        if (!qe)
        {
            // if we are out-of-memory, we probably cannot even process ErrorF, can we?
            ErrorF("%s: out-of-memory\n", __FUNCTION__);
            return NULL;
        }
    }
    // the handle is deallocated in `flush_to_next'
    key_event* ev = (key_event*)malloc(sizeof(key_event));
    if (!ev) {
        /* This message should be static string. otherwise it fails as well? */
        ErrorF("%s: out-of-memory, dropping\n", __FUNCTION__);
        if (!owner)
            mxfree (qe, event->any.length);
        return NULL;
    };

    memcpy(qe, event, event->any.length);
#if DEBUG > 1
    DB("+++ accepted new event: %s\n",
        event_names[event->any.type - 2 ]);
#endif
    ev->event = qe;
    ev->forked = 0;
    return ev;
}



/*  This is the handler for all key events.  Here we delay pushing them forward.
    it's a trampoline for the automaton.
    Should it return some Time?
*/
static Bool
ProcessEvent(PluginInstance* plugin, InternalEvent *event, Bool owner)
{
    DeviceIntPtr keybd = plugin->device;

    if (filter_config_key_maybe(plugin, event) < 0)
    {
        // fixme: I should at least push the time of (plugin->next)!
        if (owner)
            free(event);
        goto exit;
    };

    {
        machineRec* machine = plugin_machine(plugin);

        machine->check_unlocked();
        machine->lock();           // fixme: mouse must not interrupt us.

        {
            key_event* ev = create_handle_for_event(event, owner);
            if (!ev)			// memory problems
                // what to do with `event' !!
                goto exit;

            machine->log_event(ev, keybd);

            machine->accept_event(ev);
        }

        set_wakeup_time(plugin);
        machine->unlock();
    }

    // always:
  exit:
    return PLUGIN_NON_FROZEN;
};

// external API
static Bool
step_in_time(PluginInstance* plugin, Time now)
{
    machineRec* machine = plugin_machine(plugin);
    machine->mdb("%s:\n", __FUNCTION__);
    machine->lock();

    machine->step_in_time_locked(now);
    // todo: we could push the time before the first event in internal queue!
    set_wakeup_time(plugin);
    machine->unlock();

    return PLUGIN_NON_FROZEN;
};


/* Called from AllowEvents, after all events from the next plugin have been pushed. */
static void
fork_thaw_notify(PluginInstance* plugin, Time now)
{
    machineRec* machine = plugin_machine(plugin);
    machine->mdb("%s @ time %u\n", __FUNCTION__, (int)now);

    machine->lock();
    machine->step_in_time_locked(now);

    if (!plugin_frozen(plugin->next) && PluginClass(plugin->prev)->NotifyThaw)
    {
        /* thaw the previous! */
        set_wakeup_time(plugin);
        machine->unlock();
        machine->mdb("%s -- sending thaw Notify upwards!\n", __FUNCTION__);
        /* fixme:  Tail-recursion! */
        PluginClass(plugin->prev)->NotifyThaw(plugin->prev, now);
        /* I could move now to the time of our event. */
        /* step_in_time_locked(plugin); */
    } else {
        machine->mdb("%s -- NOT sending thaw Notify upwards %s!\n", __FUNCTION__,
             plugin_frozen(plugin->next)?"next is frozen":"prev has not NotifyThaw");
        machine->unlock();
    }
}


/* For now this is called to many times, for different events.! */
static void
mouse_call_back(CallbackListPtr *, PluginInstance* plugin,
                DeviceEventInfoRec* dei)
{
    InternalEvent *event = dei->event;
    if (event->any.type == ET_Motion)
    {

        machineRec* machine = plugin_machine(plugin);
#if 0
        // fixme:
        if (machine->mLock)
            ErrorF("%s running, while the machine is locked!\n", __FUNCTION__);
#endif
        /* else */
        machine->lock();
        /* bug: if we were frozen, then we have a sequence of keys, which
         * might be already released, so the head is not to be forked!
         */
        machine->step_fork_automaton_by_force();
        machine->unlock();
    }
}


/* We have to make a (new) automaton: allocate default config,
 * register hooks to other devices,
 *
 * returns: erorr of Success. Should attach stuff by side effect ! */

PluginInstance*
make_machine(const DeviceIntPtr keybd, DevicePluginRec* plugin_class)
{
    DB("%s @%lu\n", __FUNCTION__, keybd);
    DB("%s @%lu\n", __FUNCTION__, keybd->name);

    assert (strcmp(plugin_class->name, FORK_PLUGIN_NAME) == 0);

    PluginInstance* plugin = MALLOC(PluginInstance);
    plugin->pclass = plugin_class;
    plugin->device = keybd;
    plugin->frozen = FALSE;
    machineRec* forking_machine = NULL;

    // I create 2 config sets.  1 w/o forking.
    // They are numbered:  0 is the no-op.
    fork_configuration* config_no_fork = machine_new_config(); // configuration number 0
    config_no_fork->debug = 0;   // should be settable somehow.
    if (!config_no_fork)
    {
        return NULL;
    }

    fork_configuration* config = machine_new_config();
    if (!config)
    {
        free (config_no_fork);
        return NULL;
    }

    config->next = config_no_fork;
    // config->id = config_no_fork->id + 1;
    // so we start w/ config 1. 0 is empty and should not be modifiable

    ErrorF("%s: constructing the machine %d (official release: %s)\n",
           __FUNCTION__, PLUGIN_VERSION, VERSION_STRING);

    forking_machine = new machineRec(plugin);

    // set_wakeup_time(plugin, 0);
    plugin->wakeup_time = 0;

    // fixme: dangerous: this should be part of the ctor!
    config->debug = 1;
    forking_machine->config = config;

    forking_machine->unlock();

    plugin->data = (void*) forking_machine;
    ErrorF("%s:@%s returning %d\n", __FUNCTION__, keybd->name, Success);

    AddCallback(&DeviceEventCallback, (CallbackProcPtr) mouse_call_back, (void*) plugin);

    plugin_class->ref_count++;

    return plugin;
};


/* fixme!
   This is a wrong API: there is no guarantee we can do this.
   The pipeline can get frozen, and we have to wait on thaw.
   So, it's better to have a callback.

the plugin should not pass more events.
   */
static int
stop_and_exhaust_machine(PluginInstance* plugin)
{
    machineRec* machine = plugin_machine(plugin);
    machine->lock();
    machine->mdb("%s: what to do?\n", __FUNCTION__);
    // free all the stuff, and then:
    // machine->unlock();

    xkb_remove_plugin(plugin); // will this lead to destroy_plugin()?
    return 1;
}


static int
destroy_plugin(PluginInstance* plugin)
{
    machineRec* machine = plugin_machine(plugin);
    // should be locked from the STOP call?
    machine->lock();

    // delete machine->last_events;
    DeleteCallback(&DeviceEventCallback, (CallbackProcPtr) mouse_call_back,
                   (void*) plugin);

    delete machine;
    // machine->mdb("%s: what to do?\n", __FUNCTION__);
    return 1;
}



// This macro helps with providing
// initial value of struct- the member name is along the value.
#if __GNUC__
#define _B(name, value) value
#else
#define _B(name, value) name : value
#endif

extern "C" {

static void* /*DevicePluginRec* */
fork_plug(void          *options,
          int		*errmaj,
          int		*errmin,
          void* dynamic_module)
{
  ErrorF("%s: %s version %d\n", __FUNCTION__, FORK_PLUGIN_NAME, PLUGIN_VERSION);

  static struct _DevicePluginRec plugin_class = {
    // slot name,     value
    _B(name, FORK_PLUGIN_NAME),
    _B(instantiate, &make_machine),
    _B(ProcessEvent, ProcessEvent),
    _B(ProcessTime, step_in_time),
    _B(NotifyThaw, fork_thaw_notify),
    _B(config,    &machine_configure),
    _B(getconfig, ::machine_configure_get),
    _B(client_command, machine_command),
    _B(module, NULL),
    _B(ref_count, 0),
    _B(stop,       stop_and_exhaust_machine),
    _B(terminate,  destroy_plugin)
  };
  plugin_class.ref_count = 0;
  ErrorF("assigning %p\n", dynamic_module);

  plugin_class.module = dynamic_module;
  xkb_add_plugin_class(&plugin_class);

  return &plugin_class;
}

// my old way of loading modules?
#if 0
void __attribute__((constructor)) on_init()
{
    ErrorF("%s: %s\n", __FUNCTION__, VERSION); /* impossible */
    fork_plug(NULL,NULL,NULL);
}
#endif

static void*
SetupProc(void* module, pointer options, int *errmaj, int *errmin)
{
    ErrorF("%s %p\n", __func__, module);

    fork_plug(NULL,NULL,NULL, module);
    // on_init();
    return module;
}

#define PACKAGE_VERSION_MAJOR 1
#define PACKAGE_VERSION_MINOR 0
#define PACKAGE_VERSION_PATCHLEVEL 0

static XF86ModuleVersionInfo VersionRec = {
    "fork",
    MODULEVENDORSTRING,
    MODINFOSTRING1,
    MODINFOSTRING2,
    XORG_VERSION_CURRENT,
    PACKAGE_VERSION_MAJOR, PACKAGE_VERSION_MINOR, PACKAGE_VERSION_PATCHLEVEL,
    //
    ABI_CLASS_INPUT,
    ABI_INPUT_VERSION,
    MOD_CLASS_INPUT,
    {0, 0, 0, 0}
};

_X_EXPORT XF86ModuleData forkModuleData = {
    &VersionRec,
    &SetupProc,
    // teardown:
    NULL
};

}
