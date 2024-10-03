#pragma once

// KeyCode and Time:
extern "C"
{
    // We are inside server:
#include <xorg-server.h>

    // the types:
#include <X11/X.h>
#undef max
#undef min
}

using machineRec = forkingMachine<KeyCode, Time>;

// fixme:
#define plugin_machine(plugin) ((machineRec*)(plugin->data))
