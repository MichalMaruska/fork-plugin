#pragma once
#include "platform.h"
#include "circular.h"
#include <stdarg.h>

typedef long time_type;

typedef struct _archived_event
{
    time_type time;
    USHORT UnitId;
    USHORT key;
    USHORT flags;
    USHORT Reserved; // for OS
    ULONG  ExtraInformation;
    USHORT forked;
    bool press;                  /* client type? */

    void* operator new(size_t size, void* p) noexcept
    {
      UNREFERENCED_PARAMETER(size);
      return p;
    };
} extendedEvent;

extern void pass_event(const extendedEvent& pevent, WDFDEVICE hDevice);

void win_event_to_extended(const KEYBOARD_INPUT_DATA& event, extendedEvent &ev, const time_type time)
{
  ev.time = time;
  ev.key = event.MakeCode;
  ev.forked = 0;
  ev.flags = event.Flags;
  ev.press = !(event.Flags & KEY_BREAK);
  //
  ev.UnitId = event.UnitId;
  ev.Reserved = event.Reserved;
  ev.ExtraInformation = event.ExtraInformation;
}

void extended_event_to_win(const extendedEvent &pevent, KEYBOARD_INPUT_DATA& event)
{
  event.MakeCode = pevent.key;
  event.Flags = pevent.flags;

  event.UnitId = pevent.UnitId;
  event.Reserved = pevent.Reserved;
  event.ExtraInformation = pevent.ExtraInformation;
  // ev.time = time;
  // ev.forked = 0;
  // ev.press = !(event.Flags & 1);
}


// KEYBOARD_INPUT_DATA, will be converted to extendedEvent immediately.
// machine not aware of that.


#if false
#define OVERRIDE override
#else
#define OVERRIDE
#endif

class winEnvironment
#if false
 : public forkNS::platformEnvironment<USHORT, time_type, extendedEvent, extendedEvent >
#endif
{

public:
    WDFDEVICE   hDevice;
public:
  winEnvironment() = default;
  // no:
  ~winEnvironment() {}

  void* operator new(size_t size, void* p) noexcept
  {
    UNREFERENCED_PARAMETER(size);
    return p;
  }

  void operator delete(void* p) noexcept
  {
    ExFreePool(p);
  }


  bool press_p(const extendedEvent& event) const OVERRIDE {
    return event.press;
  }

  bool release_p(const extendedEvent& event) const OVERRIDE {
    return !event.press;
  }

  // I need to store this:
  time_type time_of(const extendedEvent& event) const OVERRIDE{
    return event.time;
  }

  USHORT detail_of(const extendedEvent& event) const OVERRIDE{
    // note: we ignore the E0/E1 bits!
    // I might even fork to an E0 key!
    return event.key | ((event.flags & KEY_E0)?(1<<8):0) | ((event.flags & KEY_E1)?(1<<9):0);
  }

  bool ignore_event(const extendedEvent& pevent) OVERRIDE{
    UNREFERENCED_PARAMETER(pevent);
    return false;
  }

  bool output_frozen() OVERRIDE {return false;};

  void relay_event(const extendedEvent &pevent) const OVERRIDE {
    fmt_event(__func__, pevent);
    pass_event(pevent, hDevice);
  }

  void push_time(time_type now) {
    // nothing
    UNREFERENCED_PARAMETER(now);
  }

  virtual void log(const char* format...) const {
    // output to debugger if it's attached?
    va_list args;
    va_start(args, format);

    ULONG ComponentId = DPFLTR_KBDCLASS_ID; // (long) 'FORK';
    ULONG   Level = DPFLTR_TRACE_LEVEL;
    vDbgPrintEx(ComponentId, Level, format,args);
    va_end(args);
  };

  virtual void vlog(const char* format, va_list argptr) const {
    //NTSYSAPI ULONG
    ULONG ComponentId = DPFLTR_KBDCLASS_ID; // (long) 'FORK';
    ULONG   Level = DPFLTR_TRACE_LEVEL;
    vDbgPrintEx(ComponentId, Level, format,argptr);
  };

  void fmt_event(const char* message, const extendedEvent& pevent) const OVERRIDE {
    log("%s (%s): %u %s\n", message, __func__, detail_of(pevent), press_p(pevent)?"press":"release");

  }

  void archive_event(extendedEvent& ee, const extendedEvent& event) OVERRIDE {
    UNREFERENCED_PARAMETER(ee);
    UNREFERENCED_PARAMETER(event);
  }

  void free_event(extendedEvent* pevent) const OVERRIDE {
    UNREFERENCED_PARAMETER(pevent);
  };

  void rewrite_event(extendedEvent& pevent, USHORT code) OVERRIDE {
    log("%s: to %u\n", __func__, code);
    pevent.key = code;
  }
};

