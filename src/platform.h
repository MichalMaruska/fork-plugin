//
// Created by michal on 8/10/24.
//

#ifndef PLATFORM_H
#define PLATFORM_H

extern "C" {
// #include <eventstr.h>
}

class PlatformEvent {
    // abstract
};

#include "../include/archived_event.h"

class platformEnvironment {
public:
    platformEnvironment() = default;

    virtual bool press_p(const PlatformEvent* event) = 0;
    virtual Time time_of(const PlatformEvent* event) = 0;
    virtual KeyCode detail_of(const PlatformEvent* event) = 0;

    virtual bool ignore_event(const PlatformEvent *pevent) = 0;
    //!(mouse_emulation_on(keybd))

    virtual bool output_frozen() = 0;
    // virtual void hand_over_event_to_next_plugin(PlatformEvent* event) = 0;
    virtual void output_event(PlatformEvent* pevent) = 0;

    virtual void log(const char* format...) = 0;
    virtual void log_event(const PlatformEvent *event) = 0;

    virtual void archive_event(PlatformEvent* pevent, archived_event* archived_event) = 0;
    virtual void free_event(PlatformEvent* pevent) = 0;
    virtual void rewrite_event(PlatformEvent* pevent, KeyCode code) = 0;

 // do not generate it:?
    virtual ~platformEnvironment() {};
    // = default;
};


#endif //PLATFORM_H
