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

#include "lock.h"

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

extern "C" {
#include "event_ops.h"
#include <xorg/xkbsrv.h>
#include <xorg/xf86Module.h>
}

const char* describe_key(DeviceIntPtr keybd, InternalEvent *event);


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
hand_over_event_to_next_plugin(InternalEvent *event, PluginInstance* plugin)
{
    PluginInstance* const next = plugin->next;

#if DEBUG
    if (((machineRec*) plugin_machine(plugin))->config->debug) {
        DeviceIntPtr keybd = plugin->device;
        DB(("%s<<<", keysym_color));
        DB(("%s", describe_key(keybd, event)));
        DB(("%s\n", color_reset));
    }
#endif
    assert (!plugin_frozen(next));
    memory_balance -= event->any.length;
    PluginClass(next)->ProcessEvent(next, event, TRUE); // we always own the event (up to now)
}

/*
 * Take from input_queue, + the current_time + force   -> run the machine.
 * After that you have to:   cancel the timer!!!
 */
static void
try_to_play(PluginInstance* plugin, Bool force)
{
    PluginInstance* const next = plugin->next;
    machineRec *machine = plugin_machine(plugin);

    machineRec::list_with_tail &input_queue = machine->input_queue;

    MDB(("%s: next %s: internal %d, input: %d\n", __FUNCTION__,
         (plugin_frozen(next)?"frozen":"NOT frozen"),
         machine->internal_queue.length (),
         input_queue.length ()));


    while (!plugin_frozen(next)) {

        if (! input_queue.empty()) {
            key_event *ev = input_queue.pop();
            // if time is enough...
            machine->step_fork_automaton_by_key(ev, plugin);
        } else {
            // at the end ... add the final time event:
            if (machine->current_time && (machine->state != st_normal)) {
                // was final_state_p
                if (!machine->step_fork_automaton_by_time(plugin, machine->current_time))
                    // If this time helped to decide -> machine rewound,
                    // we have to try again.
                    // Otherwise, this is the end for now:
                    return;
            } else if (force && (machine->state != st_normal)) {
                machine->step_fork_automaton_by_force(plugin);
            } else
                return;
        }
    }
    /* assert(!plugin_frozen(plugin->next)   --->
     *              queue_empty(machine->input_queue)) */
}


/* note: used only in configure.c!
 * Resets the machine, so as to reconsider the events on the
 * `internal' queue.
 * Apparently the criteria/configuration has changed!
 * Reasonably this is in response to a key event. So we are in Final state.
 */
void
replay_events(PluginInstance* plugin, Bool force)
{
    machineRec* machine= plugin_machine(plugin);
    MDB(("%s\n", __FUNCTION__));
    CHECK_LOCKED(machine);

    if (!machine->internal_queue.empty()) {
        machineRec::reverse_slice(machine->internal_queue, machine->input_queue);
    }
    machine->state = st_normal;
    // todo: what else?
    // last_released & last_released_time no more available.
    machine->last_released = 0; // bug!
    machine->decision_time = 0;     // we are not waiting for anything

    try_to_play(plugin, force);
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
                LOCK(machine);
                dump_last_events(plugin);
                UNLOCK(machine);
                break;
            case 19:
                machine = plugin_machine(plugin);
                LOCK(machine);
                machine_switch_config(plugin, machine,0); // current ->toggle ?
                UNLOCK(machine);

                /* fixme: but this is default! */
                machine->forkActive[detail_of(event)] = 0; /* ignore the release as well. */
                break;
            case 10:
                machine = plugin_machine(plugin);

                LOCK(machine);
                machine_switch_config(plugin, machine,1); // current ->toggle ?
                UNLOCK(machine);
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
                ErrorF("the key seems buggy, tolerating %lu: %d! .. & latching config mode\n",
                       time_of(event), (int)(time_of(event) - last_press_time));
                latch = 1;
                return -1;
            }
            config_mode = 0;
            // fixme: key_to_fork = 0;
            ErrorF("dumping (%s) %lu: %d!\n",
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
        ErrorF("entering config_mode & discarding the event: %lu!\n", last_press_time);
        config_mode = 1;

        /* fixme: should I update the ->down bitarray? */
        return -1;
    } else
        last_press_time = 0;
    return 0;
}

// NOW is useless
static void
set_wakeup_time(PluginInstance* plugin, Time now)
{
    machineRec* machine = plugin_machine(plugin);
    CHECK_LOCKED(machine);
    if ((machine->state == st_verify)
          || (machine->state == st_suspect))
        // we are indeed waiting, so take the minimum.
        plugin->wakeup_time =
            // fixme:  but ZERO has certain meaning!
            (plugin->next->wakeup_time == 0)?
            machine->decision_time :
            // MIN
            ((machine->decision_time < plugin->next->wakeup_time)
             ? machine->decision_time:
             plugin->next->wakeup_time);
    else
        plugin->wakeup_time = plugin->next->wakeup_time;
    // (machine->internal_queue.empty())? plugin->next->wakeup_time:0;

    MDB(("%s %s wakeup_time = %d, next wants: %u, we %lu\n", FORK_PLUGIN_NAME, __FUNCTION__,
         (int)plugin->wakeup_time, (int)plugin->next->wakeup_time,machine->decision_time));
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
    // the handle is deallocated in `try_to_output'
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
    DB(("+++ accepted new event: %s\n",
        event_names[event->any.type - 2 ]));
#endif
    ev->event = qe;
    ev->forked = 0;
    return ev;
}



/*  This is the handler for all key events.  Here we delay pushing them forward.
    it's a trampoline for the automaton.
    Should it return some Time?
*/
static void
ProcessEvent(PluginInstance* plugin, InternalEvent *event, Bool owner)
{
    DeviceIntPtr keybd = plugin->device;

    if (filter_config_key_maybe(plugin, event) < 0)
    {
        // fixme: I should at least push the time of (plugin->next)!
        if (owner)
            free(event);
        return;
    };
    machineRec* machine = plugin_machine(plugin);

    CHECK_UNLOCKED(machine);
    LOCK(machine);           // fixme: mouse must not interrupt us.

    machine->current_time = time_of(event);
    key_event* ev = create_handle_for_event(event, owner);
    if (!ev)			// memory problems
        // what to do with `event' !!
        return;

#if DEBUG
    if (machine->config->debug) {
        DB(("%s>>> ", key_io_color));
        DB(("%s", describe_key(keybd, ev->event)));
        DB(("%s\n", color_reset));
    }
#endif

    machine->input_queue.push(ev);
    try_to_play(plugin, FALSE);

    set_wakeup_time(plugin, machine->current_time);
    UNLOCK(machine);
};

// this is an internal call.
static void
step_in_time_locked(PluginInstance* plugin)
{
    machineRec* machine = plugin_machine(plugin);
    PluginInstance* const next = plugin->next;

    MDB(("%s:\n", __FUNCTION__));

    /* is this necessary?   I think not: if the next plugin was frozen,
     * and now it's not, then it must have warned us that it thawed */
    machine->try_to_output(plugin);

    /* push the time ! */
    try_to_play(plugin, FALSE);

    /* i should take the minimum of time and the time of the 1st event in the
       (output) internal queue */
    if (machine->internal_queue.empty() && machine->input_queue.empty()
        && !plugin_frozen(next))
    {
        UNLOCK(machine);
        /* might this be invoked several times?  */
        PluginClass(next)->ProcessTime(next, machine->current_time);
        LOCK(machine);
    }
    // todo: we could push the time before the first event in internal queue!
    set_wakeup_time(plugin, machine->current_time);
}

// external API
static void
step_in_time(PluginInstance* plugin, Time now)
{
    machineRec* machine = plugin_machine(plugin);
    MDB(("%s:\n", __FUNCTION__));
    LOCK(machine);
    machine->current_time = now;
    step_in_time_locked(plugin);
    UNLOCK(machine);
};


/* Called from AllowEvents, after all events from following plugins have been pushed: . */
static void
fork_thaw_notify(PluginInstance* plugin, Time now)
{
    machineRec* machine = plugin_machine(plugin);
    MDB(("%s @ time %u\n", __FUNCTION__, (int)now));

    LOCK(machine);
    machine->try_to_output(plugin);
    // is this correct?

    try_to_play(plugin, FALSE);

    if (!plugin_frozen(plugin->next) && PluginClass(plugin->prev)->NotifyThaw)
    {
        /* thaw the previous! */
        set_wakeup_time(plugin, machine->current_time);
        UNLOCK(machine);
        MDB(("%s -- sending thaw Notify upwards!\n", __FUNCTION__));
        /* fixme:  Tail-recursion! */
        PluginClass(plugin->prev)->NotifyThaw(plugin->prev, now);
        /* I could move now to the time of our event. */
        /* step_in_time_locked(plugin); */
    } else {
        MDB(("%s -- NOT sending thaw Notify upwards %s!\n", __FUNCTION__,
             plugin_frozen(plugin->next)?"next is frozen":"prev has not NotifyThaw"));
        UNLOCK(machine);
    }
}


/* For now this is called to many times, for different events.! */
static void
mouse_call_back(CallbackListPtr *, PluginInstance* plugin,
                DeviceEventInfoRec* dei)
{
    InternalEvent *event = dei->event;
    machineRec* machine = plugin_machine(plugin);

    if (event->any.type == ET_Motion)
    {
        if (machine->lock)
            ErrorF("%s running, while the machine is locked!\n", __FUNCTION__);
        /* else */
        LOCK(machine);
        /* bug: if we were frozen, then we have a sequence of keys, which
         * might be already released, so the head is not to be forked!
         */
        machine->step_fork_automaton_by_force(plugin);
        UNLOCK(machine);
    }
}


/* We have to make a (new) automaton: allocate default config,
 * register hooks to other devices,
 *
 * returns: erorr of Success. Should attach stuff by side effect ! */
extern "C"
{
PluginInstance*
make_machine(const DeviceIntPtr keybd, DevicePluginRec* plugin_class);
}

PluginInstance*
make_machine(const DeviceIntPtr keybd, DevicePluginRec* plugin_class)
{
    DB(("%s @%lu\n", __FUNCTION__, keybd));
    DB(("%s @%lu\n", __FUNCTION__, keybd->name));

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

#if 0
    forking_machine = (machineRec* )mmalloc(sizeof(machineRec));
    bzero(forking_machine, sizeof (machineRec));

    if (! forking_machine){
        ErrorF("%s: malloc failed (for forking_machine)\n",__FUNCTION__);
        // free all the previous ....!
        return NULL;              // BadAlloc
    }

    // now, if something goes wrong, we have to free it!!
    forking_machine->internal_queue.set_name(string("internal"));
    forking_machine->input_queue.set_name(string("input"));
    forking_machine->output_queue.set_name(string("output"));
    forking_machine->max_last = 100;
    forking_machine->last_events = new last_events_type(forking_machine->max_last);
#else
    forking_machine = new(machineRec);
#endif

    forking_machine->state = st_normal;
    forking_machine->last_released = 0;
    forking_machine->decision_time = 0;
    forking_machine->current_time = 0;
    // set_wakeup_time(plugin, 0);
    plugin->wakeup_time = 0;

    UNLOCK(forking_machine);

    for (int i=0;i<256;i++){                   // keycode 0 is unused!
        forking_machine->forkActive[i] = 0; /* 0 = not active */
    };

    config->debug = 1;
    forking_machine->config = config;

    plugin->data = (void*) forking_machine;
    ErrorF("%s:@%s returning %d\n", __FUNCTION__, keybd->name, Success);

    AddCallback(&DeviceEventCallback, (CallbackProcPtr) mouse_call_back, (void*) plugin);

    plugin_class->ref_count++;

    return plugin;
};


/* fixme!
   This is a wrong API: there is no guarantee we can do this.
   The pipeline can get frozen, and we have to wait on thaw.
   So, it's better to have a callback. */
static int
stop_and_exhaust_machine(PluginInstance* plugin)
{
    machineRec* machine = plugin_machine(plugin);
    LOCK(machine);
    MDB(("%s: what to do?\n", __FUNCTION__));
    // free all the stuff, and then:
    xkb_remove_plugin(plugin);
    return 1;
}


static int
destroy_machine(PluginInstance* plugin)
{
    machineRec* machine = plugin_machine(plugin);
    LOCK(machine);

    delete machine;
    // delete machine->last_events;
    DeleteCallback(&DeviceEventCallback, (CallbackProcPtr) mouse_call_back,
                   (void*) plugin);
    MDB(("%s: what to do?\n", __FUNCTION__));
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
          int		*errmin)
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
    _B(terminate,  destroy_machine)
  };
  plugin_class.ref_count = 0;
  xkb_add_plugin_class(&plugin_class);

  return &plugin_class;
}

// my old way of loading modules?
void __attribute__((constructor)) on_init()
{
    ErrorF("%s:\n", __FUNCTION__); /* impossible */
    fork_plug(NULL,NULL,NULL);
}

static pointer
SetupProc(pointer module, pointer options, int *errmaj, int *errmin)
{
    on_init();
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
    NULL
};

}
