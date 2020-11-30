#define USE_LOCKING 1

#include "fork.h"
#include "memory.h"
#include "history.h"
// uses:
// void hand_over_event_to_next_plugin(InternalEvent *event, PluginInstance* nextPlugin);
#include "debug.h"


extern "C" {
#include "event_ops.h"

#include <xorg-server.h>
#include <xorg/xkbsrv.h>
#include <xorg/xf86Module.h>
}

static Bool
mouse_emulation_on(DeviceIntPtr keybd)
{
    if (!keybd->key) {
        ErrorF("%s: keybd is null!", __func__);
        return 0;
    }

    XkbSrvInfoPtr xkbi= keybd->key->xkbInfo;
    XkbDescPtr xkb = xkbi->desc;
    return (xkb->ctrls->enabled_ctrls & XkbMouseKeysMask);
}

//
//  pointer->  |  next|-> |   next| ->
//    ^            ^
//    |    or      |
//  return a _pointer_ to the pointer  on a searched-for item.

fork_configuration**
machineRec::find_configuration_n(int n)
{
    fork_configuration** config_p = &config;

    while (((*config_p)->next) && ((*config_p)->id != n))
    {
        ErrorF("%s skipping over %d\n", __FUNCTION__, (*config_p)->id);
        config_p = &((*config_p) -> next);
    }
    return ((*config_p)->id == n)? config_p: NULL;      // ??? &(config->next);
}

// and replay whatever is inside the machine!
// locked?
void
machineRec::switch_config(int id)
{

    ErrorF("%s %d\n", __FUNCTION__, id);
    fork_configuration** config_p = find_configuration_n(id);
    ErrorF("%s found\n", __FUNCTION__);

    // fixme:   `move_to_top'   find an element in a linked list, and move it to the head.
    if ((config_p)
        // useless:
        && (*config_p)
        && (*config_p != config))
    {
        DB("switching configs %d -> %d\n", config->id, id);

        fork_configuration* new_current = *config_p;
        //fixme: this sequence works at the beginning too!!!

        // remove from the list:
        *config_p = new_current->next; //   n-1 -> n + 1

        // reinsert at the beginning:
        new_current->next = config; //    n -> 1
        config = new_current; //     -> n

        DB("switched configs %d -> %d\n", config->id, id);
        replay_events(FALSE);
    } else {
        ErrorF("config remains %d\n", config->id);
    }
}

void machineRec::log_event(const key_event *event, DeviceIntPtr keybd)
{
#if 0 // fixme: DEBUG
            if (config->debug) {
                DB("%s<<<", keysym_color);
                DB("%s", describe_key(keybd, event->ev));
                DB("%s\n", color_reset);
            }
#endif
}

void
machineRec::log_state(const char* message) const
{
    mdb("%s%s%s state: %s, queue: %d .... %s\n",
        fork_color, __FUNCTION__, color_reset,
        describe_machine_state(),
        internal_queue.length (),
        message);
}

void
machineRec::log_state_and_event(const char* message, const key_event *ev)
{
    DeviceIntPtr keybd = mPlugin->device;
    InternalEvent* event = ev->event;
    KeyCode key = detail_of(event);

    if (keybd->key)
    {
        XkbSrvInfoPtr xkbi= keybd->key->xkbInfo;
        KeySym *sym = XkbKeySymsPtr(xkbi->desc, key);
        if ((!sym) || (! isalpha(* (unsigned char*) sym)))
            sym = (KeySym*) " ";

        mdb("%s%s%s state: %s, queue: %d, event: %d %s%c %s %s\n",
            info_color,message,color_reset,
            describe_machine_state(),
            internal_queue.length (),
            key,
            key_color, (char)*sym, color_reset,
            event_type_brief(event));
    }
}

void
machineRec::log_queues(const char* message)
{
    mdb("%s: Queues: output: %d\t internal: %d\t input: %d \n", message,
        output_queue.length (),
        internal_queue.length (),
        input_queue.length ());
}

/* The machine is locked here:
 * Push as many as possible from the OUTPUT queue to the next layer */
void
machineRec::flush_to_next()  // unlocks!
{
    // todo: lock in this scope only?
    check_locked();
    log_queues(__FUNCTION__);

    PluginInstance* const nextPlugin = mPlugin->next;
    while(!plugin_frozen(nextPlugin) && !output_queue.empty()) {
        key_event* ev = output_queue.pop();

        last_events->push_back(make_archived_event(ev));

        InternalEvent* event = ev->event;
        mxfree(ev, sizeof(key_event));

        // this block (hand_over_event_to_next_plugin) can re-enter into this
        // machine. fixme: it's not true -- it cannot!
        // 2020: it can!
        // so ... this is front_lock?
        {
            log_event(ev, mPlugin->device);
            unlock();
            hand_over_event_to_next_plugin(event, nextPlugin);
            lock();
        };
    }

    // interesting: after handing over, the nextPlugin might need to be refreshed.
    // if that plugin is gone. todo!

    // now we still have time:
    if (!plugin_frozen(nextPlugin)) {
        // we should push the time!
        Time now;
        if (!output_queue.empty()) {
            now = time_of(output_queue.front()->event);
        } else if (!internal_queue.empty()) {
            now = time_of(internal_queue.front()->event);
        } else if (!input_queue.empty()) {
            now = time_of(input_queue.front()->event);
        } else {
            // fixme: this is accessed & written to directly by fork.cpp: machine->mCurrent_time = now;
            now = mCurrent_time;
            // in this case we might:
            mCurrent_time = 0;
        }

        if (now) {
            // this can thaw, freeze,?
            PluginClass(nextPlugin)->ProcessTime(nextPlugin, now);
        }
    }
    if (!output_queue.empty ())
        mdb("%s: still %d events to output\n", __FUNCTION__, output_queue.length ());
}

// Another event has been determined. So:
// todo:  possible emit a (notification) event immediately,
// ... and push the event down the pipeline, when not frozen.

/* note, that after this EV could point to a deallocated memory! */
void
machineRec::output_event(key_event* ev)
{
    assert(ev->event);

    output_queue.push(ev);
    flush_to_next();
};

/**
 * Operations on the machine
 * fixme: should it include the `self_forked' keys ?
 * `self_forked' means, that i decided to NOT fork. to mark this decision
 * (for when a repeated event arrives), i fork it to its own keycode
 */

/**
 * Now the operations on the Dynamic state
 */
void
machineRec::reverse_slice(list_with_tail &pre, list_with_tail &post)
{
    // Slice with a reversed semantic:
    // A.slice(B) --> ()  (AB)
    // traditional is (AB) ()
    pre.slice(post); // mmc:? splice?
    pre.swap(post);
}

/** one key-event investigation finished, now reset for the next one */
void
machineRec::rewind_machine()
{
    assert ((state == st_deactivated) || (state == st_activated));

    /* reset the machine */
    mdb("== Resetting the fork machine (internal %d, input %d)\n",
        internal_queue.length (),
        input_queue.length ());

    change_state(st_normal);
    verificator_keycode = 0;

    if (!(internal_queue.empty())) {
        reverse_slice(internal_queue, input_queue);
        mdb("now in input_queue: %d\n", input_queue.length ());
    }
};


/* we concluded the key is forked. "output" it and prepare for the next one.
 * fixme: locking?
 */
void
machineRec::activate_fork()
{
    assert(!internal_queue.empty());

    key_event* ev = internal_queue.pop();
    KeyCode forked_key = detail_of(ev->event);
    // assert(forked_key == suspect);

    ev->forked =  forked_key;
    /* Change the keycode, but remember the original: */
    forkActive[forked_key] =
        ev->event->device_event.detail.key = config->fork_keycode[forked_key];
    output_event(ev);

    change_state(st_activated);
    mdb("%s the key %d-> forked to: %d. Internal queue has %d events. %s\n", __FUNCTION__,
        forked_key, forkActive[forked_key],
         internal_queue.length (),
        describe_machine_state());

    rewind_machine();
}

/* note: used only in configure.c!
 * Resets the machine, so as to reconsider the events on the
 * `internal' queue.
 * Apparently the criteria/configuration has changed!
 * Reasonably this is in response to a key event. So we are in Final state.
 */
void
machineRec::replay_events(Bool force_also)
{
    mdb("%s\n", __FUNCTION__);
    check_locked();

    if (!internal_queue.empty()) {
        // fixme: worth it?
        reverse_slice(internal_queue, input_queue);
    }
    change_state(st_normal);
    // todo: what else?
    // last_released & last_released_time no more available.
    last_released = 0; // bug!
    mDecision_time = 0;     // we are not waiting for anything

    try_to_play(force_also);
}

/*
 * Take from input_queue, + the mCurrent_time + force   -> run the machine.
 */
void
machineRec::try_to_play(Bool force_also)
{
    // fixme: maybe All I need is the nextPlugin?
    PluginInstance* const nextPlugin = mPlugin->next;

    // log_queues_and_nextplugin(message)
    mdb("%s: next %s: internal %d, input: %d\n", __FUNCTION__,
         (plugin_frozen(nextPlugin)?"frozen":"NOT frozen"),
         internal_queue.length (),
         input_queue.length ());

    // notice that instead of recursion, all the calls to `rewind_machine' are
    // followed by return to this cycle!
    while (!plugin_frozen(nextPlugin)) {

        if (! input_queue.empty()) {
            key_event *ev = input_queue.pop();
            step_fork_automaton_by_key(ev);
        } else {
            if (mCurrent_time && (state != st_normal)) {
                if (step_fork_automaton_by_time(mCurrent_time))
                    // If this time helped to decide -> machine rewound,
                    // we have to try again.
                    continue;
            }

            if (force_also && (state != st_normal)) {
                step_fork_automaton_by_force();
            } else {
                return;
            }
        }
    }
    /* assert(!plugin_frozen(plugin->next)   --->
     *              queue_empty(machine->input_queue)) */
}

void
machineRec::accept_event(key_event* ev)
{
    mCurrent_time = 0; // time_of(ev->event);
    input_queue.push(ev);
    try_to_play(FALSE);
}

/*
 * Called by mouse button press processing.
 * Make all the forkable (pressed)  forked! (i.e. confirm them all)
 *
 * If in Suspect or Verify state, force the fork. (todo: should be configurable)
 */
void
machineRec::step_fork_automaton_by_force()
{
    if ((state == st_normal) || (internal_queue.empty())) {
        // doe  this imply  that ^^^ ?
        return;
    }

    if (state == st_deactivated) {
        ErrorF("%s: BUG.\n", __FUNCTION__);
        return;
    }

    /* so, the state is one of: verify, suspect or activated. */

    log_state(__FUNCTION__);

    // todo: move it inside?
    mDecision_time = 0;
    activate_fork();
}

// So the event proves, that the current event is not forked.
void
machineRec::do_confirm_non_fork_by(key_event *ev)
{
    assert(state == st_suspect || state == st_verify);

    if (mDecision_time != 0)
        mdb("BUG/assert %u\n", mDecision_time);
    // assert(mDecision_time == 0);

    change_state(st_deactivated);
    internal_queue.push(ev); //  this  will be re-processed!!

    key_event* non_forked_event = internal_queue.pop();
    // todo: improve this log:
    mdb("this is not a fork! %d\n", detail_of(non_forked_event->event));

    rewind_machine();
    output_event(non_forked_event);
}

// so EV confirms fork of the current event.
void
machineRec::do_confirm_fork_by(key_event *ev)
{
    /* fixme: ev is the just-read event. But that is surely not the head
       of queue (which is confirmed to fork) */
    mdb("confirm:\n");
    internal_queue.push(ev);

    mDecision_time = 0;
    activate_fork();
}

/*
  returns:
  state  (in the `machine')
  output_event   possibly, othewise 0
  machine->mDecision_time   ... for another timer.
*/


// return 0  if the current/first key is pressed enough time to fork.
// or time when this will happen.
Time
machineRec::key_pressed_too_long(Time current_time)
{
    assert(state== st_verify || state == st_suspect);

    int verification_interval =
        config->verification_interval_of(suspect,
                                         // note: this can be 0 (& should be, unless)
                                         verificator_keycode);
    Time decision_time = suspect_time + verification_interval;

    mdb("time: verification_interval = %dms elapsed so far =%dms\n",
         verification_interval,
         (int)(current_time - suspect_time));

    if (decision_time <= current_time)
        return 0;
    else
        return decision_time;
}


// return 0 if enough, otherwise the time when it will be enough/proving a fork.
Time
// dangerous to name it current_time, like the member variable!
machineRec::key_pressed_in_parallel(Time current_time)
{
    // verify overlap
    int overlap_tolerance = config->overlap_tolerance_of(suspect,
                                                                  verificator_keycode);
    Time decision_time =  verificator_time + overlap_tolerance;

    if (decision_time <= current_time) {
        // already "parallel"
        return 0;
    } else {
        mdb("time: overlay interval = %dms elapsed so far =%dms\n",
             overlap_tolerance,
             (int) (current_time - verificator_time));

        mdb("suspected = %d, verificator_keycode %d. Times: overlap %" TIME_FMT ", "
             "still needed: %" TIME_FMT " (ms)\n", suspect, verificator_keycode,
             current_time - verificator_time,
             decision_time - current_time);

        return decision_time;
    }
}


bool
machineRec::step_fork_automaton_by_time(Time current_time)
{
    // confirm fork:
    int reason;
    mdb("%s%s%s state: %s, queue: %d, time: %u key: %d\n",
         fork_color, __FUNCTION__, color_reset,
         describe_machine_state(),
         internal_queue.length (), (int)current_time,
         suspect);

    /* First, I try the simple (fork-by-one-keys).
     * If that works, -> fork! Otherwise, I try w/ 2-key forking, overlapping.
     */

    // notice, how mDecision_time is rewritten here:
    if (0 == (mDecision_time =
              key_pressed_too_long(current_time))) {
        reason = reason_total;

        activate_fork();
        return true;
    };

    /* To test 2 keys overlap, we need the 2nd key: a verificator! */
    if (state == st_verify) {
        // verify overlap
        Time decision_time = key_pressed_in_parallel(current_time);

        if (decision_time == 0) {
            reason = reason_overlap;
            activate_fork();
            return true;
        }

        if (decision_time < mDecision_time)
            mDecision_time = decision_time;
    }
    // so, now we are surely in the replay_mode. All we need is to
    // get an estimate on time still needed:


    /* So, we were woken too early. */
    mdb("*** %s: returning with some more time-to-wait: %lu"
        "(prematurely woken)\n", __FUNCTION__,
        mDecision_time - current_time);
    return false;
}

// This is a public api!
void
machineRec::step_in_time_locked(Time now)
{
    mdb("%s:\n", __FUNCTION__);

    PluginInstance* const nextPlugin = mPlugin->next;

    if (mCurrent_time > now)
        mdb("bug: time moved backwards!");

    mCurrent_time = now;

    /* this is run also when thawn, so this is the right moment to retry: */
    flush_to_next();

    /* push the time ! */
    try_to_play(FALSE);

    /* I should take the minimum of time and the time of the 1st event in the
       (output) internal queue */

    // todo: should be method of machine! machine.empty()
    if (!plugin_frozen(nextPlugin)
        // this is guaranteed!
        // && input_queue.empty()
        && internal_queue.empty() // i.e. state == st_normal (or st_deactivated?)
        )
    {
        unlock();
        /* might this be invoked several times?  */
        PluginClass(nextPlugin)->ProcessTime(nextPlugin, mCurrent_time);
        lock();
    }
}


/** apply_event_to_{STATE} */

// is mDecision_time always recalculated?
void
machineRec::apply_event_to_normal(key_event *ev)
{
    DeviceIntPtr keybd = mPlugin->device;

    InternalEvent* event = ev->event;
    KeyCode key = detail_of(event);
    Time simulated_time = time_of(event);

    assert(internal_queue.empty());

    // if this key might start a fork....
    if (press_p(event) && forkable_p(config, key)
        /* fixme: is this w/ 1-event precision? (i.e. is the xkb-> updated synchronously) */
        /* todo:  does it have a mouse-related action? */
        && !(mouse_emulation_on(keybd))) {
        /* Either suspect, or detect .- trick to suppress fork */

        /* .- trick: by depressing/re-pressing the key rapidly, fork is disabled,
         * and AR is invoked */
#if DEBUG
        if ( !key_forked(key) && (last_released == key )) {
            mdb("can we invoke autorepeat? %d  upper bound %d ms\n",
                // mmc: config is pointing outside memory range!
                  (int)(simulated_time - last_released_time), config->repeat_max);
        }
#endif
        /* So, unless we see the .- trick, we do suspect: */
        if (!key_forked(key) &&
            ((last_released != key ) ||
             /*todo: time_difference_more(last_released_time,simulated_time,
              * config->repeat_max) */
             (simulated_time - last_released_time) >
             (Time) config->repeat_max))
        {
            change_state(st_suspect);
            suspect = key;
            suspect_time = time_of(event);
            mDecision_time = suspect_time +
                config->verification_interval_of(key, 0);
            internal_queue.push(ev);
            return;
        } else {
            // .- trick: (fixme: or self-forked)
            mdb("re-pressed very quickly\n");
            forkActive[key] = key; // fixme: why??
            output_event(ev);
            return;
        };
    } else if (release_p(event) && (key_forked(key))) {
        mdb("releasing forked key\n");
        // fixme:  we should see if the fork was `used'.
        if (config->consider_forks_for_repeat){
            // C-f   f long becomes fork. now we wanted to repeat it....
            last_released = detail_of(event);
            last_released_time = time_of(event);
        } else {
            // imagine mouse-button during the short 1st press. Then
            // the 2nd press ..... should not relate the the 1st one.
            last_released = 0;
            last_released_time = 0;
        }
        /* we finally release a (self-)forked key. Rewrite back the keycode.
         *
         * fixme: do i do this in other machine states?
         */
        event->device_event.detail.key = forkActive[key];

        // this is the state (of the keyboard, not the machine).... better to
        // say of the machine!!!
        forkActive[key] = 0;
        output_event(ev);
    } else {
        if (release_p (event)) {
            last_released = detail_of(event);
            last_released_time = time_of(event);
        };
        // pass along the un-forkable event.
        output_event(ev);
    };
};


/*  First (press)
 *  Second    <-- we are here.
 */
void
machineRec::apply_event_to_suspect(key_event *ev)
{
    InternalEvent* event = ev->event;
    Time simulated_time = time_of(event);
    KeyCode key = detail_of(event);

    list_with_tail &queue = internal_queue;

    /* Here, we can
     * o refuse .... if suspected/forkable is released quickly,
     * o fork (definitively),  ... for _time_
     * o start verifying, or wait, or confirm (timeout)
     * todo: I should repeat a bi-depressed forkable.
     */
    assert(!queue.empty() && state == st_suspect);

    // todo: check the ranges (long vs. int)
    if (0 == (mDecision_time =
              key_pressed_too_long(simulated_time))) {
        do_confirm_fork_by(ev);
        return;
    };

    /* So, we now have a second key, since the duration of 1 key
     * was not enough. */
    if (release_p(event)) {
        mdb("suspect/release: suspected = %d, time diff: %d\n",
             suspect, (int)(simulated_time - suspect_time));
        if (key == suspect) {
            mDecision_time = 0; // might be useless!
            do_confirm_non_fork_by(ev);
            return;
            /* fixme:  here we confirm, that it was not a user error.....
               bad synchro. i.e. the suspected key was just released  */
        } else {
            /* something released, but not verificating, b/c we are in `suspect',
             * not `confirm'  */
            internal_queue.push(ev);
            return;
        };
    } else {
        if (!press_p (event)) {
            // RawPress & Device events.
            internal_queue.push(ev);
            return;
        }

        if (key == suspect) {
            /* How could this happen? Auto-repeat on the lower/hw level?
             * And that AR interval is shorter than the fork-verification */
            if (config->fork_repeatable[key]) {
                mdb("The suspected key is configured to repeat, so ...\n");
                forkActive[suspect] = suspect;
                mDecision_time = 0;
                do_confirm_non_fork_by(ev);
                return;
            } else {
                // fixme: this keycode is repeating, but we still don't know what to do.
                // ..... `discard' the event???
                // fixme: but we should recalc the mDecision_time !!
                return;
            }
        } else {
            // another key pressed
            change_state(st_verify);
            verificator_time = simulated_time;
            verificator_keycode = key; /* if already we had one -> we are not in this state!
                                           if the verificator becomes a modifier ?? fixme:*/
            // verify overlap
            Time decision_time = key_pressed_in_parallel(simulated_time);

            // well, this is an abuse ... this should never be 0.
            if (decision_time == 0) {
                mdb("absurd\n"); // this means that verificator key verifies immediately!
            }
            if (decision_time < mDecision_time)
                mDecision_time = decision_time;

            internal_queue.push(ev);
            return;
        };
    }
}


/*
 * Timeline:
 * ========
 * first
 * second .. verifier
 * third-event  < we are here now.
 *
 * ???? how long?
 * second Released.
 * So, already 2 keys have been pressed, and still no decision.
 * Now we have the 3rd key.
 * We wait only for time, and for the release of the key */
void
machineRec::apply_event_to_verify_state(key_event *ev)
{
    InternalEvent* event = ev->event;
    Time simulated_time = time_of(event);
    KeyCode key = detail_of(event);

    /* We pressed a forkable key, and another one (which could possibly
       use the modifier). Now, either the forkable key was intended
       to be `released' before the press of the other key (and we have an
       error due to mis-synchronization), or in fact, the forkable
       was actually `used' as a modifier.

       This should not be fork:
       I----I (short)
       E--E

       This should be a fork:
       I-----I (long)
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

    if (0 == (mDecision_time = key_pressed_too_long(simulated_time)))
    {
        do_confirm_fork_by(ev);
        return;
    }

    /* now, check the overlap of the 2 first keys */
    Time decision_time = key_pressed_in_parallel(simulated_time);

    // well, this is an abuse ... this should never be 0.
    if (decision_time == 0)
    {
        do_confirm_fork_by(ev);
        return;
    }
    if (decision_time < mDecision_time)
        mDecision_time = decision_time;

    if (release_p(event) && (key == suspect)){ // fixme: is release_p(event) useless?
        mdb("fork-key released on time: %dms is a tolerated error (< %lu)\n",
             (int)(simulated_time -  suspect_time),
             config->verification_interval_of(suspect,
                                      verificator_keycode));
        mDecision_time = 0; // useless fixme!
        do_confirm_non_fork_by(ev);

    } else if (release_p(event) && (verificator_keycode == key)){
        // todo: we might be interested in percentage, Then here we should do the work!

        // we should change state:
        change_state(st_suspect);
        verificator_keycode = 0;   // we _should_ take the next possible verificator
        internal_queue.push(ev);
    } else {               // fixme: a (repeated) press of the verificator ?
        // fixme: we pressed another key: but we should tell XKB to repeat it !
        internal_queue.push(ev);
    };
}

/* Apply event EV to (state, internal-queue, time).
 * This can append to the output-queue
 *      sets: `mDecision_time'
 *
 * input:
 *   internal-queue  +      input-queue + ev
 * output:
 *   either the ev  is pushed on internal_queue, or to the output-queue
 *   the head of internal_queue may be pushed to the output-queue as well.
 */
void
machineRec::step_fork_automaton_by_key(key_event *ev)
{
    assert (ev);

    InternalEvent* event = ev->event;
    KeyCode key = detail_of(event);

    /* Please, first change the state, then enqueue, and then EMIT_EVENT.
     * fixme: should be a function then  !!!*/

    // mDecision_time = 0;

#if DDX_REPEATS_KEYS || 1
    /* `quick_ignore': I want to ignore _quickly_ the hw-repeated unfiltered (forked) modifiers.
       Normal modifier are ignored before put in the X input pipe/queue.
       This is only necessary if the lower level (keyboard driver) passes through the
       HW auto-repeat events. */

    if ((key_forked(key)) && press_p(event)
        && (key != forkActive[key])) // not `self_forked'
    {
        mdb("%s: the key is forked, ignoring\n", __FUNCTION__);

        mxfree(ev->event, ev->event->any.length);
        mxfree(ev, sizeof(key_event));
        return;
    }
#endif

    // A currently forked keycode cannot be (suddenly) pressed 2nd time.
    // assert(release_p(event) || (key < MAX_KEYCODE && forkActive[key] == 0));

#if DEBUG
    if (press_p(event) || release_p(event)) {
        log_state_and_event(__FUNCTION__, ev);
    }
#endif

    switch (state) {
        case st_normal:
            apply_event_to_normal(ev);
            return;
        case st_suspect:
        {
            apply_event_to_suspect(ev);
            return;
        }
        case st_verify:
        {
            apply_event_to_verify_state(ev);
            return;
        }
        default:
            mdb("----------unexpected state---------\n");
    }
}
