/*
    Copyright (C) 1995-2014, The AROS Development Team. All rights reserved.

    Desc: Internal definitions for the input.device
*/

#ifndef INPUT_INTERN_H
#define INPUT_INTERN_H

#ifndef AROS_LIBCALL_H
#   include <aros/libcall.h>
#endif
#ifndef EXEC_EXECBASE_H
#   include <exec/execbase.h>
#endif
#ifndef EXEC_DEVICES_H
#   include <exec/devices.h>
#endif
#ifndef EXEC_SEMAPHORES_H
#   include <exec/semaphores.h>
#endif
#include <devices/input.h>
#ifndef DEVICES_INPUTEVENT_H
#   include <devices/inputevent.h>
#endif
#ifndef DEVICES_TIMER_H
#   include <devices/timer.h>
#endif
#ifndef PROTO_EXEC_H
#   include <proto/exec.h>
#endif
#ifndef PROTO_INPUT_H
#   include <proto/input.h>
#endif

#define INPUTNAME "input.device"

/* Size of the input device's stack. Input handlers run on it, and all
 * task stacks share one small pool, so handlers must be frugal:
 * intuition's handler opens windows and draws on it. */
#define IDTASK_STACKSIZE    	    3840

/* Priority of the input.device task */
#define IDTASK_PRIORITY     	    20

/* Default key repeat threshold/interval in 1/50 secs */

#define DEFAULT_KEY_REPEAT_THRESHOLD 25
#define DEFAULT_KEY_REPEAT_INTERVAL  2

struct inputbase
{
    struct InputDevice pub;

    /* The stuff below will never get deallocated, since
     ** input device is never removed, once it's initialized.
     */
    struct Task *InputTask;
    struct MsgPort CommandPort;
    struct MinList HandlerList;
    struct InputEvent *EventQueueHead;
    struct InputEvent *EventQueueTail;
    struct timeval KeyRepeatThreshold;
    struct timeval KeyRepeatInterval;
    UWORD ActQualifier;
    UBYTE Prev1DownCode;
    UBYTE Prev1DownQual;
    UBYTE Prev2DownCode;
    UBYTE Prev2DownQual;
    BOOL Stopped;
    APTR SegList;
};

/* Prototypes */
VOID ProcessEvents(VOID);
VOID ForwardQueuedEvents(struct inputbase *InputDevice);
VOID AddEQTail(struct InputEvent *ie, struct inputbase *InputDevice);
struct InputEvent *GetEventsFromQueue(struct inputbase *InputDevice);
BOOL IsQualifierKey(UWORD key);
BOOL IsRepeatableKey(UWORD key);

#endif /* INPUT_INTERN_H */
