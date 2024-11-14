#pragma once

namespace forkNS {

// these as template non-type parameters?
#define KEYCODE_UNUSED 0

// Keycode must be integral/numeric,  MAX_KEYCODE is limiti.
template <typename Keycode, typename Time, int MAX_KEYCODE>
class ForkConfiguration {

private:
    typedef Time keycode_parameter_matrix[MAX_KEYCODE][MAX_KEYCODE];
    // declaration, not definition!
#if MULTIPLE_CONFIGURATIONS
    static inline int config_counter;
#endif
public:
    Keycode          fork_keycode[MAX_KEYCODE];
    bool          fork_repeatable[MAX_KEYCODE]; /* True -> if repeat, cancel possible fork. */

    /* we don't consider an overlap, until this ms.
       fixme: we need better. a ration between `before'/`overlap'/`after' */
    keycode_parameter_matrix overlap_tolerance;

    /* After how many m-secs, we decide for the modifier.
       (x,0) just by pressing the X key
       (x,y) pressing x while y also pressed.

       hint: should be around the key-repetition rate (1st pause) */
    keycode_parameter_matrix verification_interval;

    int clear_interval = 0;
    Time repeat_max  = 80;
    bool consider_forks_for_repeat = true;

    int debug = 1; // todo: boolean?
    int id;

    // valid?
    ForkConfiguration*   next = nullptr;


private:
    static Time
    get_value_from_matrix(keycode_parameter_matrix matrix, Keycode code, Keycode verificator) {
        // code/verificator specific:
        return (matrix[code][verificator]?:
                 // default for code:
                (matrix[code][0]?: // 0 is no_key
                 // global fallback
                 matrix[0][0]));
    }

public:
    ForkConfiguration()
#if MULTIPLE_CONFIGURATIONS
        :  id(config_counter++)
#endif
    {
            // use bzero!
        for (int i = 0; i < 256; i++) {
            // local timings:  0 = use global timing
            for (int j = 0; j < 256; j++) { /* 1 ? */
                overlap_tolerance[i][j] = 0;
                verification_interval[i][j] = 0;
            };
            fork_keycode[i] = KEYCODE_UNUSED;
            /*  config->forkCancel[i] = 0; */
            fork_repeatable[i] = false;
            /* repetition is supported by default (not ignored)  True False*/
        }
        /* ms: could be XkbDfltRepeatDelay */

        // Global fallback:
        verification_interval[0][0] = 200;
        overlap_tolerance[0][0] = 100;
    }

    // note: depending on verificator is strange. There might be none!
    // fork_configuration* config,
    Time verification_interval_of(Keycode code, Keycode verificator) {
        return get_value_from_matrix(this->verification_interval, code, verificator);
    }

    Time
    overlap_tolerance_of(Keycode code, Keycode verificator) {
        return get_value_from_matrix(this->overlap_tolerance, code, verificator);
    }


    /** return 0 if enough, otherwise the time when it will be enough/proving a fork.
     * dangerous to name it current_time, like the member variable!
     */
    Time
    verifier_decision_time(Time current_time,
                           Keycode suspect, Time suspect_time,
                           Keycode verificator_keycode, Time verificator_time) {
        // Given the 2 keys (pressed), and `current_time'
        // todo:
        Time verification_interval = verification_interval_of(suspect, verificator_keycode);
        if (suspect_time + verification_interval <= current_time) {
            return 0;
        }

        // verify overlap
        int overlap_tolerance = overlap_tolerance_of(suspect, verificator_keycode);
        Time decision_point_time =  verificator_time + overlap_tolerance;

        if (decision_point_time <= current_time) {
            // already "parallel"
            return 0;
        } else {
#if 0
            mdb("time: overlay interval = %dms elapsed so far =%dms\n",
                overlap_tolerance,
                (int) (current_time - verificator_time));

            mdb("suspected = %d, verificator_keycode %d. Times: overlap %" TIME_FMT ", "
                "still needed: %" TIME_FMT " (ms)\n", suspect, verificator_keycode,
                current_time - verificator_time,
                decision_point_time - current_time);
#endif
            return decision_point_time;
        }
    }

};

}

