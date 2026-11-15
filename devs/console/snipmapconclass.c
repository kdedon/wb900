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

extern const STRPTR CONCLIP_PORTNAME;

struct snipmapcondata
{
    ULONG start_selection_x;
    ULONG start_selection_y;
};

#undef ConsoleDevice
#define ConsoleDevice ((struct ConsoleBase *)cl->cl_UserData)

/***********  SnipMapCon::New()  **********************/

static Object *snipmapcon_new(Class *cl, Object *o, struct opSet *msg)
{
    o = (Object *) DoSuperMethodA(cl, o, (Msg) msg);
    if (o)
    {
        struct snipmapdata *data = INST_DATA(cl, o);
        SetMem(data, 0, sizeof(struct snipmapcondata));
    }
    return o;
}

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
    case OM_NEW:
        retval = (IPTR) snipmapcon_new(cl, o, (struct opSet *)msg);
        break;

    /* OM_DISPOSE, M_Console_DoCommand and M_Console_Copy need nothing done
       here (the selection code still lives in charmapcon) and go to the
       superclass with all other methods */

    case M_Console_Paste:
        snipmapcon_paste(cl, o, msg);
        break;

#if 0
    case M_Console_HandleGadgets:
        snipmapcon_handlegadgets(cl, o, (struct P_Console_HandleGadgets *)msg);
        break;
#endif

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

    cl = MakeClass(NULL, NULL, CHARMAPCLASSPTR,
        sizeof(struct snipmapcondata), 0UL);
    if (cl)
    {
        cl->cl_Dispatcher.h_Entry = (APTR) dispatch_snipmapconclass;
        cl->cl_Dispatcher.h_SubEntry = NULL;

        cl->cl_UserData = (IPTR) ConsoleDevice;

        return (cl);
    }
    return NULL;
}
