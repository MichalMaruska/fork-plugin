#pragma once

#include "machine.h"
#include "xorg.h"
// KeyCode and Time:
extern "C"
{
    // We are inside server:
#include <xorg-server.h>

// todo: archived_event coud be avoided ... it's X specific
// plugin should know how to translate/send them away.l
#include "fork_requests.h"
#undef max
#undef min

    // the types:
#include <X11/X.h>
#undef max
#undef min
}
#include "empty_last.h"

using last_events_t = empty_last_events_t<archived_event>;

extern template class forkNS::forkingMachine<KeyCode, Time, XorgEvent, XOrgEnvironment, archived_event, last_events_t>;

using machineRec = forkNS::forkingMachine<KeyCode, Time,
                                          XorgEvent, XOrgEnvironment,
                                          archived_event, last_events_t>;

