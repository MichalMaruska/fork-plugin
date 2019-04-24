// todo

extern "C"
{
#include "event_ops.h"
}
#include "debug.h"

const int BufferLength = 200;
// #define BufferLength 200

/* The returned string is in static space. Don't free it! */
const char*
describe_key(DeviceIntPtr keybd, InternalEvent *event)
{
    assert (event);

    static char buffer[BufferLength];
    XkbSrvInfoPtr xkbi= keybd->key->xkbInfo;
    KeyCode key = detail_of(event);
    // assert(0 <= key <= (max_key_code - min_key_code));
    char* keycode_name = xkbi->desc->names->keys[key].name;

    const KeySym *sym= XkbKeySymsPtr(xkbi->desc,key);
    if ((!sym) || (! isalpha(*(unsigned char*)sym)))
        sym = (KeySym*) " ";

    snprintf(buffer, BufferLength, "(%s) %d %4.4s -> %c %s (%u)",
             keybd->name,
             key, keycode_name,(char)*sym,
             event_type_brief(event),(unsigned int)time_of(event));

    return buffer;
}
