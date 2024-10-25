
#include "config.h"
#include "colors.h"

#include <memory>

#include "machine.h"

// I need the enum of requests:
// fork_configure_overlap_limit ....
#include "fork_enums.h"

namespace forkNS {

#if MULTIPLE_CONFIGURATIONS
template <typename Keycode, typename Time, typename archived_event_t>
ForkConfiguration<Keycode, Time, 256>**
forkingMachine<Keycode, Time, archived_event_t>::find_configuration_n(const int n)
{
    fork_configuration** config_p = &config;

    while (((*config_p)->next) && ((*config_p)->id != n)) {
        environment->log("%s skipping over %d\n", __func__, (*config_p)->id);
        config_p = &((*config_p) -> next);
    }
    return ((*config_p)->id == n)? config_p: NULL;
}

// and replay whatever is inside the machine!
// locked?
template <typename Keycode, typename Time, typename archived_event_t>
void
forkingMachine<Keycode, Time, archived_event_t>::switch_config(int id)
{
    environment->log("%s %d\n", __func__, id);
    fork_configuration** config_p = find_configuration_n(id);
    environment->log("%s found\n", __func__);

    // fixme:   `move_to_top'   find an element in a linked list, and move it to the head.
    if ((config_p)
        // useless:
        && (*config_p)
        && (*config_p != config)) {
        mdb("switching configs %d -> %d\n", config->id, id);

        fork_configuration* new_current = *config_p;
        //fixme: this sequence works at the beginning too!!!

        // remove from the list:
        *config_p = new_current->next; //   n-1 -> n + 1

        // reinsert at the beginning:
        new_current->next = config; //    n -> 1
        config = new_current; //     -> n

        mdb("switched configs %d -> %d\n", config->id, id);
        replay_events(false);
    } else {
        environment->log("config remains %d\n", config->id);
    }
}
#endif // MULTIPLE_CONFIGURATIONS

/** update the configuration */
template <typename Keycode, typename Time, typename archived_event_t>
int
forkingMachine<Keycode, Time, archived_event_t>::configure_global(int type, int value, bool set) {
   const auto fork_configuration = this->config.get();

   switch (type) {
   case fork_configure_overlap_limit:
        if (set)
          fork_configuration->overlap_tolerance[0][0] = value;
        else
          return fork_configuration->overlap_tolerance[0][0];
        break;

   case fork_configure_total_limit:
        if (set)
          fork_configuration->verification_interval[0][0] = value;
        else
          return fork_configuration->verification_interval[0][0];
        break;

   case fork_configure_clear_interval:
        if (set)
          fork_configuration->clear_interval = value;
        else
          return fork_configuration->clear_interval;
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
          set_last_events_count(value);
        else
          return max_last;
        break;
   case fork_configure_debug:
        if (set) {
          //  here we force, rather than using MDB !
          mdb("fork_configure_debug set: %d -> %d\n",
              config->debug,
              value);
          fork_configuration->debug = value;
        } else {
          mdb("fork_configure_debug get: %d\n",
              fork_configuration->debug);
          return fork_configuration->debug; // (bool) ?True:FALSE
        }
        break;

   case fork_server_dump_keys:
       dump_last_events(environment->get_event_dumper().get());
       break;

       // mmc: this is special:
   case fork_configure_switch:
        assert(set);

        mdb("fork_configure_switch: %d\n", value);
#if MULTIPLE_CONFIGURATIONS
        switch_config(value);
#endif
        break;

   default:
        mdb("%s: invalid option %d\n", __func__, value);
   }
   return 0;
}

/**
 * key and twin have a relationship, given by type.
 * |--------------|========\-----------\
 * A key pressed  B pressed B released  A released
 *
 * |----------- |========\------------\
 * A pressed    B pressed A released  B released
 */
enum twin_parameter {
    verification_time = 1, // A--B-- t  -> A is forked.
    overlap_limit = 2,     // A B--t  -> A is forked.
                           // notice t is measured from A and B
};

template <typename Keycode, typename Time, typename archived_event_t>
int
forkingMachine<Keycode, Time, archived_event_t>::configure_twins(int type, Keycode key, Keycode twin, int value, bool set) {
    switch (type) {
        case fork_configure_total_limit:
            if (set)
                config->verification_interval[key][twin] = value;
            else
                return config->verification_interval[key][twin];
            break;
        case fork_configure_overlap_limit:
            if (set)
                config->overlap_tolerance[key][twin] = value;
            else return config->overlap_tolerance[key][twin];
            break;
        default:
            mdb("%s: invalid type %d\n", __func__, type);;
    }
    return 0;
}


enum key_parameters {
    key_fork,                   // Keycode
    key_repeat,                 // true/false
};

template <typename Keycode, typename Time, typename archived_event_t>
int
forkingMachine<Keycode, Time, archived_event_t>::configure_key(int type, Keycode key, int value, bool set) {
   mdb("%s: keycode %d -> value %d, function %d\n",
       __func__, key, value, type);

   switch (type) {
       case fork_configure_key_fork:
           if (set)
               config->fork_keycode[key] = value;
           else return config->fork_keycode[key];
           break;
       case fork_configure_key_fork_repeat:
           if (set)
               config->fork_repeatable[key] = value;
           else return config->fork_repeatable[key];
           break;
       default:
           mdb("%s: invalid option %d\n", __func__, value);
   }
   return 0;
}


template <typename Keycode, typename Time, typename archived_event_t>
int
forkingMachine<Keycode, Time, archived_event_t>::dump_last_events_to_client(
    event_publisher<archived_event_t>* publisher, int max_requested) {
    // I don't need to count them! last_events_count
    // should be locked
    int queue_count = last_events_log.size();

    if (max_requested > queue_count) {
        max_requested = queue_count;
    };

    publisher->prepare(max_requested);

    std::function<void(const archived_event_t&)> lambda =
        [publisher](const archived_event_t& ev){ publisher->event(ev); };
    // auto f = std::function<void(const archived_event&)>(bind(publisher->event(), publisher,));

    // todo:
    // fixme: we need to increase an iterator .. pointer .... to the C array!
    // last_events.
    for_each(last_events_log.begin(),
             last_events_log.end(),
             lambda);

    mdb("sending %d events\n", max_requested);

    return publisher->commit();
}

/**
 * The machine is locked here:
 * Push as many as possible from the OUTPUT queue to the next layer
 * Unlocks!
 **/
template <typename Keycode, typename Time, typename archived_event_t>
void
forkingMachine<Keycode, Time, archived_event_t>::flush_to_next() {
    // todo: could I lock only in this scope?
    check_locked();

    if (queues_non_empty()) {
        log_queues(__func__);
    }

    // send out events
    while(! environment->output_frozen() && !output_queue.empty()) {
        // now we are the owner.
        std::unique_ptr<key_event> event(output_queue.pop());

        // (ORDER) this event must be delivered before any other!
        // so no preemption of this part!  Are we re-entrant?
        // yet, the next plugin could call in here? to do what?
        {
            // could I emplace it?
            // reference = last_events_log.emplace_back()
            // reference.forked = ev->forked;
            archived_event_t archived_event;
            environment->archive_event(archived_event, event->p_event);
            archived_event.forked = event->forked;

            last_events_log.push_back(archived_event);
        }

        // machine. fixme: it's not true -- it cannot!
        // 2020: it can!
        // so ... this is front_lock?
        {
            // why unlock during this? maybe also during the push_time_to_next then?
            // because it can call into back to us? NotifyThaw()

            // fixme: was here a bigger message?
            // bug: environment->fmt_event(ev->p_event);
            unlock();
            // we must gurantee ORDER
            environment->relay_event(event->p_event);
            lock();
        };
    }
    if (! environment->output_frozen()) {
        // now we still have time:
        // we should push the time!
        push_time_to_next();
    }
    if (!output_queue.empty ())
        mdb("%s: still %d events to output\n", __func__, output_queue.length ());
}

/**
 * We concluded the key is forked. "Output" it and prepare for the next one.
 * fixme: locking?
 */
template <typename Keycode, typename Time, typename archived_event_t>
void
forkingMachine<Keycode, Time, archived_event_t>::activate_fork() {
    assert(!internal_queue.empty());

    std::unique_ptr<key_event> event(internal_queue.pop());
    Keycode forked_key = environment->detail_of(event->p_event);
    // assert(forked_key == suspect);
    event->forked = forked_key;

    /* Change the keycode, but remember the original: */
    forkActive[forked_key] = config->fork_keycode[forked_key];
    // todo: use std::format(), not hard-coded %d
    mdb("%s the key %d-> forked to: %d. Internal queue has %d events. %s\n",
        __func__,
        forked_key, forkActive[forked_key],
        internal_queue.length (),
        describe_machine_state(this->state));

    environment->rewrite_event(event->p_event, forkActive[forked_key]);
    environment->relay_event(event->p_event);

    rewind_machine(st_activated);
}


/**
 * Operations on the machine
 * fixme: should it include the `self_forked' keys ?
 * `self_forked' means, that i decided to NOT fork. to mark this decision
 * (for when a repeated event arrives), i fork it to its own keycode
 */

/**
 * Now the operations on the Dynamic state
 */


/**
 * Take from `input_queue', + the mCurrent_time + force  -> run the machine.
 */
template <typename Keycode, typename Time, typename archived_event_t>
void
forkingMachine<Keycode, Time, archived_event_t>::try_to_play(bool force_also)
{
    // fixme: maybe All I need is the nextPlugin?

    if (environment->output_frozen() ||
        (!input_queue.empty() || !internal_queue.empty())
        ) {
        // log_queues_and_nextplugin(message)
        mdb("%s: next %sfrozen: internal %d, input: %d\n", __func__,
            (environment->output_frozen()?"":"NOT "),
            internal_queue.length(),
            input_queue.length());
    }

    // notice that instead of recursion, all the calls to `rewind_machine' are
    // followed by return to this cycle!
    while (! environment->output_frozen()) {
        if (! input_queue.empty()) {
            std::unique_ptr<key_event> ev(input_queue.pop());
            step_automaton_by_key(std::move(ev));
        } else {
            if (mCurrent_time && (state != st_normal)) {
                if (step_by_time(mCurrent_time))
                    // If this time helped to decide -> machine rewound,
                    // we have to try again, maybe the queue is not empty?.
                    continue;
            }

            if (force_also && (state != st_normal)) {
                step_by_force();
            } else {
                break;
            }
        }
    }
    /* assert(!plugin_frozen(plugin->next)   --->
     *              queue_empty(machine->input_queue)) */
}

/** we take over pevent and promise to deliver back via
 *  relay_event -> hand_over_event_to_next_plugin
 *
 *  todo: PlatformEvent is now owned ... it will be destroyed by Environment.
 */
template <typename Keycode, typename Time, typename archived_event_t>
void
forkingMachine<Keycode, Time, archived_event_t>::accept_event(std::unique_ptr<PlatformEvent> pevent) noexcept(false) {

    // this can only throw
    auto event = std::make_unique<key_event>(std::move(pevent));
    event->forked = no_key; // makes sense?
    input_queue.push(event.release());

    // fixme:
    mCurrent_time = 0; // time_of(ev->event);
    try_to_play(false);
}

/*
 * Called by mouse button press processing.
 * Make all the forkable (pressed)  forked! (i.e. confirm them all)
 * (could use a bitmask to configure what reacts)
 * If in Suspect or Verify state, force the fork. (todo: should be configurable)
 */
template <typename Keycode, typename Time, typename archived_event_t>
void
forkingMachine<Keycode, Time, archived_event_t>::step_by_force()
{
    if ((state == st_normal) || (internal_queue.empty())) {
        // does this imply  that ^^^ ?
        // then maybe just test (internal_queue.empty()) ?
        return;
    }

    if (state == st_deactivated) {
        environment->log("%s: BUG.\n", __func__);
        return;
    }

    /* so, the state is one of: verify, suspect or activated. */
    log_state(__func__);

    // todo: move it inside?
    mDecision_time = 0;
    activate_fork();
}

/*
  returns:
  state  (in the `machine')
  relay_event   possibly, othewise 0
  machine->mDecision_time   ... for another timer.
*/


// return 0  if the current/first key is pressed enough time to fork.
// or time when this will happen.
template <typename Keycode, typename Time, typename archived_event_t>
Time
forkingMachine<Keycode, Time, archived_event_t>::key_pressed_too_long(Time current_time)
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
template <typename Keycode, typename Time, typename archived_event_t>
Time
// dangerous to name it current_time, like the member variable!
forkingMachine<Keycode, Time, archived_event_t>::verifier_decision_time(Time current_time)
{
    // verify overlap
    int overlap_tolerance = config->overlap_tolerance_of(suspect, verificator_keycode);
    Time decision_point_time =  verificator_time + overlap_tolerance;

    if (decision_point_time <= current_time) {
        // already "parallel"
        return 0;
    } else {
        mdb("time: overlay interval = %dms elapsed so far =%dms\n",
             overlap_tolerance,
             (int) (current_time - verificator_time));

        mdb("suspected = %d, verificator_keycode %d. Times: overlap %" TIME_FMT ", "
             "still needed: %" TIME_FMT " (ms)\n", suspect, verificator_keycode,
             current_time - verificator_time,
             decision_point_time - current_time);

        return decision_point_time;
    }
}


template <typename Keycode, typename Time, typename archived_event_t>
bool
forkingMachine<Keycode, Time, archived_event_t>::step_by_time(Time current_time)
{
    // confirm fork:
    [[maybe_unused]] fork_reason reason; // fixme: unused!
    mdb("%s%s%s state: %s, queue: %d, time: %u key: %d\n",
         fork_color, __func__, color_reset,
         describe_machine_state(this->state),
         internal_queue.length (), (int)current_time,
         suspect);

    /* First, I try the simple (fork-by-one-keys).
     * If that works, -> fork! Otherwise, I try w/ 2-key forking, overlapping.
     */

    // notice, how mDecision_time is rewritten here:
    if (0 == (mDecision_time =
              key_pressed_too_long(current_time))) {
        reason = fork_reason::reason_total;

        activate_fork();
        return true;
    };

    /* To test 2 keys overlap, we need the 2nd key: a verificator! */
    if (state == st_verify) {
        // verify overlap
        Time decision_time = verifier_decision_time(current_time);

        if (decision_time == 0) {
            reason = fork_reason::reason_overlap;
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
        "(prematurely woken)\n", __func__,
        mDecision_time - current_time);
    return false;
}

// This is a public api!
template <typename Keycode, typename Time, typename archived_event_t>
void
forkingMachine<Keycode, Time, archived_event_t>::step_in_time_locked(const Time now) // unlocks possibly!
{
    if (queues_non_empty()) {
        mdb("%s: %" TIME_FMT "\n", __func__, now);
    }
    if (mCurrent_time > now)
        mdb("bug: time moved backwards!");
    mCurrent_time = now;

    /* this is run also when thawn, so this is the right moment to retry: */
    flush_to_next();

    /* push the time ! */
    try_to_play(false);

    /* I should take the minimum of time and the time of the 1st event in the
       (output) internal queue */

    // todo: should be method of machine! machine.empty()
    if (!environment->output_frozen()
        // this is guaranteed!
        // && input_queue.empty()
        && internal_queue.empty() // i.e. state == st_normal (or st_deactivated?)
        )
    {
        unlock();
        /* might this be invoked several times?  */
        environment->push_time(mCurrent_time);
        lock();
    }
}

/** apply_event_to_{STATE} */

// is mDecision_time always recalculated?
template <typename Keycode, typename Time, typename archived_event_t>
void
forkingMachine<Keycode, Time, archived_event_t>::apply_event_to_normal(std::unique_ptr<key_event> event) // possibly unlocks
{
    const PlatformEvent *pevent = event->p_event;

    const Keycode key = environment->detail_of(pevent);
    const Time simulated_time = environment->time_of(pevent);

    assert(state == st_normal && internal_queue.empty());

    // if this key might start a fork....
    if (environment->press_p(pevent)
        && forkable_p(config.get(), key)
        && !environment->ignore_event(pevent)) {
        /* ".-" AR-trick: by depressing/re-pressing the key rapidly, AR is invoked, not fork */
#if DEBUG
        if ( !key_forked(key) && (last_released == key )) {
            mdb("can we invoke autorepeat? %d  upper bound %d ms\n",
                // mmc: config is pointing outside memory range!
                  (int)(simulated_time - last_released_time), config->repeat_max);
        }
#endif
        /* So, unless we see the .- trick, we do suspect: */
        if (!key_forked(key) &&
            ((last_released != key )
             || time_difference_more(simulated_time, last_released_time, config->repeat_max))) {

            change_state(st_suspect);
            suspect = key;
            suspect_time = environment->time_of(pevent);
            mDecision_time = suspect_time +
                config->verification_interval_of(key, 0);
            internal_queue.push(event.release());
            return;
        } else {
            // .- trick: (fixme: or self-forked)
            mdb("re-pressed very quickly\n");
            forkActive[key] = key; // fixme: why not 0 ?
            output_event(std::move(event));
            return;
        };
    } else if (environment->release_p(pevent) && (key_forked(key))) {
        mdb("releasing forked key\n");
        // fixme:  we should see if the fork was `used'.
        if (config->consider_forks_for_repeat){
            // C-f   f long becomes fork. now we wanted to repeat it....
            last_released = environment->detail_of(pevent);
            last_released_time = environment->time_of(pevent);
        } else {
            // imagine mouse-button during the short 1st press. Then
            // the 2nd press ..... should not relate the the 1st one.
            last_released = no_key;
            last_released_time = 0;
        }
        /* we finally release a (self-)forked key. Rewrite back the keycode.
         *
         * fixme: do i do this in other machine states?
         */
        environment->rewrite_event(const_cast<PlatformEvent*>(pevent), forkActive[key]);
        forkActive[key] = 0;

        output_event(std::move(event));
    } else {
        if (environment->release_p(pevent)) {
            last_released = environment->detail_of(pevent);
            last_released_time = environment->time_of(pevent);
        };
        // pass along the un-forkable event.
        output_event(std::move(event));
    };
};


/**  First (press)
 *    v   ^     0   <-- we are here.
 *        Second
 */
template <typename Keycode, typename Time, typename archived_event_t>
void
forkingMachine<Keycode, Time, archived_event_t>::apply_event_to_suspect(std::unique_ptr<key_event> ev)
{
    const PlatformEvent* pevent = ev->p_event;
    Time simulated_time = environment->time_of(pevent);
    Keycode key = environment->detail_of(pevent);

    auto &queue = internal_queue;
    assert(!queue.empty() && state == st_suspect);

    /* Here, we can
     * o refuse .... if suspected/forkable is released quickly,
     * o fork (definitively),  ... for _time_
     * o start verifying, or wait, or confirm (timeout)
     * todo: I should repeat a bi-depressed forkable.
     */

    // first we look at the time:
    if (0 == (mDecision_time = key_pressed_too_long(simulated_time))) {
        do_confirm_fork_by(std::move(ev));
        return;
    };

    /* So, we now have a second key, since the duration of 1 key
     * was not enough. */
    if (environment->release_p(pevent)) {
        mdb("suspect/release: suspected = %d, time diff: %d\n",
             suspect, (int)(simulated_time - suspect_time));
        if (key == suspect) {
            do_confirm_non_fork_by(std::move(ev));
            return;
            /* fixme:  here we confirm, that it was not a user error.....
               bad synchro. i.e. the suspected key was just released  */
        } else {
            /* something released, but not verificating, b/c we are in `suspect',
             * not `confirm'  */
            internal_queue.push(ev.release());
            return;
        };
    } else {
        if (! environment->press_p(pevent)) { // why not release_p() ?
            // RawPress & Device events.
            if (!environment->release_p(pevent)) {
                mdb("a bizzare event scanned\n");
            }
            internal_queue.push(ev.release());
            return;
        }
        // press-event here:
        if (key == suspect) {
            /* How could this happen? Auto-repeat on the lower/hw level?
             * And that AR interval is shorter than the fork-verification */
            if (config->fork_repeatable[key]) {
                mdb("The suspected key is configured to repeat, so ...\n");
                forkActive[suspect] = suspect;
                do_confirm_non_fork_by(std::move(ev));
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
            Time decision_time = verifier_decision_time(simulated_time);

            // well, this is an abuse ... this should never be 0.
            if (decision_time == 0) {
                mdb("absurd\n"); // this means that verificator key verifies immediately!
            }
            if (decision_time < mDecision_time)
                mDecision_time = decision_time;

            internal_queue.push(ev.release());
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
template <typename Keycode, typename Time, typename archived_event_t>
void
forkingMachine<Keycode, Time, archived_event_t>::apply_event_to_verify_state(std::unique_ptr<key_event> ev)
{
    const PlatformEvent* pevent = ev->p_event;
    Time simulated_time = environment->time_of(pevent);
    Keycode key = environment->detail_of(pevent);

    /* We pressed a forkable key I, and another one E (which could possibly
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

    if (0 == (mDecision_time = key_pressed_too_long(simulated_time))) {
        do_confirm_fork_by(std::move(ev));
        return;
    }

    /* now, check the overlap of the 2 first keys */
    Time decision_time = verifier_decision_time(simulated_time);
    if (decision_time == 0) {
        do_confirm_fork_by(std::move(ev));
        return;
    }
    if (decision_time < mDecision_time)
        mDecision_time = decision_time;

    if ((key == suspect) && environment->release_p(pevent)){ // fixme: is release_p(event) useless?
        mdb("fork-key released on time: %" TIME_FMT "ms is a tolerated error (< %lu)\n",
             (simulated_time -  suspect_time),
             config->verification_interval_of(suspect,
                                      verificator_keycode));
        do_confirm_non_fork_by(std::move(ev));

    } else if ((verificator_keycode == key) && environment->release_p(pevent)) {
        // todo: maybe this is too weak.

        // todo: we might be interested in percentage, then here we should do the work!

        change_state(st_suspect);
        verificator_keycode = 0;   // we _should_ take the next possible verificator
        internal_queue.push(ev.release());
    } else {
        // fixme: a (repeated) press of the verificator ?
        // fixme: we pressed another key: but we should tell XKB to repeat it !
        internal_queue.push(ev.release());
    };
}

/* Apply event EV to (state, internal-queue, time).
 * This can append to the output-queue
 *      sets: `mDecision_time'
 *
 * input:
 *   internal-queue  + ev + input-queue
 * output:
 *   either the ev  is pushed on internal_queue, or to the output-queue
 *   the head of internal_queue may be pushed to the output-queue as well.
 */
template <typename Keycode, typename Time, typename archived_event_t>
void
forkingMachine<Keycode, Time, archived_event_t>::step_automaton_by_key(std::unique_ptr<key_event> ev)
{
    mdb("%s:\n", __func__);

    assert(ev);
    const PlatformEvent* pevent = ev->p_event;
    const Keycode key = environment->detail_of(pevent);

    /* Please, first change the state, then enqueue, and then EMIT_EVENT.
     * fixme: should be a function then  !!!*/

    // mDecision_time = 0;

#if DDX_REPEATS_KEYS || 1
    /* `quick_ignore': I want to ignore _quickly_ the hw-repeated unfiltered (forked) modifiers.
       Normal modifier are ignored before put in the X input pipe/queue.
       This is only necessary if the lower level (keyboard driver) passes through the
       HW auto-repeat events. */

    if ((key_forked(key)) && environment->press_p(pevent)
        && (key != forkActive[key])) // not `self_forked'
    {
        mdb("%s: the key is forked, ignoring\n", __func__);

        environment->free_event(ev->p_event);
        return;
    }
#endif
    // `limitation':
    // A currently forked keycode cannot be (suddenly) pressed 2nd time.
    // assert(release_p(event) || (key < MAX_KEYCODE && forkActive[key] == 0));
    switch (state) {
        case st_normal:
            apply_event_to_normal(std::move(ev));
            return;
        case st_suspect:
        {
            apply_event_to_suspect(std::move(ev));
            return;
        }
        case st_verify:
        {
            apply_event_to_verify_state(std::move(ev));
            return;
        }
        default:
            mdb("----------unexpected state---------\n");
    }
}

}

