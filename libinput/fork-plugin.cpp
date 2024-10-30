
#include <cstdint>
#include <libinput.h>
#include <memory>
#include "machine.h"
#include "machine.cpp"
#include "libinput_environment.h"

// it seems this will be part of libinput!
// but I want it c++

using machineRec = forkNS::forkingMachine<int, uint64_t, archived_event>;

namespace forkNS {
template uint64_t forkingMachine<int, uint64_t, archived_event>::accept_event(std::unique_ptr<PlatformEvent> pevent);
}


static std::unique_ptr<libinputEvent>
create_libinput_platform_event(const struct libinput_device *device, const struct libinput_event_keyboard *key_event)
{
  return std::make_unique<libinputEvent>(key_event, device);
}


static
void accept_event(void* user_data, const struct libinput_device *device, const struct libinput_event_keyboard *key_event)
{
  machineRec* forking_machine = static_cast<machineRec*>(user_data);

  forking_machine->mdb("%s, event %p (%d), device %p\n",
                       __func__, key_event,
                       libinput_event_get_type(libinput_event_keyboard_get_base_event(const_cast<libinput_event_keyboard*>(key_event))),
                       device);

  std::unique_ptr<PlatformEvent> ev = create_libinput_platform_event(device, key_event);

  uint64_t time = forking_machine->accept_event(std::move(ev));
#if 0
  if (time!=0)
    service->set_timer(time);
#endif
}


static void
accept_time(void* user_data, struct libinput_device *device, uint64_t time) {
  machineRec* forking_machine = static_cast<machineRec*>(user_data);

  uint64_t time = forking_machine->accept_time(time);
};

extern "C" {
void fork_init(struct libinput_fork_services* services)
{
  if (services == NULL)
    return;

  services->log(services, LIBINPUT_LOG_PRIORITY_ERROR, "hello from C++\n");

  auto libinputEnv = std::make_unique<libinputEnvironment>(services);

  // we hand it over to C
  machineRec *forking_machine = new machineRec(libinputEnv.release());
  forking_machine->create_configs();
  forking_machine->set_debug(1);
  forking_machine->configure_key(fork_configure_key_fork, 57, 42, 1);

  // todo:
  // * create timer
  struct libinput_keyboard_plugin* plugin = (struct libinput_keyboard_plugin*) malloc(sizeof *plugin);
  *plugin = (struct libinput_keyboard_plugin) {
    .user_data = forking_machine,
    .accept_event = &accept_event,
    .accept_time = &accept_time,
  };

  libinput_register_fork(plugin);
}

}
