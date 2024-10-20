#pragma once

/**
 * Created by michal on 8/10/24.
 *
 */


extern "C" {
// todo: archived_event coud be avoided ... it's X specific
#include "fork_requests.h"
#undef max
#undef min
}

#include <string>
#include <memory>
class PlatformEvent {};

struct key_event {
    PlatformEvent* p_event;
    KeyCode forked; /* if forked to (another keycode), this is the original key */
    key_event(PlatformEvent* p) : p_event(p){};
    ~key_event() {
        // should be nullptr
        free(p_event);
    }
};

class event_dumper {
    public:
    virtual void operator() (const archived_event& event) = 0;
    virtual ~event_dumper() {};
};

class event_publisher {
    public:
    virtual void prepare(int max_events) = 0;
    virtual int commit() = 0;
    virtual void event(const archived_event& event) = 0;
    virtual ~event_publisher() {};
};



// fixme: template on <Keycode, Time> ?
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
    virtual std::string fmt_event(const PlatformEvent *pevent) = 0;

    virtual archived_event archive_event(const key_event& event) = 0;
    virtual void free_event(PlatformEvent* pevent) = 0;
    virtual void rewrite_event(PlatformEvent* pevent, KeyCode code) = 0;


    virtual std::unique_ptr<event_dumper> get_event_dumper() = 0;

    virtual ~platformEnvironment() = default;
};
