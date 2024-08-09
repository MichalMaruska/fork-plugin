extern "C" {
// this one pulls in all the dependencies. When that one is split to *.cpp we will
// have to state our dependencies explicity.
#include "event_ops.h"

// KeySym #define KeySym CARD32 comes from:
#include <X11/X.h>
#include <xorg/xkbsrv.h>
// specific keysyms:
#include <X11/keysym.h>
}
#include "debug.h"

constexpr int BufferLength = 200;

static void
retrieve_keysym(DeviceIntPtr keybd, const KeyCode key, KeySym *sym, const char **keycode_name)
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
    KeyCode key = detail_of1(event);

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
