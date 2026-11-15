/*
    Copyright (C) 1995-2006, The AROS Development Team. All rights reserved.

    NoCapsLock commodity -- Renders the CAPS LOCK key ineffective
*/

/******************************************************************************

    NAME

        NoCapsLock

    SYNOPSIS

        Tool types: CX_PRIORITY=N

    LOCATION

        SYS:Tools/Commodity

    FUNCTION

        Renders the CAPS LOCK key ineffective.

    INPUTS

        CX_PRIORITY  --  The priority of the commodity

    RESULT

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

    HISTORY

    05.03.2000  SDuvan   implemented

******************************************************************************/

#include <exec/exec.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <workbench/startup.h>
#include <libraries/commodities.h>
#include <proto/dos.h>
#include <proto/icon.h>
#include <proto/commodities.h>
#include <proto/alib.h>

struct DosLibrary *DOSBase;
struct Library    *IconBase;
struct Library    *CxBase;

static struct NewBroker nb =
{
    NB_VERSION,
    "NoCapsLock",
    "No Caps Lock",
    "Makes the Caps Lock key ineffective",
    NBU_NOTIFY | NBU_UNIQUE,
    0,
    0,
    NULL,
    0
};

typedef struct _APState
{
    CxObj          *as_broker;
    struct MsgPort *as_msgPort;
} APState;

/***************************************************************************/

static void killCapsLockCustomFunc(CxMsg *msg, CxObj *co);

/***************************************************************************/

static BOOL initiate(struct WBStartup *wbs, APState *as)
{
    CxObj *customObj, *filterObj;

    as->as_broker = NULL;

    if (wbs != NULL)
    {
        UBYTE **array = ArgArrayInit(0, (UBYTE **)wbs);

        nb.nb_Pri = ArgInt(array, "CX_PRIORITY", 0);
        ArgArrayDone();
    }

    as->as_msgPort = CreateMsgPort();

    if (as->as_msgPort == NULL)
    {
        return FALSE;
    }

    nb.nb_Port = as->as_msgPort;

    as->as_broker = CxBroker(&nb, 0);

    if (as->as_broker == NULL)
    {
        return FALSE;
    }

    filterObj = CxFilter(NULL);

    if (filterObj == NULL)
    {
        return FALSE;
    }
    else
    {
        static IX ix =
        {
            IX_VERSION,
            IECLASS_RAWKEY,
            0,
            0,
            IEQUALIFIER_CAPSLOCK,
            IEQUALIFIER_CAPSLOCK,
            0
        };

        SetFilterIX(filterObj, &ix);
        AttachCxObj(as->as_broker, filterObj);
    }

    customObj = CxCustom(killCapsLockCustomFunc, 0);

    if (customObj == NULL)
    {
        return FALSE;
    }

    AttachCxObj(filterObj, customObj);
    ActivateCxObj(as->as_broker, TRUE);

    return TRUE;
}

/***************************************************************************/

static void freeResources(APState *as)
{
    struct Message  *cxm;

    if (as->as_broker != NULL)
    {
        DeleteCxObjAll(as->as_broker);
    }

    if (as->as_msgPort != NULL)
    {
        while ((cxm = GetMsg(as->as_msgPort)))
        {
            ReplyMsg(cxm);
        }

        DeleteMsgPort(as->as_msgPort);
    }
}

/***************************************************************************/

/* Our CxCustom() function that is invoked everytime an imputevent is
   routed to our broker */
static void killCapsLockCustomFunc(CxMsg *msg, CxObj *co)
{
    struct InputEvent *ie = (struct InputEvent *)CxMsgData(msg);

    ie->ie_Qualifier &= ~IEQUALIFIER_CAPSLOCK;
}

/***************************************************************************/

/* React on command messages sent by commodities.library */
static void handleCx(APState *as)
{
    CxMsg *msg;
    BOOL   quit = FALSE;
    LONG   signals;

    while (!quit)
    {
        signals = Wait((1L << nb.nb_Port->mp_SigBit) | SIGBREAKF_CTRL_C);

        if (signals & (1L << nb.nb_Port->mp_SigBit))
        {
            while ((msg = (CxMsg *)GetMsg(as->as_msgPort)))
            {
                switch (CxMsgType(msg))
                {
                    case CXM_COMMAND:
                        switch (CxMsgID(msg))
                        {
                            case CXCMD_DISABLE:
                                ActivateCxObj(as->as_broker, FALSE);
                                break;

                            case CXCMD_ENABLE:
                                ActivateCxObj(as->as_broker, TRUE);
                                break;

                            case CXCMD_UNIQUE:
                                /* Running the program twice <=> quit */
                                /* Fall through */

                            case CXCMD_KILL:
                                quit = TRUE;
                                break;

                        } /* switch(CxMsgID(msg)) */

                        break;
                } /* switch (CxMsgType(msg))*/

                ReplyMsg((struct Message *)msg);

            } /* while((msg = (CxMsg *)GetMsg(cs->cs_msgPort))) */
        }

        if (signals & SIGBREAKF_CTRL_C)
        {
            quit = TRUE;
        }
    } /* while (!quit) */
}

/***************************************************************************/

int main(void)
{
    struct Process *me = (struct Process *)FindTask(NULL);
    struct WBStartup *wbs = NULL;
    APState aState;
    int     error = RETURN_FAIL;

    if (me->pr_CLI == NULL)
    {
        WaitPort(&me->pr_MsgPort);
        wbs = (struct WBStartup *)GetMsg(&me->pr_MsgPort);
    }

    DOSBase = (struct DosLibrary *)OpenLibrary(DOSNAME, 36L);
    IconBase = OpenLibrary(ICONNAME, 0);
    CxBase = OpenLibrary("commodities.library", 0);

    aState.as_msgPort = NULL;
    if (DOSBase && IconBase && CxBase && initiate(wbs, &aState))
    {
        handleCx(&aState);
        error = RETURN_OK;
    }

    if (CxBase)
        freeResources(&aState);

    CloseLibrary(CxBase);
    CloseLibrary(IconBase);
    CloseLibrary((struct Library *)DOSBase);

    if (wbs != NULL)
    {
        Forbid();
        ReplyMsg((struct Message *)wbs);
    }

    return error;
}
