#ifndef _FORK_H_
#define _FORK_H_


extern "C" {
#include <xorg-server.h>

#ifndef MMC_PIPELINE
#error "This is useful only when the xorg-server is configured with --enable-pipeline"
#endif

#include <X11/X.h>
#include <X11/Xproto.h>
#include <X11/keysym.h>
#include <xorg/inputstr.h>


#include <X11/Xdefs.h>
#include <stdint.h>
#include <xorg/input.h>
#include <xorg/eventstr.h>

// include/os.h
#undef xalloc

#include "fork_requests.h"
}

#include <cstdarg>
#include "config.h"

// #include "debug.h"

#include "queue.h"
#include "history.h"


#define plugin_machine(plugin) ((machineRec*)(plugin->data))
#define MALLOC(type)   (type *) malloc(sizeof (type))
#define MAX_KEYCODE 256         /* fixme: inherit from xorg! */
typedef int keycode_parameter_matrix[MAX_KEYCODE][MAX_KEYCODE];


/* We can switch between configs. */
typedef struct _fork_configuration fork_configuration;

struct _fork_configuration
{
    /* static data of the machine: i.e.  `configuration' */

private:
    static Time
    get_value_from_matrix (keycode_parameter_matrix matrix, KeyCode code, KeyCode verificator)
        {
            return (matrix[code][verificator]?
                    matrix[code][verificator]:
                    (matrix[code][0]?
                     matrix[code][0]: matrix[0][0]));
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

  /* after how many m-secs, we decide for the modifier.
     Should be around the key-repeatition rate (1st pause) */
  keycode_parameter_matrix verification_interval;

  int clear_interval;
  int repeat_max;
  Bool consider_forks_for_repeat;
  int debug;

  const char*  name;
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

extern char const *state_description[];

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

    /* used only for debugging */
    static char const *state_description[];

    volatile int lock;           /* the mouse interrupt handler should ..... err!  `volatile'
                                  * useless mmc!  But i want to avoid any caching it.... SMP ??*/
    // fork_state_t
    unsigned char state;

    /* To allow AR for forkable keys:
     * When we press a key the second time in a row, we might avoid forking:
     * So, this is for the detector:
     *
     * This means I cannot do this trick w/ 2 keys, only 1 is the last/considered! */
    KeyCode last_released; // .- trick
    int last_released_time;

    KeyCode suspect;
    KeyCode verificator;

    // these are "registers"
    Time suspect_time;           /* time of the 1st event in the queue. */
    Time verificator_time;       /* press of the `verificator' */
    // calculated:
    Time decision_time;         /* Time to wait... so that the current event queue could decide more*/
    Time current_time;

    /* we cannot hold only a Bool, since when we have to reconfigure, we need the original
       forked keycode for the release event. */
    KeyCode          forkActive[MAX_KEYCODE];

    list_with_tail internal_queue;
    /* Still undecided events: these events alone don't decide what event is the 1st on the
       queue.*/
    list_with_tail input_queue;  /* Not yet processed at all. Since we wait for external
                                  * events to resume processing (Grab is active-frozen) */
    list_with_tail output_queue; /* We have decided, but externals don't accept, so we keep them. */

    last_events_type *last_events; // history
    int max_last = 100; // can be updated!

    fork_configuration  *config;

/* The Static state = configuration.
 * This is the matrix with some Time values:
 * using the fact, that valid KeyCodes are non zero, we use
 * the 0 column for `code's global values

 * Global      xxxxxxxx unused xxxxxx
 * key-wise   per-pair per-pair ....
 * key-wise   per-pair per-pair ....
 * ....
 */

private:
    static Bool
    forkable_p(fork_configuration* config, KeyCode code)
    {
        return (config->fork_keycode[code]);
    }

public:

    static const int BufferLength = 200;
    const char*
    describe_machine_state()
    {
        static char buffer[BufferLength];

        snprintf(buffer, BufferLength, "%s[%dm%s%s",
                 escape_sequence, 32 + this->state,
                 state_description[this->state], color_reset);
        return buffer;
    }

    PluginInstance* mPlugin;

public:

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
          decision_time(0),
          current_time(0),
          state(st_normal)
        {
            last_events = new last_events_type(max_last);

            for (int i=0;i<256;i++){                   // keycode 0 is unused!
                forkActive[i] = 0; /* 0 = not active */
            };
        };

    void rewind_machine();

    void activate_fork(PluginInstance* plugin);
    void step_fork_automaton_by_force(PluginInstance* plugin);
    void apply_event_to_suspect(key_event *ev, PluginInstance* plugin);
    void step_fork_automaton_by_key(key_event *ev, PluginInstance* plugin);

    bool step_fork_automaton_by_time(PluginInstance* plugin, Time current_time);

    void mdb(const char* format...)
    {
        va_list argptr;
        // if (config->debug) {ErrorF(x, ...);}
        va_start(argptr, format);
        VErrorF(format, argptr);
        va_end(argptr);
    };

    void
    change_state(fork_state_t new_state)
    {
        this->state = new_state;
        mdb(" --->%s[%dm%s%s\n", escape_sequence, 32 + new_state,
            state_description[new_state], color_reset);
    }

    void do_enqueue_event(key_event *ev);
    void do_confirm_fork(key_event *ev, PluginInstance* plugin);
    void apply_event_to_verify(key_event *ev, PluginInstance* plugin);

    Bool key_forked(KeyCode code);

    void do_confirm_non_fork_by(key_event *ev, PluginInstance* plugin);
    void apply_event_to_normal(key_event *ev, PluginInstance* plugin);

    Time key_pressed_too_long(Time current_time);
    Time key_pressed_in_parallel(Time current_time);

    static void reverse_slice(list_with_tail &pre, list_with_tail &post);

    void try_to_output(PluginInstance* plugin);
    void output_event(key_event* ev, PluginInstance* plugin);
};

extern fork_configuration* machine_new_config(void);
extern void machine_switch_config(PluginInstance* plugin, machineRec* machine,int id);
extern int machine_set_last_events_count(machineRec* machine, int new_max);
extern void replay_events(PluginInstance* plugin, Bool force);

extern int dump_last_events_to_client(PluginInstance* plugin, ClientPtr client, int n);

void hand_over_event_to_next_plugin(InternalEvent *event, PluginInstance* plugin);

enum {
  PAUSE_KEYCODE = 127
};



// I want to track the memory usage, and warn when it's too high.
extern size_t memory_balance;

inline
void* mmalloc(size_t size)
{
  void* p = malloc(size);
  if (p)
    {
      memory_balance += size;
      if (memory_balance > sizeof(machineRec) + sizeof(PluginInstance) + 2000)
        ErrorF("%s: memory_balance = %ld\n", __FUNCTION__, memory_balance);
    }
  return p;
}

// #pragma message ( "Debug configuration - OK" )
inline
void
mxfree(void* p, size_t size)
{
  memory_balance -= size;
  free(p);
}

#endif  /* _FORK_H_ */
