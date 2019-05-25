#define USE_LOCKING 1

#include "fork.h"
#include "debug.h"


extern "C" {
#include "event_ops.h"

#include <xorg-server.h>
#include <xorg/xkbsrv.h>
#include <xorg/xf86Module.h>
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




/* The machine is locked here:
 * Push as many as possible from the OUTPUT queue to the next layer */
void
machineRec::try_to_output(PluginInstance* plugin)
{
    // todo: lock in this scope only?
    check_locked();

    PluginInstance* const next = plugin->next;

    mdb("%s: Queues: output: %d\t internal: %d\t input: %d \n", __FUNCTION__,
         output_queue.length (),
         internal_queue.length (),
         input_queue.length ());

    while((!plugin_frozen(next)) && (!output_queue.empty ())) {
        key_event* ev = output_queue.pop();

        last_events->push_back(make_archived_events(ev));
        InternalEvent* event = ev->event;
        mxfree(ev, sizeof(key_event));

        unlock();
        hand_over_event_to_next_plugin(event, plugin);
        lock();
    };

    // interesting: after handing over, the NEXT might need to be refreshed.
    // if that plugin is gone. todo!

    if (!plugin_frozen(next)) {
        // we should push the time!
        Time now;
        if (!output_queue.empty()) {
            now = time_of(output_queue.front()->event);
        } else if (!internal_queue.empty()) {
            now = time_of(internal_queue.front()->event);
        } else if (!input_queue.empty()) {
            now = time_of(input_queue.front()->event);
        } else {
            now = current_time;
        }

        if (now) {
            // this can thaw, freeze,?
            unlock();
            PluginClass(next)->ProcessTime(next, now);
            lock();
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
machineRec::output_event(key_event* ev, PluginInstance* plugin)
{
    assert(ev->event);
    output_queue.push(ev);
    try_to_output(plugin);
};



/**
 * Operations on the machine
 * fixme: should it include the `self_forked' keys ?
 * `self_forked' means, that i decided to NOT fork. to mark this decision
 * (for when a repeated event arrives), i fork it to its own keycode
 */



/* Now the operations on the Dynamic state */

/* Return the keycode into which CODE has forked _last_ time.
   Returns code itself, if not forked. */
Bool
machineRec::key_forked(KeyCode code)
{
    return (forkActive[code]);
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
    assert (final_state_p(state));

    /* reset the machine */
    mdb("== Resetting the fork machine (internal %d, input %d)\n",
        internal_queue.length (),
        input_queue.length ());

    change_state(st_normal);
    verificator = 0;

    if (!(internal_queue.empty())) {
        reverse_slice(internal_queue, input_queue);
        mdb("now in input_queue: %d\n", input_queue.length ());
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
    list_with_tail &queue = internal_queue;
    assert(!queue.empty());

    key_event* ev = queue.pop();

    KeyCode forked_key = detail_of(ev->event);
    // assert (forked_key == machine->suspect)

    /* change the keycode, but remember the original: */
    ev->forked =  forked_key;
    forkActive[forked_key] =
        ev->event->device_event.detail.key = config->fork_keycode[forked_key];

    change_state(st_activated);
    mdb("%s suspected: %d-> forked to: %d,  internal queue is long: %d, %s\n", __FUNCTION__,
         forked_key,
         config->fork_keycode[forked_key],
         internal_queue.length (),
         describe_machine_state()
         );
    rewind_machine();

    output_event(ev,plugin);
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
    if (state == st_normal) {
        return;
    }
    if (state == st_deactivated) {
        ErrorF("%s: BUG.\n", __FUNCTION__);
        return;
    }

    if (internal_queue.empty())
        return;

    /* so, the state is one of: verify, suspect or activated. */
    list_with_tail& queue = internal_queue;

    mdb("%s%s%s state: %s, queue: %d .... FORCE\n",
         fork_color, __FUNCTION__, color_reset,
         describe_machine_state(),
         queue.length ());

    decision_time = 0;
    activate_fork(plugin);
}

void
machineRec::do_enqueue_event(key_event *ev)
{
    internal_queue.push(ev);
    // when replaying no need to show this:
    // MDB(("enqueue_event: time left: %u\n", machine->decision_time));
}

// so the ev proves, that the current event is not forked.
void
machineRec::do_confirm_non_fork_by(key_event *ev,
                       PluginInstance* plugin)
{
    assert(decision_time == 0);
    change_state(st_deactivated);
    internal_queue.push(ev); //  this  will be re-processed!!


    key_event* non_forked_event = internal_queue.pop();
    mdb("this is not a fork! %d\n", detail_of(non_forked_event->event));
    rewind_machine();

    output_event(non_forked_event,plugin);
}

// so EV confirms fork of the current event.
void
machineRec::do_confirm_fork(key_event *ev, PluginInstance* plugin)
{
    decision_time = 0;

    /* fixme: ev is the just-read event. But that is surely not the head
       of queue (which is confirmed to fork) */
    mdb("confirm:\n");
    internal_queue.push(ev);
    activate_fork(plugin);
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
machineRec::key_pressed_too_long(Time current_time)
{
    int verification_interval =
        config->verification_interval_of(suspect,
                                 // this can be 0 (& should be, unless)
                                 verificator);
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
machineRec::key_pressed_in_parallel(Time current_time)
{
    // verify overlap
    int overlap_tolerance = config->overlap_tolerance_of(suspect,
                                                                  verificator);
    Time decision_time =  verificator_time + overlap_tolerance;

    if (decision_time <= current_time) {
        return 0;
    } else {
        mdb("time: overlay interval = %dms elapsed so far =%dms\n",
             overlap_tolerance,
             (int) (current_time - verificator_time));

        mdb("suspected = %d, verificator %d. Times: overlap %lu, "
             "still needed: %lu (ms)\n", suspect, verificator,
             current_time - verificator_time,
             decision_time - current_time);

        return decision_time;
    }
}


bool
machineRec::step_fork_automaton_by_time(PluginInstance* plugin,
                            Time current_time)
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

    if (0 == (decision_time =
              key_pressed_too_long(current_time))) {
        reason = machineRec::reason_total;
        activate_fork(plugin);
        return true;
    };

    /* To test 2 keys overlap, we need the 2nd key: a verificator! */
    if (state == st_verify) {
        // verify overlap
        Time decision_time = key_pressed_in_parallel(current_time);

        if (decision_time == 0) {
            reason = machineRec::reason_overlap;
            activate_fork(plugin);
            return true;
        }

        if (decision_time < decision_time)
            decision_time = decision_time;
    }
    // so, now we are surely in the replay_mode. All we need is to
    // get an estimate on time still needed:


    /* So, we were woken too early. */
    mdb("*** %s: returning with some more time-to-wait: %lu"
         "(prematurely woken)\n", __FUNCTION__,
         decision_time - current_time);
    return false;
}


void
machineRec::apply_event_to_normal(key_event *ev, PluginInstance* plugin)
{
    DeviceIntPtr keybd = plugin->device;

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
                  (int)(simulated_time - last_released_time), config->repeat_max);
        }
#endif
        /* So, unless we see the .- trick, we do suspect: */
        if (!key_forked(key) &&
            ((last_released != key ) ||
             /*todo: time_difference_more(last_released_time,simulated_time,
              * config->repeat_max) */
             (simulated_time - last_released_time) >
             (Time) config->repeat_max)) {
            /* Emacs indenting bug: */
            change_state(st_suspect);
            suspect = key;
            suspect_time = time_of(event);
            decision_time = suspect_time +
                config->verification_interval_of(key, 0);
            do_enqueue_event(ev);
            return;
        } else {
            // .- trick: (fixme: or self-forked)
            mdb("re-pressed very quickly\n");
            forkActive[key] = key; // fixme: why??
            output_event(ev,plugin);
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
        output_event(ev,plugin);
    } else {
        if (release_p (event)) {
            last_released = detail_of(event);
            last_released_time = time_of(event);
        };
        // pass along the un-forkable event.
        output_event(ev,plugin);
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

    list_with_tail &queue = internal_queue;

    /* Here, we can
     * o refuse .... if suspected/forkable is released quickly,
     * o fork (definitively),  ... for _time_
     * o start verifying, or wait, or confirm (timeout)
     * todo: I should repeat a bi-depressed forkable.
     */
    assert(!queue.empty() && state == st_suspect);

    // todo: check the ranges (long vs. int)
    if ((decision_time =
         key_pressed_too_long(simulated_time)) == 0) {
        do_confirm_fork(ev, plugin);
        return;
    };

    /* So, we now have a second key, since the duration of 1 key
     * was not enough. */
    if (release_p(event)) {
        mdb("suspect/release: suspected = %d, time diff: %d\n",
             suspect, (int)(simulated_time - suspect_time));
        if (key == suspect) {
            decision_time = 0; // might be useless!
            do_confirm_non_fork_by(ev, plugin);
            return;
            /* fixme:  here we confirm, that it was not a user error.....
               bad synchro. i.e. the suspected key was just released  */
        } else {
            /* something released, but not verificating, b/c we are in `suspect',
             * not `confirm'  */
            do_enqueue_event(ev); // the `key'
            return;
        };
    } else {
        if (!press_p (event)) {
            // RawPress & Device events.
            do_enqueue_event(ev);
            return;
        }

        if (key == suspect) {
            /* How could this happen? Auto-repeat on the lower/hw level?
             * And that AR interval is shorter than the fork-verification */
            if (config->fork_repeatable[key]) {
                mdb("The suspected key is configured to repeat, so ...\n");
                forkActive[suspect] = suspect;
                decision_time = 0;
                do_confirm_non_fork_by(ev, plugin);
                return;
            } else {
                // fixme: this keycode is repeating, but we still don't know what to do.
                // ..... `discard' the event???
                // fixme: but we should recalc the decision_time !!
                return;
            }
        } else {
            // another key pressed
            change_state(st_verify);
            verificator_time = simulated_time;
            verificator = key; /* if already we had one -> we are not in this state!
                                           if the verificator becomes a modifier ?? fixme:*/
            // verify overlap
            Time decision_time = key_pressed_in_parallel(simulated_time);

            // well, this is an abuse ... this should never be 0.
            if (decision_time == 0) {
                mdb("absurd\n"); // this means that verificator key verifies immediately!
            }
            if (decision_time < decision_time)
                decision_time = decision_time;

            do_enqueue_event(ev);
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
void
machineRec::apply_event_to_verify(key_event *ev, PluginInstance* plugin)
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

    if ((decision_time = key_pressed_too_long(simulated_time)) == 0)
    {
        do_confirm_fork(ev, plugin);
        return;
    }

    /* now, check the overlap of the 2 first keys */
    Time decision_time = key_pressed_in_parallel(simulated_time);

    // well, this is an abuse ... this should never be 0.
    if (decision_time == 0)
    {
        do_confirm_fork(ev, plugin);
        return;
    }
    if (decision_time < decision_time)
        decision_time = decision_time;


    if (release_p(event) && (key == suspect)){ // fixme: is release_p(event) useless?
        mdb("fork-key released on time: %dms is a tolerated error (< %lu)\n",
             (int)(simulated_time -  suspect_time),
             config->verification_interval_of(suspect,
                                      verificator));
        decision_time = 0; // useless fixme!
        do_confirm_non_fork_by(ev, plugin);

    } else if (release_p(event) && (verificator == key)){
        // todo: we might be interested in percentage, Then here we should do the work!

        // we should change state:
        change_state(st_suspect);
        verificator = 0;   // we _should_ take the next possible verificator
        do_enqueue_event(ev);
    } else {               // fixme: a (repeated) press of the verificator ?
        // fixme: we pressed another key: but we should tell XKB to repeat it !
        do_enqueue_event(ev);
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

    list_with_tail &queue = internal_queue;

    // decision_time = 0;


#if DDX_REPEATS_KEYS || 1
    /* `quick_ignore': I want to ignore _quickly_ the repeated forked modifiers. Normal
       modifier are ignored before put in the X input pipe/queue This is only if the
       lower level (keyboard driver) passes through the auto-repeat events. */

    if ((key_forked(key)) && press_p(event)
        && (key != forkActive[key])) // not `self_forked'
    {
        mdb("%s: the key is forked, ignoring\n", __FUNCTION__);
        mxfree(ev->event, ev->event->any.length);
        mxfree(ev, sizeof(key_event));
        return;
    }
#endif

    // A currently forked keycode cannot be (suddenly) pressed 2nd time. But any pressed
    // key cannot be pressed once more:
    // assert (release_p(event) || (key < MAX_KEYCODE && forkActive[key] == 0));

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
            describe_machine_state(),
            queue.length (),
            key,
            key_color, (char)*sym, color_reset,
            event_type_brief(event));
    }
#endif

    switch (state) {
        case st_normal:
            apply_event_to_normal(ev, plugin);
            return;
        case st_suspect:
        {
            apply_event_to_suspect(ev, plugin);
            return;
        }
        case st_verify:
        {
            apply_event_to_verify(ev, plugin);
            return;
        }
        default:
            mdb("----------unexpected state---------\n");
    }
}
