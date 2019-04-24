// todo

extern "C"
{
#include <X11/Xdefs.h>          /* for Bool */
// #include <X11/Xproto.h>   // KeySym #define KeySym CARD32
#include <X11/X.h>
#include <X11/Xfuncproto.h>  // _XFUNCPROTOBEGIN

// fixme: this *requires* Bool!

#include <stdio.h> // FILE
#include <xorg/xkbsrv.h>




// #include <xorg/inputstr.h>

#include <X11/keysym.h>

#include "event_ops.h"
}
#include "debug.h"

const int BufferLength = 200;
// #define BufferLength 200

static void
retrieve_keysym(DeviceIntPtr keybd, KeyCode key, KeySym *sym, const char **keycode_name)
{
    if (!keybd->key)
        return;

    XkbSrvInfoPtr xkbi= keybd->key->xkbInfo;

    *keycode_name = xkbi->desc->names->keys[key].name;
    *sym= *XkbKeySymsPtr(xkbi->desc,key);
}

/* The returned string is in static space. Don't free it! */
const char*
describe_key(DeviceIntPtr keybd, InternalEvent *event)
{
    assert(event);
    static char buffer[BufferLength];
    KeyCode key = detail_of(event);

    const char *keycode_name = "";
    KeySym sym = XK_space;

    retrieve_keysym(keybd, key, &sym, &keycode_name);
    // assert(0 <= key <= (max_key_code - min_key_code));

#if 0
    if ((!sym) || (! isalpha(*(unsigned char*)sym)))
        sym = (KeySym*) " ";
#endif

    snprintf(buffer, BufferLength, "(%s) %d %4.4s -> %c %s (%u)",
             ((long)keybd->name<100)?"":keybd->name, key, keycode_name,
             (char)sym,
             event_type_brief(event),(unsigned int)time_of(event));

    return buffer;
}
