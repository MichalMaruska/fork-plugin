#pragma once
#include "platform.h"
#include "circular.h"

typedef long time_type;

typedef struct _archived_event
{
    time_type time;
    USHORT key;
    USHORT forked;
    bool press;                  /* client type? */
} extendedEvent;


void win_event_to_extended(const KEYBOARD_INPUT_DATA& event, extendedEvent &ev, const time_type time)
{
  ev.time = time;
  ev.key = event.MakeCode;
  ev.forked = 0;
  ev.press = (event.Flags == 0);
}

// KEYBOARD_INPUT_DATA, will be converted to extendedEvent immediately.
// machine not aware of that.

class winEnvironment : public forkNS::platformEnvironment<
  USHORT, time_type, extendedEvent, extendedEvent > {

private:

public:
  winEnvironment() = default;
  ~winEnvironment() = default;

  bool press_p(const extendedEvent& event) const override {
    return event.press;
  }

  bool release_p(const extendedEvent& event) const override {
    return !event.press;
  }

  // I need to store this:
  time_type time_of(const extendedEvent& event) const override{
    return event.time;
  }

  USHORT detail_of(const extendedEvent& event) const override{
    return event.key;
  }

  bool ignore_event(const extendedEvent& pevent) override{
    UNREFERENCED_PARAMETER(pevent);
    return false;
  }

  bool output_frozen() override {return false;};

  void relay_event(const extendedEvent &pevent) const override {
    UNREFERENCED_PARAMETER(pevent);
    // todo:
  }

  void push_time(time_type now) {
    // nothing
    UNREFERENCED_PARAMETER(now);
  }

#ifndef KERNEL
    virtual void log(const char* format...) const = 0;
    virtual void vlog(const char* format, va_list argptr) const = 0;
#endif

  void fmt_event(const char* message, const extendedEvent& pevent) const override {
    UNREFERENCED_PARAMETER(message);
    UNREFERENCED_PARAMETER(pevent);
    //
  }


  void archive_event(extendedEvent& ee, const extendedEvent& event) override {
    UNREFERENCED_PARAMETER(ee);
    UNREFERENCED_PARAMETER(event);
  }

  void free_event(extendedEvent* pevent) const override {
    UNREFERENCED_PARAMETER(pevent);
  };


  void rewrite_event(extendedEvent& pevent, USHORT code) override {
    pevent.key = code;
  }
};

