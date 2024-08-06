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

#if USE_LOCKING
#define CHECK_LOCKED(m)   assert((m)->lock)
#define CHECK_UNLOCKED(m) assert((m)->lock == 0)
// might be: CHECK_UNLOCKED(m)      ((m)->lock==1)

#define LOCK(m)    (m)->lock=1
#define UNLOCK(m)  (m)->lock=0
#else

#error "define locking macros!"
// m UNUSED
#define CHECK_LOCKED(m) while (0) {}
#define CHECK_UNLOCKED(m) while (0) {}
#define LOCK(m)  while (0) {}
#define UNLOCK(m)while (0) {}

#endif


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

/* used only for debugging */
char const *machineRec::state_description[]={
    "normal",
    "suspect",
    "verify",
    "deactivated",
    "activated"
};

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
inline void
hand_over_event_to_next_plugin(InternalEvent *event, PluginInstance* plugin)
{
    PluginInstance* next = plugin->next;

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


/* The machine is locked here:
 * Push as many as possible from the OUTPUT queue to the next layer */
static void
try_to_output(PluginInstance* plugin)
{
    machineRec* machine = plugin_machine(plugin);

    CHECK_LOCKED(machine);

    machineRec::list_with_tail &queue = machine->output_queue;
    PluginInstance* next = plugin->next;

    MDB(("%s: Queues: output: %d\t internal: %d\t input: %d \n", __FUNCTION__,
         queue.length (),
         machine->internal_queue.length (),
         machine->input_queue.length ()));

    while((!plugin_frozen(next)) && (!queue.empty ())) {
        key_event* ev = queue.pop();

        machine->last_events->push_back(make_archived_events(ev));
        InternalEvent* event = ev->event;
        mxfree(ev, sizeof(key_event));

        UNLOCK(machine);
        hand_over_event_to_next_plugin(event, plugin);
        LOCK(machine);
    };

    // interesting: after handing over, the NEXT might need to be refreshed.
    // if that plugin is gone. todo!

    if (!plugin_frozen(next)) {
        // we should push the time!
        Time now;
        if (!queue.empty()) {
            now = time_of(queue.front()->event);
        } else if (!machine->internal_queue.empty()) {
            now = time_of(machine->internal_queue.front()->event);
        } else if (!machine->input_queue.empty()) {
            now = time_of(machine->input_queue.front()->event);
        } else {
            now = machine->current_time;
        }

        if (now) {
            // this can thaw, freeze,?
            UNLOCK(machine);
            PluginClass(plugin->next)->ProcessTime(plugin->next, now);
            LOCK(machine);
        }

    }
    if (!queue.empty ())
        MDB(("%s: still %d events to output\n", __FUNCTION__, queue.length ()));
}

// Another event has been determined. So:
// todo:  possible emit a (notification) event immediately,
// ... and push the event down the pipeline, when not frozen.
static void
output_event(key_event* ev, PluginInstance* plugin)
{
    assert(ev->event);
    machineRec* machine = plugin_machine(plugin);
    machine->output_queue.push(ev);
    try_to_output(plugin);
};


/* note, that after this EV could point to a deallocated memory!
 * bad macro: evaluates EV twice. */
#define EMIT_EVENT(ev) {output_event((ev), plugin); ev=NULL;}

/**
 * Operations on the machine
 * fixme: should it include the `self_forked' keys ?
 * `self_forked' means, that i decided to NOT fork. to mark this decision
 * (for when a repeated event arrives), i fork it to its own keycode
 */



/* The Static state = configuration.
 * This is the matrix with some Time values:
 * using the fact, that valid KeyCodes are non zero, we use
 * the 0 column for `code's global values

 * Global      xxxxxxxx unused xxxxxx
 * key-wise   per-pair per-pair ....
 * key-wise   per-pair per-pair ....
 * ....
 */

inline Bool
forkable_p(fork_configuration* config, KeyCode code)
{
    return (config->fork_keycode[code]);
}


/* Now the operations on the Dynamic state */

/* Return the keycode into which CODE has forked _last_ time.
   Returns code itself, if not forked. */
inline Bool
key_forked(machineRec *machine, KeyCode code)
{
    return (machine->forkActive[code]);
}


inline void
change_state(machineRec* machine, fork_state_t new_state)
{
    machine->state = new_state;
    MDB((" --->%s[%dm%s%s\n", escape_sequence, 32 + new_state,
         machineRec::state_description[new_state], color_reset));
}

void
machineRec::reverse_slice(list_with_tail &pre, list_with_tail &post)
{
    // Slice with a reversed semantic:
    // A.slice(B) --> ()  (AB)
    // traditional is (AB) ()
    pre.slice(post);
    pre.swap(post);
}

#define final_state_p(state)  ((state == st_deactivated) || (state == st_activated))

void
machineRec::rewind_machine()
{
    assert (final_state_p(this->state));

    /* reset the machine */
    mdb("== Resetting the fork machine (internal %d, input %d)\n",
         this->internal_queue.length (),
         this->input_queue.length ());

    change_state(this,st_normal);
    this->verificator = 0;

    if (!(this->internal_queue.empty())) {
        reverse_slice(this->internal_queue, this->input_queue);
        mdb("now in input_queue: %d\n", this->input_queue.length ());
    }
};


/* Fork the 1st element on the internal_queue. Remove it from the queue
 * and push to the output_queue.
 *
 * todo: Should I have a link back from machine to the plugin? Here useful!
 * todo:  do away with the `forked_key' argument --- it's useless!
 */
void
machineRec::activate_fork(PluginInstance* plugin)
{
    list_with_tail &queue = this->internal_queue;
    assert(!queue.empty());

    key_event* ev = queue.pop();

    KeyCode forked_key = detail_of(ev->event);
    // assert (forked_key == machine->suspect)

    /* change the keycode, but remember the original: */
    ev->forked =  forked_key;
    this->forkActive[forked_key] =
        ev->event->device_event.detail.key = this->config->fork_keycode[forked_key];

    change_state(this, st_activated);
    mdb("%s suspected: %d-> forked to: %d,  internal queue is long: %d, %s\n", __FUNCTION__,
         forked_key,
         this->config->fork_keycode[forked_key],
         this->internal_queue.length (),
         this->describe_machine_state()
         );
    this->rewind_machine();

    EMIT_EVENT(ev);
}


/*
 * Called by mouse button press processing.
 * Make all the forkable (pressed)  forked! (i.e. confirm them all)
 *
 * If in Suspect or Verify state, force the fork. (todo: should be configurable)
 */
void
machineRec::step_fork_automaton_by_force(PluginInstance* plugin)
{
    if (this->state == st_normal) {
        return;
    }
    if (this->state == st_deactivated) {
        ErrorF("%s: BUG.\n", __FUNCTION__);
        return;
    }

    if (this->internal_queue.empty())
        return;

    /* so, the state is one of: verify, suspect or activated. */
    list_with_tail& queue = this->internal_queue;

    mdb("%s%s%s state: %s, queue: %d .... FORCE\n",
         fork_color, __FUNCTION__, color_reset,
         this->describe_machine_state(),
         queue.length ());

    this->decision_time = 0;
    this->activate_fork(plugin);
}

static void
do_enqueue_event(machineRec *machine, key_event *ev)
{
    machine->internal_queue.push(ev);
    // when replaying no need to show this:
    // MDB(("enqueue_event: time left: %u\n", machine->decision_time));
}

// so the ev proves, that the current event is not forked.
static void
do_confirm_non_fork_by(machineRec *machine, key_event *ev,
                       PluginInstance* plugin)
{
    assert(machine->decision_time == 0);
    change_state(machine, st_deactivated);
    machine->internal_queue.push(ev); //  this  will be re-processed!!


    key_event* non_forked_event = machine->internal_queue.pop();
    MDB(("this is not a fork! %d\n", detail_of(non_forked_event->event)));
    machine->rewind_machine();

    EMIT_EVENT(non_forked_event);
}

// so EV confirms fork of the current event.
static void
do_confirm_fork(machineRec *machine, key_event *ev, PluginInstance* plugin)
{
    machine->decision_time = 0;

    /* fixme: ev is the just-read event. But that is surely not the head
       of queue (which is confirmed to fork) */
    MDB(("confirm:\n"));
    machine->internal_queue.push(ev);
    machine->activate_fork(plugin);
}

/*
  returns:
  state  (in the `machine')
  output_event   possibly, othewise 0
  machine->decision_time   ... for another timer.
*/

#define time_difference_less(start,end,difference)   (end < (start + difference))
#define time_difference_more(start,end,difference)   (end > (start + difference))


// return 0 ... elapsed, or time when will happen
Time
key_pressed_too_long(machineRec *machine, Time current_time)
{
    int verification_interval =
        machine->config->verification_interval_of(machine->suspect,
                                 // this can be 0 (& should be, unless)
                                 machine->verificator);
    Time decision_time = machine->suspect_time + verification_interval;

    MDB(("time: verification_interval = %dms elapsed so far =%dms\n",
         verification_interval,
         (int)(current_time - machine->suspect_time)));

    if (decision_time <= current_time)
        return 0;
    else
        return decision_time;
}


// return 0 if enough, otherwise the time when it will be enough/proving a fork.
Time
key_pressed_in_parallel(machineRec *machine, Time current_time)
{
    // verify overlap
    int overlap_tolerance = machine->config->overlap_tolerance_of(machine->suspect,
                                                                  machine->verificator);
    Time decision_time =  machine->verificator_time + overlap_tolerance;

    if (decision_time <= current_time) {
        return 0;
    } else {
        MDB(("time: overlay interval = %dms elapsed so far =%dms\n",
             overlap_tolerance,
             (int) (current_time - machine->verificator_time)));

        MDB(("suspected = %d, verificator %d. Times: overlap %lu, "
             "still needed: %lu (ms)\n", machine->suspect, machine->verificator,
             current_time - machine->verificator_time,
             decision_time - current_time));

        return decision_time;
    }
}


static bool
step_fork_automaton_by_time(machineRec *machine, PluginInstance* plugin,
                            Time current_time)
{
    // confirm fork:
    int reason;
    MDB(("%s%s%s state: %s, queue: %d, time: %u key: %d\n",
         fork_color, __FUNCTION__, color_reset,
         machine->describe_machine_state(),
         machine->internal_queue.length (), (int)current_time,
         machine->suspect));

    /* First, I try the simple (fork-by-one-keys).
     * If that works, -> fork! Otherwise, I try w/ 2-key forking, overlapping.
     */

    if (0 == (machine->decision_time =
              key_pressed_too_long(machine, current_time))) {
        reason = machineRec::reason_total;
        machine->activate_fork(plugin);
        return true;
    };

    /* To test 2 keys overlap, we need the 2nd key: a verificator! */
    if (machine->state == st_verify) {
        // verify overlap
        Time decision_time = key_pressed_in_parallel(machine, current_time);

        if (decision_time == 0) {
            reason = machineRec::reason_overlap;
            machine->activate_fork(plugin);
            return true;
        }

        if (decision_time < machine->decision_time)
            machine->decision_time = decision_time;
    }
    // so, now we are surely in the replay_mode. All we need is to
    // get an estimate on time still needed:


    /* So, we were woken too early. */
    MDB(("*** %s: returning with some more time-to-wait: %lu"
         "(prematurely woken)\n", __FUNCTION__,
         machine->decision_time - current_time));
    return false;
}


#define MOUSE_EMULATION_ON(xkb) (xkb->ctrls->enabled_ctrls & XkbMouseKeysMask)


/** apply_event_to_{STATE} */

static Bool
mouse_emulation_on(DeviceIntPtr keybd)
{
    if (!keybd->key)
        return 0;

    XkbSrvInfoPtr xkbi= keybd->key->xkbInfo;
    XkbDescPtr xkb = xkbi->desc;
    return (MOUSE_EMULATION_ON(xkb));
}

static void
apply_event_to_normal(machineRec *machine, key_event *ev, PluginInstance* plugin)
{
    DeviceIntPtr keybd = plugin->device;

    InternalEvent* event = ev->event;
    KeyCode key = detail_of(event);
    Time simulated_time = time_of(event);

    fork_configuration* config = machine->config;

    assert(machine->internal_queue.empty());

    // if this key might start a fork....
    if (press_p(event) && forkable_p(config, key)
        /* fixme: is this w/ 1-event precision? (i.e. is the xkb-> updated synchronously) */
        /* todo:  does it have a mouse-related action? */
        && !(mouse_emulation_on(keybd))) {
        /* Either suspect, or detect .- trick to suppress fork */

        /* .- trick: by depressing/re-pressing the key rapidly, fork is disabled,
         * and AR is invoked */
#if DEBUG
        if ( !key_forked(machine, key) && (machine->last_released == key )) {
            MDB (("can we invoke autorepeat? %d  upper bound %d ms\n",
                  (int)(simulated_time - machine->last_released_time), config->repeat_max));
        }
#endif
        /* So, unless we see the .- trick, we do suspect: */
        if (!key_forked(machine, key) &&
            ((machine->last_released != key ) ||
             /*todo: time_difference_more(machine->last_released_time,simulated_time,
              * config->repeat_max) */
             (simulated_time - machine->last_released_time) >
             (Time) config->repeat_max)) {
            /* Emacs indenting bug: */
            change_state(machine, st_suspect);
            machine->suspect = key;
            machine->suspect_time = time_of(event);
            machine->decision_time = machine->suspect_time +
                machine->config->verification_interval_of(key, 0);
            do_enqueue_event(machine, ev);
            return;
        } else {
            // .- trick: (fixme: or self-forked)
            MDB(("re-pressed very quickly\n"));
            machine->forkActive[key] = key; // fixme: why??
            EMIT_EVENT(ev);
            return;
        };
    } else if (release_p(event) && (key_forked(machine, key))) {
        MDB(("releasing forked key\n"));
        // fixme:  we should see if the fork was `used'.
        if (config->consider_forks_for_repeat){
            // C-f   f long becomes fork. now we wanted to repeat it....
            machine->last_released = detail_of(event);
            machine->last_released_time = time_of(event);
        } else {
            // imagine mouse-button during the short 1st press. Then
            // the 2nd press ..... should not relate the the 1st one.
            machine->last_released = 0;
            machine->last_released_time = 0;
        }
        /* we finally release a (self-)forked key. Rewrite back the keycode.
         *
         * fixme: do i do this in other machine states?
         */
        event->device_event.detail.key = machine->forkActive[key];

        // this is the state (of the keyboard, not the machine).... better to
        // say of the machine!!!
        machine->forkActive[key] = 0;
        EMIT_EVENT(ev);
    } else {
        if (release_p (event)) {
            machine->last_released = detail_of(event);
            machine->last_released_time = time_of(event);
        };
        // pass along the un-forkable event.
        EMIT_EVENT(ev);
    };
};



/*  First (press)
 *  Second    <-- we are here.
 */
void
machineRec::apply_event_to_suspect(key_event *ev, PluginInstance* plugin)
{
    InternalEvent* event = ev->event;
    Time simulated_time = time_of(event);
    KeyCode key = detail_of(event);

    list_with_tail &queue = this->internal_queue;

    /* Here, we can
     * o refuse .... if suspected/forkable is released quickly,
     * o fork (definitively),  ... for _time_
     * o start verifying, or wait, or confirm (timeout)
     * todo: I should repeat a bi-depressed forkable.
     */
    assert(!queue.empty() && this->state == st_suspect);

    // todo: check the ranges (long vs. int)
    if ((this->decision_time =
         key_pressed_too_long(this, simulated_time)) == 0) {
        do_confirm_fork(this, ev, plugin);
        return;
    };

    /* So, we now have a second key, since the duration of 1 key
     * was not enough. */
    if (release_p(event)) {
        mdb("suspect/release: suspected = %d, time diff: %d\n",
             this->suspect, (int)(simulated_time  -  this->suspect_time));
        if (key == this->suspect) {
            this->decision_time = 0; // might be useless!
            do_confirm_non_fork_by(this, ev, plugin);
            return;
            /* fixme:  here we confirm, that it was not a user error.....
               bad synchro. i.e. the suspected key was just released  */
        } else {
            /* something released, but not verificating, b/c we are in `suspect',
             * not `confirm'  */
            do_enqueue_event(this, ev); // the `key'
            return;
        };
    } else {
        if (!press_p (event)) {
            // RawPress & Device events.
            do_enqueue_event(this, ev);
            return;
        }

        if (key == this->suspect) {
            /* How could this happen? Auto-repeat on the lower/hw level?
             * And that AR interval is shorter than the fork-verification */
            if (this->config->fork_repeatable[key]) {
                mdb("The suspected key is configured to repeat, so ...\n");
                this->forkActive[this->suspect] = this->suspect;
                this->decision_time = 0;
                do_confirm_non_fork_by(this, ev, plugin);
                return;
            } else {
                // fixme: this keycode is repeating, but we still don't know what to do.
                // ..... `discard' the event???
                // fixme: but we should recalc the decision_time !!
                return;
            }
        } else {
            // another key pressed
            change_state(this, st_verify);
            this->verificator_time = simulated_time;
            this->verificator = key; /* if already we had one -> we are not in this state!
                                           if the verificator becomes a modifier ?? fixme:*/
            // verify overlap
            Time decision_time = key_pressed_in_parallel(this, simulated_time);

            // well, this is an abuse ... this should never be 0.
            if (decision_time == 0) {
                mdb("absurd\n"); // this means that verificator key verifies immediately!
            }
            if (decision_time < this->decision_time)
                this->decision_time = decision_time;

            do_enqueue_event(this, ev);
            return;
        };
    }
}


/*
 * first
 * second
 * third  < we are here now.
 * ???? how long?
 * second Released.
 * So, already 2 keys have been pressed, and still no decision.
 * Now we have the 3rd key.
 *  We wait only for time, and for the release of the key */
static void
apply_event_to_verify(machineRec *machine, key_event *ev, PluginInstance* plugin)
{
    InternalEvent* event = ev->event;
    Time simulated_time = time_of(event);
    KeyCode key = detail_of(event);

    /* We pressed the forkable key, and another one (which could possibly
       use the modifier). Now, either the forkable key was intended
       to be `released' before the press of the other key (and we have an
       error due to mis-synchronization), or in fact, the forkable
       was actually `used' as a modifier.

       This should not be fork:
       I----I
       E--E
       This should be a fork:
       I-----I
       E--E
       Motivation:  we want to press the modifier for short time (simultaneously
       pressing other keys). But sometimes writing quickly, we
       press before we release the previous letter. We handle this, ignoring
       a short overlay. I.e. we wait for the verification key
       to be down in parallel for at least X ms.
       There might be a matrix of values! How to train it?
    */

    /* As before, in the suspect case, we check the 1-key timeout ? But this time,
       we have the 2 key, and we can have a more specific parameter:  Some keys
       are slow to release, when we press a specific one afterwards. So in this case fork slower!
    */

    if ((machine->decision_time = key_pressed_too_long(machine, simulated_time)) == 0)
    {
        do_confirm_fork(machine, ev, plugin);
        return;
    }

    /* now, check the overlap of the 2 first keys */
    Time decision_time = key_pressed_in_parallel(machine, simulated_time);

    // well, this is an abuse ... this should never be 0.
    if (decision_time == 0)
    {
        do_confirm_fork(machine, ev, plugin);
        return;
    }
    if (decision_time < machine->decision_time)
        machine->decision_time = decision_time;


    if (release_p(event) && (key == machine->suspect)){ // fixme: is release_p(event) useless?
        MDB(("fork-key released on time: %dms is a tolerated error (< %lu)\n",
             (int)(simulated_time -  machine->suspect_time),
             machine->config->verification_interval_of(machine->suspect,
                                      machine->verificator)));
        machine->decision_time = 0; // useless fixme!
        do_confirm_non_fork_by(machine, ev, plugin);

    } else if (release_p(event) && (machine->verificator == key)){
        // todo: we might be interested in percentage, Then here we should do the work!

        // we should change state:
        change_state(machine,st_suspect);
        machine->verificator = 0;   // we _should_ take the next possible verificator
        do_enqueue_event(machine, ev);
    } else {               // fixme: a (repeated) press of the verificator ?
        // fixme: we pressed another key: but we should tell XKB to repeat it !
        do_enqueue_event(machine, ev);
    };
}


/* apply event EV to (state, internal-queue, time).
 * This can append to the OUTPUT-queue
 * sets: `decision_time'
 *
 * input:
 *   internal-queue  <+      input-queue
 *                   ev
 * output:
 *   either the ev  is pushed on internal_queue, or to the output-queue
 *   the head of internal_queue may be pushed to the output-queue as well.
 */
void
machineRec::step_fork_automaton_by_key(key_event *ev, PluginInstance* plugin)
{
    assert (ev);

    DeviceIntPtr keybd = plugin->device;

    InternalEvent* event = ev->event;
    KeyCode key = detail_of(event);

    /* please, 1st change the state, then enqueue, and then EMIT_EVENT.
     * fixme: should be a function then  !!!*/

    list_with_tail &queue = this->internal_queue;

    // this->decision_time = 0;


#if DDX_REPEATS_KEYS || 1
    /* `quick_ignore': I want to ignore _quickly_ the repeated forked modifiers. Normal
       modifier are ignored before put in the X input pipe/queue This is only if the
       lower level (keyboard driver) passes through the auto-repeat events. */

    if ((key_forked(this, key)) && press_p(event)
        && (key != this->forkActive[key])) // not `self_forked'
    {
        mdb("%s: the key is forked, ignoring\n", __FUNCTION__);
        mxfree(ev->event, ev->event->any.length);
        mxfree(ev, sizeof(key_event));
        return;
    }
#endif

    // A currently forked keycode cannot be (suddenly) pressed 2nd time. But any pressed
    // key cannot be pressed once more:
    // assert (release_p(event) || (key < MAX_KEYCODE && this->forkActive[key] == 0));

#if DEBUG
    /* describe the (state, key) */
    if (keybd->key)
    {
    XkbSrvInfoPtr xkbi= keybd->key->xkbInfo;
    KeySym *sym = XkbKeySymsPtr(xkbi->desc,key);
    if ((!sym) || (! isalpha(* (unsigned char*) sym)))
        sym = (KeySym*) " ";
    mdb("%s%s%s state: %s, queue: %d, event: %d %s%c %s %s\n",
         info_color,__FUNCTION__,color_reset,
         this->describe_machine_state(),
         queue.length (),
         key, key_color, (char)*sym, color_reset, event_type_brief(event));
    }
#endif

    switch (this->state) {
        case st_normal:
            apply_event_to_normal(this, ev, plugin);
            return;
        case st_suspect:
        {
            this->apply_event_to_suspect(ev, plugin);
            return;
        }
        case st_verify:
        {
            apply_event_to_verify(this, ev, plugin);
            return;
        }
        default:
            mdb("----------unexpected state---------\n");
    }
}


/*
 * Take from input_queue, + the current_time + force   -> run the machine.
 * After that you have to:   cancel the timer!!!
 */
static void
try_to_play(PluginInstance* plugin, Bool force)
{
    machineRec *machine = plugin_machine(plugin);

    machineRec::list_with_tail &input_queue = machine->input_queue;

    MDB(("%s: next %s: internal %d, input: %d\n", __FUNCTION__,
         (plugin_frozen(plugin->next)?"frozen":"NOT frozen"),
         machine->internal_queue.length (),
         input_queue.length ()));


    while (!plugin_frozen(plugin->next)) {

        if (! input_queue.empty()) {
            key_event *ev = input_queue.pop();
            // if time is enough...
            machine->step_fork_automaton_by_key(ev, plugin);
        } else {
            // at the end ... add the final time event:
            if (machine->current_time && (machine->state != st_normal)) {
                // was final_state_p
                if (!step_fork_automaton_by_time(machine, plugin,
                                                 machine->current_time))
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
    if (((machineRec*) plugin_machine(plugin))->config->debug) {
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
    MDB(("%s:\n", __FUNCTION__));

    /* is this necessary?   I think not: if the next plugin was frozen,
     * and now it's not, then it must have warned us that it thawed */
    try_to_output(plugin);

    /* push the time ! */
    try_to_play(plugin, FALSE);

    /* i should take the minimum of time and the time of the 1st event in the
       (output) internal queue */
    if (machine->internal_queue.empty() && machine->input_queue.empty()
        && !plugin_frozen(plugin->next))
    {
        UNLOCK(machine);
        /* might this be invoked several times?  */
        PluginClass(plugin->next)->ProcessTime(plugin->next, machine->current_time);
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
    try_to_output(plugin);
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
#else
    forking_machine = new(machineRec);
#endif
    forking_machine->max_last = 100;
    forking_machine->last_events = new last_events_type(forking_machine->max_last);

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

    delete machine->last_events;
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
