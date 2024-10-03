#pragma once

#include "platform.h"

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
#include "history.h"
#include <string>

extern void hand_over_event_to_next_plugin(InternalEvent *event, PluginInstance* nextPlugin);


class XorgEvent : public PlatformEvent {
public:
    // take ownership:
    XorgEvent(InternalEvent* event) : event(event) {};
    // so why not UniquePointer?
    InternalEvent* event;
};


class XOrgEnvironment : public platformEnvironment {
private:
    const DeviceIntPtr keybd; // reference
    PluginInstance* const plugin;

public:
    XOrgEnvironment(const DeviceIntPtr keybd, PluginInstance* plugin): keybd(keybd), plugin(plugin){};
    // should I just assert(keybd)

    virtual ~XOrgEnvironment() = default;

    bool output_frozen() override {
        const PluginInstance* const nextPlugin = plugin->next;
        return plugin_frozen(nextPlugin);
    };

    KeyCode detail_of(const PlatformEvent* pevent) override {
        auto event = static_cast<const XorgEvent*>(pevent)->event;
        return event->device_event.detail.key;
    };

    bool ignore_event(const PlatformEvent *pevent) override {
        if (!keybd || !keybd->key) {
            // should I just assert(keybd)
            ErrorF("%s: keybd is wrong!", __func__);
            return false;
        }

        XkbSrvInfoPtr xkbi= keybd->key->xkbInfo;
        XkbDescPtr xkb = xkbi->desc;
        return (xkb->ctrls->enabled_ctrls & XkbMouseKeysMask);
    }


    // so this is orthogonal? platform-independent?
    archived_event archive_event(const key_event& event) override {
        PlatformEvent* pevent = event.p_event;

        archived_event archived_event;

#if DEBUG > 1
        auto xevent = static_cast<XorgEvent*>(pevent)->event;
        // dynamic_cast

        log("%s:%d type: %d\n", __func__, __LINE__, xevent->any.type);
        log("%s:%d keycode: %d\n", __func__, __LINE__, detail_of(pevent));
        log("%s:%d keycode via function: %d\n", __func__, __LINE__, detail_of(pevent));
#endif
        archived_event.key = detail_of(pevent);
        archived_event.time = time_of(pevent);
        archived_event.press = press_p(pevent);
        archived_event.forked = event.forked;

        return archived_event;
    };

    virtual void rewrite_event(PlatformEvent* pevent, KeyCode code) override {
        auto event = static_cast<XorgEvent*>(pevent)->event;
        event->device_event.detail.key = code;
    }

    virtual void free_event(PlatformEvent* pevent) override {
        auto event = static_cast<XorgEvent*>(pevent)->event;
        free(event);
    }
    virtual bool press_p(const PlatformEvent* pevent) override {
        auto event = static_cast<const XorgEvent*>(pevent)->event;
        return (event->any.type == ET_KeyPress);
    }
    virtual bool release_p(const PlatformEvent* pevent) override {
        auto event = static_cast<const XorgEvent*>(pevent)->event;
        return (event->any.type == ET_KeyRelease);
    }
    virtual Time time_of(const PlatformEvent* pevent) override {
        auto event = static_cast<const XorgEvent*>(pevent)->event;
        return event->any.time;
    }

    virtual void relay_event(PlatformEvent* pevent) override {
        auto event = static_cast<XorgEvent*>(pevent)->event;
        PluginInstance* nextPlugin = plugin->next;
        hand_over_event_to_next_plugin(event, nextPlugin);
    };

    virtual void push_time(Time now) override {
        PluginInstance* nextPlugin = plugin->next;
        ErrorF("%s: %" TIME_FMT "\n", __func__, now);
        PluginClass(nextPlugin)->ProcessTime(nextPlugin, now);
    }

    virtual void log(const char* format ...) override {
        va_list argptr;
        va_start(argptr, format);
        VErrorF(format, argptr);
        va_end(argptr);
    }

    virtual void vlog(const char* format, va_list argptr) override {
        VErrorF(format, argptr);
    }

    virtual std::string fmt_event(const PlatformEvent *pevent) override {
        // const auto event = (static_cast<const XorgEvent *>(pevent))->event;
#if 0
        const KeyCode key = detail_of(pevent);
        bool press = press_p(pevent);
        bool release = release_p(pevent);

        //event->device_event.detail.key;
        // KeyCode key = detail_of(event);
#if DEBUG > 1
        log("%s: trying to resolve to keysym %d through %p\n", __func__, key, keybd);
#endif
        if (keybd->key) {
            XkbSrvInfoPtr xkbi = keybd->key->xkbInfo;
            KeySym *sym = XkbKeySymsPtr(xkbi->desc, key);
            if ((!sym) || (!isalpha(*(unsigned char *) sym)))
                sym = (KeySym *) " ";

            log("%s: ", key,
                key_color, (char) *sym, color_reset,
                (press ? "down" : (release ) ? "up" : "??"));
        }
#endif
        return "";
    };
};



// prints into the Xorg.*.log
static void
dump_event(KeyCode key, KeyCode fork, bool press, Time event_time,
           XkbDescPtr xkb, XkbSrvInfoPtr xkbi, Time prev_time)
{
    if (key == 0)
        return;
    DB("%s: %d %.4s\n", __func__,
       key,
       xkb->names->keys[key].name);

    // 0.1   keysym bound to the key:
    KeySym* sym= XkbKeySymsPtr(xkbi->desc,key); // mmc: is this enough ?
    [[maybe_unused]] char* sname = nullptr;

    if (sym){
#if 0
        // todo: fixme!
        sname = XkbKeysymText(*sym,XkbCFile); // doesn't work inside server !!
#endif
        // my ascii hack
        if (! isalpha(* reinterpret_cast<unsigned char *>(sym))){
            sym = (KeySym*) " ";
        } else {
            static char keysymname[15];
            sprintf(keysymname, "%c",(* (char*)sym)); // fixme!
            sname = keysymname;
        };
    };
    /*  Format:
        keycode
        press/release
        [  57 4 18500973        112
        ] 33   18502021        1048
    */

    DB("%s %d (%d)",
       (press?" ]":"[ "),
       static_cast<int>(key), static_cast<int>(fork));
    DB(" %.4s (%5.5s) %" TIME_FMT "\t%" TIME_FMT "\n",
           sname, sname,
           event_time,
           event_time - prev_time);
}

// Closure
class xorg_event_dumper : public event_dumper
{
private:
    DeviceIntPtr keybd;
    XkbSrvInfoPtr xkbi;
    XkbDescPtr xkb;

    Time previous_time;

public:
    void operator() (const archived_event& event) override {
        DB("%s:\n", __func__);
        DB("%s: %d\n", __func__, event.key);
        dump_event(event.key,
                   event.forked,
                   event.press,
                   event.time,
                   xkb, xkbi, previous_time);
        previous_time = event.time;
        DB("%s: done\n", __func__);
    };

    explicit xorg_event_dumper(const PluginInstance* plugin):
        keybd(plugin->device),
        xkbi(keybd->key->xkbInfo),
        xkb(xkbi->desc),
        previous_time(0) {};
};
