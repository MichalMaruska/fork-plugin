/*
 * Copyright (C) 2003-2005-2010-2024 Michal Maruska <mmaruska@gmail.com>
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



/* This is how it works:
 * We have a `state' and 3 queues:
 *
 *  output Q  |   internal Q    | input Q
 *  waits for |
 *  thaw         Oxxxxxx        |  yyyyy
 *               ^ forked?
 *
 * We push at the end of input Q.  Then we pop from that Q and push on
 * Internal where we determine for 1 event, if forked/non-forked.
 *
 * Then we push on the output Q. At that moment, we also restart: all
 * from internal Q is returned/prepended to the input Q.
 */

extern "C" {
#include <fork_requests.h>
}

#include "config.h"
#include "debug.h"

#include "configure.h"
#include "history.h"
#include "machine.h"
#include "memory.h"
#include "xorg.h"

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

// template instantiation:
using machineRec = forkingMachine<KeyCode, Time>;
template class forkingMachine<KeyCode, Time>;

#define plugin_machine(plugin) ((machineRec*)(plugin->data))
// used elsewhere :(

enum keycodes {
    zero = 19,
    one = 10,
    pc_break = 110,
    PAUSE = 127,
    key_l = 46,
};

/*
 * React to some `hot_keys':
 * Pause  Pause  -> dump
 */
static bool // return @true if config-mode continues.
handle_config_key(PluginInstance* plugin, const InternalEvent *event)
{
    // I could use optional
    static KeyCode key_to_fork = 0;         //  what key we want to configure
    machineRec* machine;

    if (press_p(event))
        switch (auto keycode = detail_of(event); keycode) {
            case keycodes::pc_break:
                machine = plugin_machine(plugin);
                machine->lock();
                dump_last_events(plugin);
                machine->unlock();
                break;
            case keycodes::zero:
                machine = plugin_machine(plugin);
                machine->lock();
                machine->switch_config(0); // current ->toggle ?
                machine->unlock();

                /* fixme: but this is default! */
                machine->forkActive[keycode] = 0; /* ignore the release as well. */
                break;
            case keycodes::one:
                machine = plugin_machine(plugin);

                machine->lock();
                machine->switch_config(1); // current ->toggle ?
                machine->unlock();
                machine->forkActive[keycode] = 0;
                break;

            case keycodes::key_l:
                machine = plugin_machine(plugin);
                machine->lock();
                ErrorF("%s: toggle debug\n", __func__);
                machine->config->debug = (machine->config->debug? 0: 1);
                machine->unlock();
                break;
            default:            /* todo: remove this: */
                // so press BREAK FROM TO
                // to have FROM fork to TO
                if (key_to_fork == 0) {
                    key_to_fork = keycode;
                } else {
                    machineRec* machine = plugin_machine(plugin);
                    machine->config->fork_keycode[key_to_fork] = keycode;
                    key_to_fork = 0;
                }
        };
    return true;
}

static bool   // return:  true if handled & should be skipped
filter_config_key_maybe(PluginInstance* const plugin, const InternalEvent* const event)
{
    static bool config_mode = false; // While the Pause key is down.
    static Time last_press_time = 0;

    if (config_mode) {
#if DEBUG > 3
        ErrorF("in config_mode ...\n");
#endif
        static bool latch = false;
        // [21/10/04]  I noticed, that some (non-plain ps/2) keyboard generate
        // the release event at the same time as press.
        // So, to overcome this limitation, I detect this short-lasting `down' &
        // take the `next' event as in `config_mode'   (latch)

        if ((detail_of(event) == keycodes::PAUSE) && release_p(event)) { //  fake ?
            if ( (time_of(event) - last_press_time) < 30) // fixme: configurable!
            {
                ErrorF("the key seems buggy, tolerating %" TIME_FMT ": %d! .. & latching config mode\n",
                       time_of(event), (int)(time_of(event) - last_press_time));
                latch = true;
                return true;
            }
#if DEBUG > 3
            ErrorF("exiting config_mode\n");
#endif
            config_mode = false;
            // fixme: key_to_fork = 0;
            ErrorF("dumping (%s) %" TIME_FMT ": %d!\n",
                   plugin->device->name,
                   time_of(event), (int)(time_of(event) - last_press_time));
            // todo: send a message to listening clients.
            dump_last_events(plugin);

        } else {
            last_press_time = 0;
            config_mode = handle_config_key(plugin, event);
            if (latch) {
                config_mode = latch = false;
            };
        }
    }
    // `Dump'
#if DEBUG > 3
    // ErrorF("%s: %p\n", __func__, event);
    ErrorF("%s: type %s\n", __func__, event_type_brief(event));
    ErrorF("%s: keycode: %d\n", __func__, detail_of(event));
    // ErrorF("%s: %d\n", __func__, event->device_event.detail.key);
#endif

        /* wait for the next and act ? but start w/ printing the last events: */
    {
        last_press_time = time_of(event);
#if DEBUG > 3
        ErrorF("entering config_mode & discarding the event: %" TIME_FMT "!\n", last_press_time);
#endif
        config_mode = true;

        /* fixme: should I update the ->down bitarray? */
        return true;
    } else
        last_press_time = 0;
#if DEBUG > 3
    ErrorF("%s: end\n", __func__);
#endif
    return false;
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
    Time machine_time = machine->next_decision_time();
    plugin->wakeup_time =
        // fixme:  but ZERO has certain meaning!
        // this is wrong: if machine waits, it cannot pass to the next-plugin!
        first_non_zero(machine_time, plugin->next->wakeup_time);

    // || ( DEBUG > 1 )
    if (plugin->wakeup_time != 0) {
        if (machine_time != 0) {
            machine->mdb("%s %s wakeup_time = %d, next wants: %u, we %" TIME_FMT "\n",
                FORK_PLUGIN_NAME, __func__,
                (int)plugin->wakeup_time, (int)plugin->next->wakeup_time, machine_time);
        }
    }
}


static XorgEvent*
create_xorg_platform_event(InternalEvent *event, bool owner)
{
#if DEBUG > 1
    ErrorF("%s: %s\n", __func__, owner?"owner":"not owner");
#endif

    InternalEvent* qe;
    if (owner)
        qe = event;
    else {
        qe = (InternalEvent*)malloc(event->any.length);
        if (!qe) {
            ErrorF("%s: out-of-memory\n", __func__);
            return NULL;
        }

        memcpy(qe, event, event->any.length);
    }

#if DEBUG > 1
    DB("+++ accepted new event: %s\n", event_names[event->any.type - 2 ]);
#endif

    return new XorgEvent(qe);
}



/*  This is the handler for all key events.  Here we delay pushing them forward.
    it's a trampoline for the automaton.
    Should it return some Time?
*/
static Bool
ProcessEvent(PluginInstance* plugin, InternalEvent *event, const Bool owner)
{
    // ErrorF("%s: start %d %s\n", __func__, event->any.type, owner?"owner":"not owner");
    if ((event->any.type != ET_KeyPress) && (event->any.type != ET_KeyRelease)) {
        // ET_RawKeyPress
#if DEBUG > 1
        ErrorF("ignoring this type of event %d\n", event->any.type);
#endif
        goto exit_free;
    }

    // this could be a different plugin!
    if (filter_config_key_maybe(plugin, event)) {
        // should we update the XKB `down' array, to signal that the key is up/down?
        // todo: I should at least push the time of (plugin->next)!
        ErrorF("not passing this event to forking-machine\n");

        goto exit_free;
    };

    {
        // This is C++ code:
        const auto machine = plugin_machine(plugin);
        machine->check_unlocked();
        machine->lock();           // fixme: mouse must not interrupt us.
        {
            auto* ev = create_xorg_platform_event(event, owner);
            if (!ev) // memory problems
                goto exit_free;
            machine->accept_event(ev);
        }

        set_wakeup_time(plugin);
        machine->unlock();
    }
    // by now, if owner, we consumed the event.
    goto exit;

  exit_free:
    if (owner)
        free(event);

  exit:
    return PLUGIN_NON_FROZEN;
};

// external API
static Bool
step_in_time(PluginInstance* plugin, Time now)
{
    machineRec* machine = plugin_machine(plugin);
    machine->mdb("%s: %" TIME_FMT "\n", __func__, now);
    machine->lock();

    machine->step_in_time_locked(now); // possibly unlocks
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
    machine->mdb("%s @ time %u\n", __func__, (int)now);
    machine->check_unlocked();

    machine->lock();
    machine->step_in_time_locked(now); // possibly unlocks

    if (!plugin_frozen(plugin->next) && PluginClass(plugin->prev)->NotifyThaw)
    {
        /* thaw the previous! */
        set_wakeup_time(plugin);
        machine->unlock();
        machine->mdb("%s -- sending thaw Notify upwards!\n", __func__);
        /* fixme:  Tail-recursion! */
        PluginClass(plugin->prev)->NotifyThaw(plugin->prev, now);
        /* I could move now to the time of our event. */
        /* step_in_time_locked(plugin); */
    } else {
        machine->mdb("%s -- NOT sending thaw Notify upwards %s!\n", __func__,
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
            ErrorF("%s running, while the machine is locked!\n", __func__);
#endif
        /* else */
        machine->lock();
        /* bug: if we were frozen, then we have a sequence of keys, which
         * might be already released, so the head is not to be forked!
         */
        machine->step_by_force();
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
    DB("%s @%p\n", __func__, keybd);
    DB("%s @%p\n", __func__, static_cast<void *>(keybd->name));

    assert (strcmp(plugin_class->name, FORK_PLUGIN_NAME) == 0);
    PluginInstance* plugin = (PluginInstance*) malloc(sizeof(PluginInstance));
    plugin->pclass = plugin_class;
    plugin->device = keybd;
    plugin->frozen = FALSE;


    ErrorF("%s: constructing the machine. Version %d (official release: %s)\n",
           __func__, PLUGIN_VERSION, VERSION_STRING);

    auto* xorg = new XOrgEnvironment(keybd, plugin);
    auto* const forking_machine = new machineRec(xorg);
    forking_machine->create_configs();

    // set_wakeup_time(plugin, 0);
    plugin->wakeup_time = 0;

    forking_machine->unlock();

    plugin->data = static_cast<void *>(forking_machine);
    //
    ErrorF("%s:keybd: next %p private %p on: %d\n", __func__, keybd->next, keybd->cpublic.devicePrivate, keybd->cpublic.on);

    ErrorF("%s:keybd: coreEvents %d, size %zd %zd\n", __func__, keybd->coreEvents, sizeof(Atom), sizeof(CARD32));
    ErrorF("%s:@%s returning %d\n", __func__, keybd->name, Success);

    AddCallback(&DeviceEventCallback, reinterpret_cast<CallbackProcPtr>(mouse_call_back), (void*) plugin);

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
    const auto machine = plugin_machine(plugin);
    machine->lock();
    machine->mdb("%s: what to do?\n", __func__);
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
    // machine->mdb("%s: what to do?\n", __func__);
    return 1;
}


template<>
int ForkConfiguration<KeyCode, Time>::config_counter = 0;


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
  ErrorF("%s: %s version %d\n", __func__, FORK_PLUGIN_NAME, PLUGIN_VERSION);

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
    ErrorF("%s: %s\n", __func__, VERSION); /* impossible */
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
