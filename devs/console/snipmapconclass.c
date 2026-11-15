/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: Code for CONU_SNIPMAP console units.
*/


#include <proto/utility.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <intuition/intuition.h>

#include <intuition/imageclass.h>
#include <intuition/gadgetclass.h>
#include <libraries/gadtools.h>

#include <graphics/rastport.h>
#include <aros/asmcall.h>


#include "console_gcc.h"
#include "consoleif.h"

#undef ConsoleDevice
#define ConsoleDevice ((struct ConsoleBase *)cl->cl_UserData)

static VOID snipmapcon_paste(Class *cl, Object *o, Msg msg)
{
    /* if conclip is running, insert <CSI> "0 v" and we're done.
     * The console.handler will be responsible for the actual paste.
     */
    if (!IsListEmpty(&ConsoleDevice->sniphooks)
        || FindPort(CONCLIP_PORTNAME))
    {
        con_inject(ConsoleDevice, CU(o), "\x9b" "0 v", -1);
        return;
    }
    ObtainSemaphore(&ConsoleDevice->copyBufferLock);
    con_inject(ConsoleDevice, CU(o), ConsoleDevice->copyBuffer,
        ConsoleDevice->copyBufferSize);
    ReleaseSemaphore(&ConsoleDevice->copyBufferLock);
}

AROS_UFH3S(IPTR, dispatch_snipmapconclass,
    AROS_UFHA(Class *, cl, A0),
    AROS_UFHA(Object *, o, A2), AROS_UFHA(Msg, msg, A1))
{
    AROS_USERFUNC_INIT

    IPTR retval = 0UL;

    switch (msg->MethodID)
    {
    /* OM_NEW, OM_DISPOSE, M_Console_DoCommand and M_Console_Copy need
       nothing done here (the selection code still lives in charmapcon) and
       go to the superclass with all other methods */

    case M_Console_Paste:
        snipmapcon_paste(cl, o, msg);
        break;

    default:
        retval = DoSuperMethodA(cl, o, msg);
        break;
    }

    return retval;

    AROS_USERFUNC_EXIT
}

#undef ConsoleDevice

Class *makeSnipMapConClass(struct ConsoleBase *ConsoleDevice)
{
    Class *cl;

    cl = MakeClass(NULL, NULL, CHARMAPCLASSPTR, 0, 0UL);
    if (cl)
    {
        cl->cl_Dispatcher.h_Entry = (ULONG (*)()) dispatch_snipmapconclass;
        cl->cl_Dispatcher.h_SubEntry = NULL;

        cl->cl_UserData = (IPTR) ConsoleDevice;

        return (cl);
    }
    return NULL;
}
