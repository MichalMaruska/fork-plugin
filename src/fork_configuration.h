#pragma once

#define KEYCODE_UNUSED 0
#define MAX_KEYCODE 256         /* fixme: inherit from xorg! */
// using?
typedef int keycode_parameter_matrix[MAX_KEYCODE][MAX_KEYCODE];


// namespace fork {
// todo: use a C++ <list> ?
template <typename KeyCode, typename Time>
class ForkConfiguration {
private:
    // declaration, not definition!
    static int config_counter;

public:
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

    int clear_interval = 0;
    int repeat_max  = 80;
    Bool consider_forks_for_repeat = true;

    int debug = 1; // todo: boolean?
    const char* name = "default";
    int id;

    // valid?
    ForkConfiguration*   next = nullptr;


private:
    static Time
    get_value_from_matrix(keycode_parameter_matrix matrix, KeyCode code, KeyCode verificator) {
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
    ForkConfiguration() : id(config_counter++) {

        // use bzero!
        for (int i = 0; i < 256; i++) {
            // local timings:  0 = use global timing
            for (int j = 0; j < 256; j++) { /* 1 ? */
                overlap_tolerance[i][j] = 0;
                verification_interval[i][j] = 0;
            };
            fork_keycode[i] = 0;
            /*  config->forkCancel[i] = 0; */
            fork_repeatable[i] = FALSE;
            /* repetition is supported by default (not ignored)  True False*/
        }
        /* ms: could be XkbDfltRepeatDelay */

        // Global fallback:
        verification_interval[0][0] = 200;
        overlap_tolerance[0][0] = 100;
    }

    // note: depending on verificator is strange. There might be none!
    // fork_configuration* config,
    Time verification_interval_of(KeyCode code, KeyCode verificator) {
        return get_value_from_matrix(this->verification_interval, code, verificator);
    }

    Time
    overlap_tolerance_of(KeyCode code, KeyCode verificator) {
        return get_value_from_matrix(this->overlap_tolerance, code, verificator);
    }
};

