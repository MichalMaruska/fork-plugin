//
// Created by michal on 8/10/24.
//

#ifndef PLATFORM_H
#define PLATFORM_H

extern "C" {
// #include <eventstr.h>
    // archived_event
#include "fork_requests.h"
#undef max
#undef min
}
// #include "../include/archived_event.h"

class PlatformEvent {
    // abstract
};


class platformEnvironment {
public:
    platformEnvironment() = default;

    virtual bool press_p(const PlatformEvent* event) = 0;
    virtual bool release_p(const PlatformEvent* event) = 0;
    virtual Time time_of(const PlatformEvent* event) = 0;
    virtual KeyCode detail_of(const PlatformEvent* event) = 0;

    virtual bool ignore_event(const PlatformEvent *pevent) = 0;

    virtual bool output_frozen() = 0;
    virtual void relay_event(PlatformEvent* pevent) = 0;
    virtual void push_time(Time now) = 0;

    virtual void log(const char* format...) = 0;
    virtual void vlog(const char* format, va_list argptr) = 0;
    virtual void log_event(const std::string &message, const PlatformEvent *event) = 0;

    virtual void archive_event(PlatformEvent* pevent, archived_event* archived_event) = 0;
    virtual void free_event(PlatformEvent* pevent) = 0;
    virtual void rewrite_event(PlatformEvent* pevent, KeyCode code) = 0;

 // do not generate it:?
    virtual ~platformEnvironment() {};
    // = default;
};


#endif //PLATFORM_H
