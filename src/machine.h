#pragma once


#include <cstdarg>
#include "config.h"
#include <algorithm>
#include <functional>

#include "queue.h"
#include "platform.h"
#include "colors.h"
#include <boost/circular_buffer.hpp>


#include "fork_configuration.h"

// namespace fork {

typedef boost::circular_buffer<archived_event> last_events_type;

/* states of the automaton: */

/* `machine': the dynamic `state' */

// typename PlatformEvent, typename platformEnvironment,
template <typename Keycode, typename Time>
// so key_event
class forkingMachine
{
    // history:
    typedef my_queue<key_event> list_with_tail;

    /* How we decided for the fork */
    enum class fork_reason {
        reason_total,               // key pressed too long
        reason_overlap,             // key press overlaps with another key
        reason_force                // mouse-button was pressed & triggered fork.
    };

    enum fork_state_t {  // states of the automaton
        st_normal,
        st_suspect,
        st_verify,
        st_deactivated,
        st_activated
    };


private:
    /* used only for debugging */
    char const * const state_description[5] = {
        "normal",
        "suspect",
        "verify",
        "deactivated",
        "activated"
    };

    volatile int mLock;           /* the mouse interrupt handler should ..... err!  `volatile'
                                  *
                                  * useless mmc!  But i want to avoid any caching it.... SMP ??*/
public:
    platformEnvironment* environment;
#if USE_LOCKING
    void check_locked() const {
        assert(mLock);
    }
    void check_unlocked() const {
        assert(mLock == 0);
    }

    void lock()
    {
        mLock=1;
        // mdb_raw("/--\n");
    }
    void unlock()
    {
        mLock=0;
        // mdb_raw("\\__ (unlock)\n");
    }
#endif


private:
    fork_state_t state;
    // only for certain states we keep (updated):

    Keycode suspect;
    Keycode verificator_keycode;

    // these are "registers"
    Time suspect_time;           /* time of the 1st event in the queue. */
    Time verificator_time = 0;       /* press of the `verificator' */

    /* To allow AR for forkable keys:
     * When we press a key the second time in a row, we might avoid forking:
     * So, this is for the detector:
     *
     * This means I cannot do this trick w/ 2 keys, only 1 is the last/considered! */
    Keycode last_released; // .- trick
    int last_released_time;

    // calculated:
    Time mDecision_time;         /* Time to wait... so that the HEAD event in queue could decide more*/
    Time mCurrent_time;          // the last time we received from previous plugin/device

    list_with_tail internal_queue;
    /* Still undecided events: these events alone don't decide what event is the 1st on the
       queue.*/
    list_with_tail input_queue;  /* Not yet processed at all. Since we wait for external
                                  * events to resume processing (Grab is active-frozen) */
    list_with_tail output_queue; /* We have decided, but externals don't accept, so we keep them. */

public:
    /* we cannot hold only a Bool, since when we have to reconfigure, we need the original
       forked keycode for the release event. */
    Keycode          forkActive[MAX_KEYCODE];

    last_events_type last_events; // history
    int max_last = 10; // can be updated!

    using fork_configuration = ForkConfiguration<Keycode, Time>;

    fork_configuration *config; // list<fork_configuration>

/* The Static state = configuration.
 * This is the matrix with some Time values:
 * using the fact, that valid Keycodes are non zero, we use
 * the 0 column for `code's global values

 * Global      xxxxxxxx unused xxxxxx
 * key-wise   per-pair per-pair ....
 * key-wise   per-pair per-pair ....
 * ....
 */

    int set_last_events_count(const int new_max) // fixme:  lock ??
    {
        mdb("%s: allocating %d events\n", __FUNCTION__, new_max);

        if (max_last > new_max)
        {
            // shrink. todo! in the circular.h!
        }
        else
        {
            last_events.set_capacity(new_max);
        }

        max_last = new_max;
        return 0;
    }

private:
    static Bool
    forkable_p(const fork_configuration* config, Keycode code)
    {
        return (config->fork_keycode[code]);
    }

    static constexpr int BufferLength = 200;

    [[nodiscard]] const char*
    describe_machine_state() const
    {
        static char buffer[BufferLength];

        snprintf(buffer, BufferLength, "%s[%dm%s%s",
                 escape_sequence, 32 + state,
                 state_description[state], color_reset);
        return buffer;
    }

    // PluginInstance* mPlugin;

    fork_configuration** find_configuration_n(int n);

    void log_state(const char* message) const;
    void log_state_and_event(const char* message, const key_event *ev);
    void log_queues(const char* message);
    bool queues_non_empty();

    static void reverse_splice(list_with_tail &pre, list_with_tail &post);

    void try_to_play(Bool force);

    void replay_events(Bool force_also);

    void apply_event_to_suspect(key_event *ev);

    void rewind_machine();
    void activate_fork();

    void
    change_state(fork_state_t new_state)
    {
        state = new_state;
        mdb(" --->%s[%dm%s%s\n", escape_sequence, 32 + new_state,
            state_description[new_state], color_reset);
    }

    void do_confirm_fork_by(key_event *ev);
    void apply_event_to_verify_state(key_event *ev);


    Time key_pressed_too_long(Time current_time);
    Time key_pressed_in_parallel(Time current_time);

   /* Return the keycode into which CODE has forked _last_ time.
   Returns code itself, if not forked. */
    [[nodiscard]] Bool
    key_forked(Keycode code) const {
        return (forkActive[code]);
    }

    void do_confirm_non_fork_by(key_event *ev);
    void apply_event_to_normal(key_event *ev);


    void output_event(key_event* ev);

public:
    // prefix with a space.
    void mdb(const char* format...) const
    {
        if (config->debug) {
            // alloca()
            char* new_format = (char*) malloc(strlen(format) + 2);
            new_format[0] = ' ';
            strcpy(new_format + 1, format);

            va_list argptr;
            va_start(argptr, format);
            environment->vlog(new_format, argptr);
            va_end(argptr);

            free(new_format);
        }
    };

    // without the leading space
    void mdb_raw(const char* format...) const {
        if (config->debug) {
            va_list argptr;
            va_start(argptr, format);
            environment->vlog(format, argptr);
            va_end(argptr);
        }
    };

    ~forkingMachine() {};

    explicit forkingMachine(platformEnvironment* environment)
        : mLock(0),
          environment(environment),
          state(st_normal), suspect(0), verificator_keycode(0), suspect_time(0),
          last_released(KEYCODE_UNUSED), last_released_time(0),
          mDecision_time(0),
          mCurrent_time(0),
          internal_queue("internal"),
          input_queue("input_queue"),
          output_queue("output_queue"),
          config(nullptr) {

        environment->log("ctor: allocating last_events\n");
        last_events.set_capacity(max_last);
        environment->log("ctor: allocated last_events %lu (%lu\n", last_events.size(), max_last);

        environment->log("ctor: resetting forkActive\n");
        for (unsigned char &i: forkActive) {
            i = KEYCODE_UNUSED; /* not active */
        };
        environment->log("ctor: end\n");
    };

    void switch_config(int id);

    int configure_global(int type, int value, Bool set);
    int configure_twins(int type, Keycode key, Keycode twin, int value, Bool set);
    int configure_key(int type, Keycode key, int value, Bool set);

    int dump_last_events_to_client(event_publisher* publisher, int max_requested);

    void step_in_time_locked(Time now);

    void step_by_force();

    void step_by_key(key_event *ev);

    bool step_by_time(Time current_time);

    void accept_event(PlatformEvent* pevent);

    void flush_to_next();

    // calculated:
    [[nodiscard]] Time next_decision_time() const {
        if ((state == st_verify)
            || (state == st_suspect))
        // we are indeed waiting:
            return mDecision_time;
        else return 0;
    }

    bool create_configs();

    void dump_last_events(event_dumper* dumper) const {
#if 0
        std::function<void(const event_dumper&, const archived_event&)> doit0 = &event_dumper::operator();
        // lambda?
        std::function<void(const archived_event&)> doit = std::bind(&event_dumper::operator(), doit, placeholders::_1);
#else
        std::function<void(const archived_event&)> lambda = [dumper](const archived_event& ev){ dumper->operator()(ev); };
#endif

        std::for_each(last_events.begin(),
                      last_events.end() - 1,
                      lambda);
    }

};

// extern int dump_last_events_to_client(PluginInstance* plugin, ClientPtr client, int n);
