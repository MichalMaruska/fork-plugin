#pragma once


#include <cstdarg>
#include "config.h"
#include <algorithm>
#include <functional>

#include "queue.h"
#include "platform.h"
#include "colors.h"
#include <boost/circular_buffer.hpp>
#include <memory>


#include "fork_configuration.h"

// namespace fork {

/* fixme: inherit from xorg! */
constexpr int MAX_KEYCODE=256;




// typename PlatformEvent, typename platformEnvironment,
template <typename Keycode, typename Time, typename archived_event_t>
// so key_event
class forkingMachine
{

public:
    typedef boost::circular_buffer<archived_event_t> last_events_t;
    // history:
    typedef platformEnvironment1<Keycode, Time, archived_event_t> platformEnvironment;

    /* `machine': the dynamic `state' */
    struct key_event {
        PlatformEvent* p_event;
        Keycode forked; /* if forked to (another keycode), this is the original key */
        key_event(PlatformEvent* p) : p_event(p){};
        ~key_event() {
            // should be nullptr
            // bug: must call environment -> free_event()
            free(p_event);
        }
    };

    typedef my_queue<key_event*> list_with_tail;

private:
    /* states of the automaton: */
    constexpr static const Keycode no_key = 0;
    enum fork_state_t {  // states of the automaton
        st_normal,
        st_suspect,
        st_verify,
        st_deactivated,
        st_activated
    };

    /* How we decided for the fork */
    enum class fork_reason {
        reason_total,               // key pressed too long
        reason_overlap,             // key press overlaps with another key
        reason_force                // mouse-button was pressed & triggered fork.
    };

    /* used only for debugging */
    static constexpr
    char const * const state_description[5] = {
        // map<fork_state_t, string>
        "normal",
        "suspect",
        "verify",
        "deactivated",
        "activated"
    };

    // atomic?
    volatile int mLock;           /* the mouse interrupt handler should ..... err!  `volatile'
                                  *
                                  * useless mmc!  But i want to avoid any caching it.... SMP ??*/
public:
    std::unique_ptr<platformEnvironment> environment;
#if USE_LOCKING
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
    /* Still undecided events: these events alone don't decide
       how to interpret the first event on the queue.*/
    list_with_tail input_queue;  /* Not yet processed at all. Since we wait for external
                                  * events to resume processing (Grab is active-frozen) */
    list_with_tail output_queue; /* We have decided, so possibly modified events (forked),
                                  * Externals don't accept, so we keep them. */

public:
    /* we cannot hold only a Bool, since when we have to reconfigure, we need the original
       forked keycode for the release event. */
    Keycode          forkActive[MAX_KEYCODE];

private:
    last_events_t last_events_log;
    int max_last = 10; // can be updated!

    using fork_configuration = ForkConfiguration<Keycode, Time, MAX_KEYCODE>;

public:
    std::unique_ptr<fork_configuration> config; // list<fork_configuration>

/* The Static state = configuration.
 * This is the matrix with some Time values:
 * using the fact, that valid Keycodes are non zero, we use
 * the 0 column for `code's global values

 * Global      xxxxxxxx unused xxxxxx
 * key-wise   per-pair per-pair ....
 * key-wise   per-pair per-pair ....
 * ....
 */

private:
    int set_last_events_count(const int new_max) {
        // fixme:  lock ??
        mdb("%s: allocating %d events\n", __func__, new_max);

        if (max_last > new_max) {
            // shrink. todo! in the circular.h!
            mdb("%s: unimplemented\n", __func__);
        } else {
            last_events_log.set_capacity(new_max);
        }

        max_last = new_max;
        return 0;
    }

    void check_locked() const {
        assert(mLock);
    }
    void check_unlocked() const {
        assert(mLock == 0);
    }

    static bool
    forkable_p(const fork_configuration* config, Keycode code)
    {
        return (config->fork_keycode[code]);
    }

    static constexpr int BufferLength = 200;

    [[nodiscard]]
    static const char*
    describe_machine_state(fork_state_t state) {
        static char buffer[BufferLength];

        snprintf(buffer, BufferLength, "%s[%dm%s%s",
                 escape_sequence, 32 + state,
                 state_description[state], color_reset);
        return buffer;
    }

    // PluginInstance* mPlugin;

    fork_configuration** find_configuration_n(int n);

    bool queues_non_empty() const;

    static void reverse_splice(list_with_tail &pre, list_with_tail &post);

    void try_to_play(bool force);

    void replay_events(bool force_also);

    void apply_event_to_suspect(std::unique_ptr<key_event> ev);

    void rewind_machine();
    void activate_fork();

    void
    change_state(fork_state_t new_state)
    {
        state = new_state;
        mdb(" --->%s[%dm%s%s\n", escape_sequence, 32 + new_state,
            state_description[new_state], color_reset);
    }

    void do_confirm_fork_by(std::unique_ptr<key_event> ev);
    void apply_event_to_verify_state(std::unique_ptr<key_event> ev);


    Time key_pressed_too_long(Time current_time);
    Time key_pressed_in_parallel(Time current_time);

   /* Return the keycode into which CODE has forked _last_ time.
   Returns code itself, if not forked. */
    [[nodiscard]] bool
    key_forked(Keycode code) const {
        return (forkActive[code]);
    }

    void do_confirm_non_fork_by(std::unique_ptr<key_event> ev);
    void apply_event_to_normal(std::unique_ptr<key_event> ev);


    void output_event(std::unique_ptr<key_event> ev);

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
        last_events_log.set_capacity(max_last);
        environment->log("ctor: allocated last_events %lu (%lu\n", last_events_log.size(), max_last);

        environment->log("ctor: resetting forkActive\n");
        for (unsigned char &i: forkActive) {
            i = KEYCODE_UNUSED; /* not active */
        };
        environment->log("ctor: end\n");
    };
#if MULTIPLE_CONFIGURATIONS
    void switch_config(int id);
#endif
    int configure_global(int type, int value, bool set);
    int configure_twins(int type, Keycode key, Keycode twin, int value, bool set);
    int configure_key(int type, Keycode key, int value, bool set);

    int dump_last_events_to_client(event_publisher<archived_event_t>* publisher, int max_requested);

    void step_in_time_locked(Time now);

    void step_by_force();

    void step_by_key(std::unique_ptr<key_event> ev);

    bool step_by_time(Time current_time);

    void accept_event(PlatformEvent* pevent);

    void flush_to_next();

    // calculated:
    [[nodiscard]] Time next_decision_time() const {
        check_locked();
        if ((state == st_verify)
            || (state == st_suspect))
        // we are indeed waiting:
            return mDecision_time;
        else return 0;
    }

    /** Create 2 configuration sets:
        0. w/o forking,  no-op.
        1. user-configurable
        this is on loading, so should not use Abort allocation policy:

        @return false if allocation  failed.
    */
    bool create_configs() {
        environment->log("%s\n", __func__);

        try {
#if MULTIPLE_CONFIGURATIONS
            auto config_no_fork = std::unique_ptr<fork_configuration>(new fork_configuration);
            config_no_fork->debug = 0; // should be settable somehow.

            auto user_configurable = std::unique_ptr<fork_configuration>(new fork_configuration);
            user_configurable->debug = 1;

            // todo:
            // user_configurable->next = config_no_fork.release();

            config = user_configurable.release();
#else
            config = std::make_unique<fork_configuration>();
#endif
            return true;

        } catch (std::bad_alloc &exc) {
            return false;
        }
    }

    void dump_last_events(event_dumper<archived_event_t>* dumper) const {
#if 0
        std::function<void(const event_dumper&, const archived_event_t&)> doit0 = &event_dumper::operator();
        // lambda?
        std::function<void(const archived_event_t&)> doit = std::bind(&event_dumper::operator(), doit, placeholders::_1);
#else
        std::function<void(const archived_event_t&)> lambda = [dumper](const archived_event_t& ev){ dumper->operator()(ev); };
#endif
        if (last_events_log.full()) {
            std::for_each(last_events_log.begin(),
                      last_events_log.end() - 1,
                      lambda);
        } else {
            std::for_each(last_events_log.begin(),
                          last_events_log.begin() + last_events_log.size(),
                          lambda);
        }
        environment->log("not sure %s\n", __func__);
    }

private:
    /**
     * Logging ... why templated?
     */
    void log_state(const char *message) const {
        mdb("%s%s%s state: %s, queue: %d.  %s\n", fork_color, __func__, color_reset,
            describe_machine_state(this->state), internal_queue.length(), message);
    }

    void log_queues(const char *message) const {
        mdb("%s: Queues: output: %d\t internal: %d\t input: %d\n", message,
            output_queue.length(), internal_queue.length(), input_queue.length());
    }

    void log_state_and_event(const char* message, const key_event *ev) {

        mdb("%s%s%s state: %s, queue: %d\n", // , event: %d %s%c %s %s
            info_color,message,color_reset,
            describe_machine_state(this->state),
            internal_queue.length ()
            );
        environment->fmt_event(ev->p_event);
    }
};



// extern int dump_last_events_to_client(PluginInstance* plugin, ClientPtr client, int n);
