#pragma once
extern "C" {
#include <xorg-server.h>
#include <xorg/eventstr.h>

#undef max
#undef min
}

typedef struct
{
    Time time;
    KeyCode key;
    KeyCode forked;
    Bool press;                  /* client type? */
} archived_event;
/* 10 bytes? i guess 12! */
