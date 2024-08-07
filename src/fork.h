#ifndef _FORK_H_
#define _FORK_H_


extern "C" {
#include <xorg-server.h>

#ifndef MMC_PIPELINE
#error "This is useful only when the xorg-server is configured with --enable-pipeline"
#endif

#include <X11/X.h>
#include <X11/Xproto.h>
#include <xorg/inputstr.h>


#include <X11/Xdefs.h>
#include <xorg/input.h>
#include <xorg/eventstr.h>

#undef xalloc

#undef max
#undef min
}

#include <cstdarg>
#include "config.h"

#include "queue.h"
#include "history.h"


#define plugin_machine(plugin) ((machineRec*)(plugin->data))
#define MALLOC(type)   (type *) malloc(sizeof (type))

#define KEYCODE_UNUSED 0
#define MAX_KEYCODE 256         /* fixme: inherit from xorg! */
typedef int keycode_parameter_matrix[MAX_KEYCODE][MAX_KEYCODE];


/* We can switch between configs. */
typedef struct _fork_configuration fork_configuration;

// todo: use a C++ <list> ?
struct _fork_configuration
{
    /* static data of the machine: i.e.  `configuration' */

private:
    static Time
    get_value_from_matrix (keycode_parameter_matrix matrix, KeyCode code, KeyCode verificator)
    {
        return (matrix[code][verificator]?
                // code/verificator specific:
                matrix[code][verificator]:
                (matrix[code][0]?
                 // default for code:
                 matrix[code][0]:
                 // global fallback
                 matrix[0][0]));
    }

public:
    // note: depending on verificator is strange. There might be none!
    // fork_configuration* config,
    Time verification_interval_of(KeyCode code, KeyCode verificator)
    {
        return get_value_from_matrix(this->verification_interval, code, verificator);
    }
    Time
    overlap_tolerance_of(KeyCode code, KeyCode verificator)
    {
        return get_value_from_matrix(this->overlap_tolerance, code, verificator);
    }

  KeyCode          fork_keycode[MAX_KEYCODE];
  Bool          fork_repeatable[MAX_KEYCODE]; /* True -> if repeat, cancel possible fork. */

  /* we don't consider an overlap, until this ms.
     fixme: we need better. a ration between `before'/`overlap'/`after' */
  keycode_parameter_matrix overlap_tolerance;

  /* After how many m-secs, we decide for the modifier.
     (x,0) just by pressing the X key
     (x,y) pressing x while y also pressed.

     hint: should be around the key-repetition rate (1st pause) */
  keycode_parameter_matrix verification_interval;

  int clear_interval;
  int repeat_max;
  Bool consider_forks_for_repeat;

  int debug;
  const char* name;
  int id;

  fork_configuration*   next;
};



/* states of the automaton: */
typedef enum {
  st_normal,
  st_suspect,
  st_verify,
  st_deactivated,
  st_activated
} fork_state_t;

/* `machine': the dynamic `state' */

// typedef
struct machineRec
{
    typedef my_queue<key_event> list_with_tail;

    /* How we decided for the fork */
    enum {
        reason_total,               // key pressed too long
        reason_overlap,             // key press overlaps with another key
        reason_force                // mouse-button was pressed & triggered fork.
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
        mdb_raw("/--\n");
    }
    void unlock()
    {
        mLock=0;
        mdb_raw("\\__\n");
    }
#endif


private:
    // fork_state_t
    unsigned char state;
    // only for certain states we keep (updated):

    KeyCode suspect;
    KeyCode verificator_keycode;

    // these are "registers"
    Time suspect_time;           /* time of the 1st event in the queue. */
    Time verificator_time = 0;       /* press of the `verificator' */

    /* To allow AR for forkable keys:
     * When we press a key the second time in a row, we might avoid forking:
     * So, this is for the detector:
     *
     * This means I cannot do this trick w/ 2 keys, only 1 is the last/considered! */
    KeyCode last_released; // .- trick
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
    KeyCode          forkActive[MAX_KEYCODE];


    last_events_type *last_events; // history
    int max_last = 100; // can be updated!

    fork_configuration  *config; // list<fork_configuration>

/* The Static state = configuration.
 * This is the matrix with some Time values:
 * using the fact, that valid KeyCodes are non zero, we use
 * the 0 column for `code's global values

 * Global      xxxxxxxx unused xxxxxx
 * key-wise   per-pair per-pair ....
 * key-wise   per-pair per-pair ....
 * ....
 */

    int set_last_events_count(int new_max) // fixme:  lock ??
    {
        mdb("%s: allocating %d events\n", __FUNCTION__, new_max);

        if (max_last > new_max)
        {
            // shrink. todo! in the circular.h!
        }
        else
        {
            last_events->reserve(new_max);
        }

        max_last = new_max;
        return 0;
    }

    void log_event(const key_event *event, DeviceIntPtr keybd);
private:
    static Bool
    forkable_p(const fork_configuration* config, KeyCode code)
    {
        return (config->fork_keycode[code]);
    }

    static constexpr int BufferLength = 200;

    [[nodiscard]] const char*
    describe_machine_state() const
    {
        static char buffer[BufferLength];

        snprintf(buffer, BufferLength, "%s[%dm%s%s",
                 escape_sequence, 32 + this->state,
                 state_description[this->state], color_reset);
        return buffer;
    }

    PluginInstance* mPlugin;

    fork_configuration** find_configuration_n(int n);

    void log_state(const char* message) const;
    void log_state_and_event(const char* message, const key_event *ev);
    void log_queues(const char* message);

    static void reverse_slice(list_with_tail &pre, list_with_tail &post);

    void try_to_play(Bool force);

    void replay_events(Bool force_also);

    void apply_event_to_suspect(key_event *ev);

    void rewind_machine();
    void activate_fork();

    void
    change_state(fork_state_t new_state)
    {
        this->state = new_state;
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
    key_forked(KeyCode code) const {
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
            VErrorF(new_format, argptr);
            va_end(argptr);

            free(new_format);
        }
    };

    // without the leading space
    void mdb_raw(const char* format...) const
    {
        if (config->debug) {
            va_list argptr;
            va_start(argptr, format);
            VErrorF(format, argptr);
            va_end(argptr);
        }
    };


    ~machineRec()
        {
            delete last_events;
        };

    machineRec(PluginInstance* plugin)
        : internal_queue("internal"),
          input_queue("input_queue"),
          output_queue("output_queue"),
          mPlugin(plugin),
          last_released(0),
          mDecision_time(0),
          mCurrent_time(0),
          state(st_normal)
        {
            last_events = new last_events_type(max_last);

            for (unsigned char & i : forkActive){
                i = KEYCODE_UNUSED; /* not active */
            };
        };

    void switch_config(int id);

    void step_in_time_locked(Time now);

    void step_fork_automaton_by_force();

    void step_fork_automaton_by_key(key_event *ev);

    bool step_fork_automaton_by_time(Time current_time);

    void accept_event(key_event* ev);

    void flush_to_next();

    // calculated:
    [[nodiscard]] Time next_decision_time() const {
        if ((state == st_verify)
            || (state == st_suspect))
        // we are indeed waiting:
            return mDecision_time;
        else return 0;
    }

};

extern fork_configuration* machine_new_config();

// extern int dump_last_events_to_client(PluginInstance* plugin, ClientPtr client, int n);

void hand_over_event_to_next_plugin(InternalEvent *event, PluginInstance* plugin);

enum {
  PAUSE_KEYCODE = 127
};

#endif  /* _FORK_H_ */
