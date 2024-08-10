//
// Created by michal on 8/10/24.
//

#ifndef PLATFORM_H
#define PLATFORM_H

extern "C" {
#include <eventstr.h>
}

class PlatformEvent {
    // abstract
};

class XorgEvent : public PlatformEvent {
public:
    const InternalEvent* event;
};

class platformEnvironment {
    virtual bool output_frozen();
    virtual void hand_over_event_to_next_plugin(PlatformEvent* event);
    virtual bool press_p(const PlatformEvent* event);
    virtual void log(const char* format...);
    //
    virtual void log_event(const PlatformEvent *event);
    virtual void copy_event(PlatformEvent* pevent, archived_event* archived_event);
};


#endif //PLATFORM_H
