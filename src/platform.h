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
    virtual bool output_frozen();
    virtual void hand_over_event_to_next_plugin(PlatformEvent* event);
    virtual bool press_p(const PlatformEvent* event);
    virtual void log(const char* format...);
    //
    virtual Time time_of(const PlatformEvent* event);
    virtual KeyCode detail_of(const PlatformEvent* event);
    virtual void log_event(const PlatformEvent *event);

    virtual void copy_event(PlatformEvent* pevent, archived_event* archived_event);
    virtual void free_event(PlatformEvent* pevent);
    virtual void rewrite_event(PlatformEvent* pevent, KeyCode code);
    virtual void output_event(PlatformEvent* pevent);
};


#endif //PLATFORM_H
