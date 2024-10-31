#pragma once

#include <cstdint>
#include <libinput.h>

// #include "fork_requests.h"
#include "platform.h"
#include "colors.h"
#include <memory>
#include <string>

// struct libinput_event_keyboard;

// typedef archived_event
struct archived_event
{
    uint64_t time;
    int key;
    int forked;
    bool press;                  /* client type? */
};


class libinputEvent : public PlatformEvent {
public:
  // private:
  const libinput_event_keyboard* event;
  const libinput_device *device;

public:
  libinputEvent(const libinput_event_keyboard *event, const libinput_device *device) : event(event), device(device) {};
  ~libinputEvent() {} // free(event);
  // we own it
};

#if 0
class xorg_event_publisher : public event_publisher<archived_event>
{
    private:
    char* memory;
    const ClientPtr client;
    /* const */ PluginInstance* plugin;
    std::size_t appendix_len;

    public:
    xorg_event_publisher(ClientPtr client, PluginInstance* plugin) : client(client), plugin(plugin) {};

    virtual ~xorg_event_publisher() {
        free(memory);
        memory = nullptr;
    }

    virtual void prepare(int max_events) override{
        // memory =
        appendix_len = sizeof(fork_events_reply) + (max_events * sizeof(archived_event));
        memory = (char*) malloc(appendix_len);
        // if this fails?
    }

    virtual int commit() override{
        xkb_plugin_send_reply(client, plugin, memory, appendix_len);
          /* What XReply to send?? */

        // can do now:
        free(memory);
        memory = nullptr;
        return 0;
    }
    virtual void event(const archived_event& event) override {
        // memcpy into the buffer:
        // typecast
        // const archived_event&
    }
};
#endif

#if 1
// Closure
class libinput_event_dumper : public event_dumper<archived_event>
{
private:
  // services
  uint64_t previous_time;

public:
    void operator() (const archived_event& event) override {
#if 0
        dump_event(event.key,
                   event.forked,
                   event.press,
                   event.time,
                   xkb, xkbi, previous_time);
        previous_time = event.time;
#endif
    };

    virtual ~libinput_event_dumper() {};


    explicit libinput_event_dumper() :
        previous_time(0) {
#if DEBUG > 1
      // ErrorF("%s: creating dumper for %s\n", __func__, keybd->name);
#endif
    };
};
#endif

// using Keycode int;

class libinputEnvironment : public platformEnvironment<int, uint64_t, archived_event> {
private:
  libinput_fork_services *services;

public:
  libinputEnvironment(libinput_fork_services* services) : services(services) {};

  virtual ~libinputEnvironment() = default;


  virtual bool output_frozen() override {
    return false;
  };

  virtual bool ignore_event(const PlatformEvent *pevent) override {
    return false;
  }

  // libinput_event_get_keyboard_event(
#define  GET_EVENT(pevent)                                              \
  (const_cast<libinput_event_keyboard*>(static_cast<const libinputEvent*>(pevent)->event))

#define GET_DEVICE(pevent) \
  (const_cast<libinput_device*>(static_cast<const libinputEvent*>(pevent)->device))


  virtual int detail_of(const PlatformEvent* pevent) override {
    // struct libinput_event_keyboard *
    auto* event = GET_EVENT(pevent);
    return libinput_event_keyboard_get_key(event);
  };

  virtual void rewrite_event(PlatformEvent* pevent, int code) override {
    auto event = static_cast<libinputEvent*>(pevent)->event;
    // fixme:
#if 0
    libinput_event_keyboard_set_key(event, code);
#endif
  }



  virtual void free_event(PlatformEvent* pevent) const override {
    if (pevent == nullptr) {
      // ErrorF("BUG %s: %p\n", __func__, pevent);
      return;
    }

    auto* event = GET_EVENT(pevent);
    free(event);
  }

  virtual bool press_p(const PlatformEvent* pevent) override {
    auto* event = GET_EVENT(pevent);

    return (libinput_event_keyboard_get_key_state(event) == LIBINPUT_KEY_STATE_PRESSED);
  }
  virtual bool release_p(const PlatformEvent* pevent) override {
    auto* event = GET_EVENT(pevent);

    return (libinput_event_keyboard_get_key_state(event) == LIBINPUT_KEY_STATE_RELEASED);
  }

  virtual uint64_t time_of(const PlatformEvent* pevent) override {
    auto* event = GET_EVENT(pevent);
    return libinput_event_keyboard_get_time_usec(event);
  }

  // so this is orthogonal? platform-independent?
  void archive_event(archived_event& archived_event, const PlatformEvent *pevent) override {

#if DEBUG > 1
    auto xevent = static_cast<libinputEvent*>(pevent)->event;
    // dynamic_cast

    log("%s:%d type: %d\n", __func__, __LINE__, xevent->any.type);
    log("%s:%d keycode: %d\n", __func__, __LINE__, detail_of(pevent));
    log("%s:%d keycode via function: %d\n", __func__, __LINE__, detail_of(pevent));
#endif
    archived_event.key = detail_of(pevent);
    archived_event.time = time_of(pevent);
    archived_event.press = press_p(pevent);
  };



  virtual void relay_event(PlatformEvent* &pevent) override {
    auto* event = GET_EVENT(pevent);
    services->post_event(services, GET_DEVICE(pevent), event);
    // we didn't allocate?
    // pevent = nullptr;
  };

  virtual void push_time(uint64_t now) override {
    // services->push_time(now)
  }


  virtual void log(const char* format ...) const override {
    va_list args;
    va_start(args, format);
    services->log(services, LIBINPUT_LOG_PRIORITY_INFO, format, args);
    va_end(args);
  }

  virtual void vlog(const char* format, va_list args) const override {
    services->log(services, LIBINPUT_LOG_PRIORITY_INFO, format, args);
  }


  // the idea was to return a string. but who will deallocate it?
  virtual std::string fmt_event(const PlatformEvent *pevent) override {
    // return std::string("");
    return "";
  };

#if 1
  virtual
  std::unique_ptr<event_dumper<archived_event>> get_event_dumper() override {
    // return nullptr;
    return std::make_unique<libinput_event_dumper>(); // services
  }
#endif

};

