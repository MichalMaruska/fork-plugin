#ifndef XORG_H
#define XORG_H

#include "platform.h"

extern "C" {
#include <xorg/events.h>
}

extern "C" {
    // /usr/include/xorg/xorg-server.h
#include <xorg-server.h>

#ifndef MMC_PIPELINE
#error "This is useful only when the xorg-server is configured with --enable-pipeline"
#endif

    // _XSERVER64
#include <X11/X.h>
#include <X11/Xproto.h>
#include <xorg/inputstr.h>

#include <xorg/xkbsrv.h>
#include <X11/Xdefs.h>
#include <xorg/input.h>
#include <xorg/eventstr.h>

#undef xalloc

#undef max
#undef min
}


// extern "C"
extern void hand_over_event_to_next_plugin(InternalEvent *event, PluginInstance* const nextPlugin);

class XorgEvent : public PlatformEvent {
public:
    const InternalEvent* event;
};


class XOrgEnvironment : public platformEnvironment {
private:
    const DeviceIntPtr keybd;
    PluginInstance* const nextPlugin;
    // how to ctor for those 2 members?
    // XOrgEnvironment()
public:
    XOrgEnvironment(const DeviceIntPtr keybd, PluginInstance* const nextPlugin) :
    keybd(keybd), nextPlugin(nextPlugin){};

    virtual ~XOrgEnvironment() = default;

    virtual void log_event(const PlatformEvent *event) override;


    bool output_frozen() override {
        return plugin_frozen(nextPlugin);
    };

    KeyCode detail_of(const PlatformEvent* pevent) {
        auto event = static_cast<const XorgEvent*>(pevent)->event;
        return event->device_event.detail.key;
        };

    bool ignore_event(const PlatformEvent *pevent) override {
        // mouse_emulation_on(DeviceIntPtr keybd)
        auto event = static_cast<const XorgEvent*>(pevent)->event;
            if (!keybd->key) {
                ErrorF("%s: keybd is null!", __func__);
                return 0;
            }

            XkbSrvInfoPtr xkbi= keybd->key->xkbInfo;
            XkbDescPtr xkb = xkbi->desc;
            return (xkb->ctrls->enabled_ctrls & XkbMouseKeysMask);
    }
    //
    virtual void archive_event(PlatformEvent* pevent, archived_event* archived_event) {

        auto event = static_cast<XorgEvent*>(pevent)->event;
        // dynamic_cast

        DB("%s:%d type: %d\n", __func__, __LINE__, event->any.type);
        DB("%s:%d keycode: %d\n", __func__, __LINE__, event->device_event.detail.key);
        DB("%s:%d keycode via function: %d\n", __func__, __LINE__, detail_of2()); // ev->event

#if 0
        archived_event->key = detail_of1(event);
        DB("%s: %d\n", __func__, __LINE__);
        archived_event->time = time_of(event);
        DB("%s: %d\n", __func__, __LINE__);
        archived_event->press = press_p(event);
        DB("%s: %d\n", __func__, __LINE__);
        archived_event->forked = ev->forked;
#endif
    };

    virtual void rewrite_event(PlatformEvent* pevent, KeyCode code) {
        auto event = static_cast<XorgEvent*>(pevent)->event;
        event->device_event.detail.key = code;
    }

    virtual void free_event(PlatformEvent* pevent) {
        auto event = static_cast<XorgEvent*>(pevent)->event;
        mxfree(event, event->any.length);
    }

    virtual bool press_p(const PlatformEvent* pevent) {
        auto event = static_cast<const XorgEvent*>(pevent)->event;
        return (event->any.type == ET_KeyPress);
    }
    virtual Time time_of(const PlatformEvent* pevent) {
        auto event = static_cast<const XorgEvent*>(pevent)->event;
        return event->any.time;
    }

    virtual void output_event(PlatformEvent* pevent) {
        auto event = static_cast<XorgEvent*>(pevent)->event;
        ::hand_over_event_to_next_plugin(event, nextPlugin);
    };
#if 0
    virtual void hand_over_event_to_next_plugin(PlatformEvent *pevent) override {
        auto event = static_cast<XorgEvent*>(pevent)->event;
        ::hand_over_event_to_next_plugin(event, nextPlugin);
    }
#endif


    virtual void log(const char* fmt ...) {
        ErrorF(fmt, ##__VA_ARGS__);
    }
};

#endif //XORG_H
