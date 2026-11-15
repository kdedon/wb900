/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: console.device function CDInputHandler()
*/

#include <proto/exec.h>
#include <exec/libraries.h>
#include <exec/memory.h>
#include <exec/interrupts.h>
#include <proto/console.h>
#include <proto/intuition.h>
#include <intuition/intuitionbase.h>

#include <devices/inputevent.h>

#include "console_gcc.h"

/* protos */
static Object *obtainconunit(struct Window *window,
    struct ConsoleBase *ConsoleDevice);
static VOID releaseconunit(struct ConsoleBase *ConsoleDevice);

/*************************************************************************

    NAME */
        AROS_LH2I(struct InputEvent *, CDInputHandler,

/*  SYNOPSIS */
        AROS_LHA(struct InputEvent *, events, A0),
        AROS_LHA(APTR, consoleDevice, A1),

/*  LOCATION */
        struct Library *, ConsoleDevice, 7, Console)

/*  FUNCTION

    INPUTS

    RESULT

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    return cdihandler(events, (struct ConsoleBase *)consoleDevice);

    AROS_LIBFUNC_EXIT
} /* CDInputHandler */

/* The input handler, called by input.device with its is_Data */
struct InputEvent *cdihandler(struct InputEvent *events,
    struct ConsoleBase *ConsoleDevice)
{
    struct InputEvent *ie;

    struct cdihData *cdihdata = (struct cdihData *)&ConsoleDevice->consIHData;

    for (ie = events; ie; ie = ie->ie_NextEvent)
    {
        /* A rawkey event ? */
        if ((ie->ie_Class == IECLASS_RAWKEY
                && !(ie->ie_Code & IECODE_UP_PREFIX))
            || (ie->ie_Class == IECLASS_SIZEWINDOW)
            || (ie->ie_Class == IECLASS_CLOSEWINDOW)
            || (ie->ie_Class == IECLASS_REFRESHWINDOW)
            || (ie->ie_Class == IECLASS_GADGETDOWN)
            || (ie->ie_Class == IECLASS_GADGETUP)
            || (ie->ie_Class == IECLASS_MENULIST)
            || (ie->ie_Class == IECLASS_RAWMOUSE)
            || (ie->ie_Class == IECLASS_ACTIVEWINDOW)
            || (ie->ie_Class == IECLASS_INACTIVEWINDOW)
            || (ie->ie_Class == IECLASS_TIMER))
        {
            /* What console do we send it to ? */
            Object *unit;

            /* find and prevent deletion of unit */
            unit = obtainconunit(
                (ie->ie_Class == IECLASS_ACTIVEWINDOW ||
                 ie->ie_Class == IECLASS_INACTIVEWINDOW) ?
                    (struct Window *)ie->ie_EventAddress : NULL,
                ConsoleDevice);
            if (unit)
            {
                /* Hand the event to the console task asynchronously: one
                   message per event, freed by the task after processing.
                   NEVER wait for the console task here. This handler runs
                   in the input.device task, and the console task renders
                   (RectFill obtains the window's layer lock). During an
                   interactive size/drag intuition holds LockLayers() across
                   many input events, so waiting for the console task while
                   it waits for the layer lock deadlocks all input. Under
                   memory pressure the event is dropped instead. */
                struct cdihMessage *message =
                    AllocMem(sizeof(struct cdihMessage),
                    MEMF_PUBLIC | MEMF_CLEAR);

                if (message)
                {
                    message->msg.mn_Length = sizeof(struct cdihMessage);
                    message->unit = unit;
                    message->ie = *ie;
                    PutMsg(cdihdata->inputPort, (struct Message *)message);
                }

                /* deletion of unit is now allowed */
                releaseconunit(ConsoleDevice);

            } /* if (RAWKEY event was meant for a console window) */
        }
    } /* for (each event in the chain) */

    return events;
}

/***********************
**  Support funtions  **
***********************/

/* Obtains a conunit object, and locks it, so that it's
   not deleted while we work on it
*/

#undef IntuitionBase

static Object *obtainconunit(struct Window *window,
    struct ConsoleBase *ConsoleDevice)
{
    struct IntuitionBase *IntuitionBase =
        (APTR) ConsoleDevice->cb_IntuitionBase;
    struct Window *activewin;
    Object *o, *ostate;
    ULONG lock;

    /* Lock the console list */
    ObtainSemaphoreShared(&ConsoleDevice->unitListLock);

    /* Activation events name the affected window explicitly.  This matters
     * for INACTIVEWINDOW, because ActiveWindow already points at the newly
     * activated window by the time the event is delivered. */
    if (window)
        activewin = window;
    else
    {
        lock = LockIBase(0UL);
        activewin = IntuitionBase->ActiveWindow;
        UnlockIBase(lock);
    }

    /* Try to find the correct unit object for taht window */
    ostate = (Object *) ConsoleDevice->unitList.mlh_Head;

    while ((o = NextObject(&ostate)))
    {
        /* Is this console the currently active window ? */
        if (CU(o)->cu_Window == activewin)
        {
            /* Keep the shared unit-list lock until the caller is done
               using this console object. */
            break;
        }
    }

    /*
     * A found unit remains protected by the shared list lock until
     * releaseconunit().  CloseDevice() takes this lock exclusively before
     * removing a unit, so it cannot dispose the object while this caller
     * still uses it.
     */
    if (!o)
        ReleaseSemaphore(&ConsoleDevice->unitListLock);

    return o;
}

static VOID releaseconunit(struct ConsoleBase *ConsoleDevice)
{
    ReleaseSemaphore(&ConsoleDevice->unitListLock);
}

/****************
** initCDIH()  **
****************/
/* This function should be executed on the console.device task's context only,
   so that the inputport is set correctly
*/

struct Interrupt *initCDIH(struct ConsoleBase *ConsoleDevice)
{
    struct Interrupt *cdihandler_is;
    struct cdihData *cdihdata;

    cdihandler_is =
        AllocMem(sizeof(struct Interrupt), MEMF_PUBLIC | MEMF_CLEAR);
    if (cdihandler_is)
    {
        cdihdata = &ConsoleDevice->consIHData;
        cdihdata->inputPort = CreateMsgPort();
        if (cdihdata->inputPort)
        {
            /* Initialize Interrupt struct */
            cdihandler_is->is_Code = (int (*)())cdihandler;
            cdihandler_is->is_Data = ConsoleDevice;
            /* Intuition runs at priority 50 and leaves the original event
             * available to lower-priority handlers.  Run afterwards so its
             * ActiveWindow is current before obtainconunit() routes the event,
             * matching the classic console.device input chain. */
            cdihandler_is->is_Node.ln_Pri = 0;
            cdihandler_is->is_Node.ln_Name = "console.device InputHandler";

            return cdihandler_is;
        }
        FreeMem(cdihandler_is, sizeof(struct Interrupt));
    }
    return NULL;
}
