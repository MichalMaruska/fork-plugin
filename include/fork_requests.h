#ifndef FORK_REQUESTS_H
#define FORK_REQUESTS_H

extern "C" {
#include <X11/Xmd.h>

#include <xorg-server.h>

#include <X11/X.h>
#include <X11/Xproto.h>
#include <X11/keysym.h>
#include <xorg/inputstr.h>
}

#define FORK_PLUGIN_NAME "fork"

/** I think, that this is, at the beginning, a list of requests, which each plugin should handle.
 * Then, there should be interpretation of the config sub-OPs.
 */

/** Requests:  **/
enum {
      // can C ?
      //      X_kbSetFork = 28,
      //      X_kbSetForkRepeat,
      X_kbGetLastKeys = 30,

      X_kbConfigure,
      X_kbConfigureKey,
      X_kbGetConfigure,
      X_kbGetConfigureKey,
};


/**  Configuring:  sending a pair of numbers + OP-code   **/

/* fixme: not keys, but events ! */
typedef struct _forkConfigure {
   CARD8                reqType;
   CARD8                forkReqType;
   CARD16               length B16;
   CARD16               deviceSpec B16;
   CARD16               pad1;

  /* see /x/cvs/xfree/xpatches/medved-plugin/include/extensions/XKBproto.h  ->        _forkPluginConfig  */

   CARD16               what;
   CARD16               pad2;
   CARD32               value;
} forkConfigureReq;

#define sz_forkConfigureReq     16


/* fixme: not keys, but events ! */
/* twin ... the other code, forked(code) */
typedef struct _forkConfigureKey {
   CARD8                reqType;
   CARD8                forkReqType;
   CARD16               length B16;
   CARD16               deviceSpec B16;

   CARD16               what;
   CARD16               code;
   CARD16               twin;
   CARD32               value;
} forkConfigureKeyReq;
#define sz_forkConfigureKeyReq  16

typedef struct _forkGetConfigure {
   CARD8                reqType;
   CARD8                forkReqType;
   CARD16               length B16;
   CARD16               deviceSpec B16;

   CARD16               pad1;
   CARD16               what;
   CARD16               pad2;
} forkGetConfigureReq;
#define sz_forkGetConfigureReq  12

typedef struct _forkGetConfigureKey {
   CARD8                reqType;
   CARD8                forkReqType;
   CARD16               length B16;
   CARD16               deviceSpec B16;

   CARD16               code;
   CARD16               what;
   CARD16               twin;
} forkGetConfigureKeyReq;
#define sz_forkGetConfigureKeyReq  12


/** Sub-OP-codes for the Set/Get-Configure-key request:  **/
enum  {
       fork_configure_last_events,
       fork_configure_overlap_limit,
       fork_configure_total_limit,
       fork_configure_repeat_limit,

       fork_configure_repeat_consider_forks,
       /* only per key: */
       fork_configure_key_fork,
       fork_configure_key_fork_repeat, // 7

       fork_configure_debug,
       fork_configure_switch,
       fork_configure_clone,
       fork_configure_clear_interval,

       fork_server_dump_keys, // 11
       fork_client_dump_keys,
};

/*  Dumping Events */
typedef struct _fork_event_notify {
    BYTE        type;
    BYTE        forkType;
    CARD16      sequenceNumber B16;
    Time        time B32;
    CARD8       deviceID;

    CARD8       mods;
    CARD8       baseMods;
    CARD8       latchedMods;
    CARD8       lockedMods;
    CARD8       group;
    INT16       baseGroup B16;

    INT16       latchedGroup B16;
    CARD8       lockedGroup;
    CARD8       compatState;

    CARD8       grabMods;
    CARD8       compatGrabMods;
    CARD8       lookupMods;
    CARD8       compatLookupMods;

    CARD16      ptrBtnState B16;
    CARD16      changed B16;

    KeyCode     keycode;
    CARD8       eventType;
    CARD8       requestMajor;
    CARD8       requestMinor;
} fork_event_notify;
#define sz_fork_event_notify    32


/**  Grabbed keys should not be pushed?   I think I'll make it confgurable in XF86Config  **/

/**  Requesting & providing  last keys typed **/
typedef struct _xforkGetLastKeys {
    CARD8                reqType;
    CARD8                forkReqType;    /* always X_KBSetFork */
    CARD16               length B16;
    CARD16               deviceSpec B16;
    CARD16               count;  /* how many ? */
} xforkGetLastKeysReq;
#define sz_xforkGetLastKeysReq  8

typedef struct
{
    Time time;
    KeyCode key;
    KeyCode forked;
    Bool press;                  /* client type? */
} archived_event;
/* 10 bytes? i guess 12! */

typedef struct
{
    int count;
    archived_event e[];
} fork_events_reply;

#endif
