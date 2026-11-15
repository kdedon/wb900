/*
    Copyright (C) 1995-2020, The AROS Development Team. All rights reserved.

    AutoPoint commodity -- activates the window under the mouse pointer.
*/

/******************************************************************************

    NAME

        AutoPoint

    SYNOPSIS

        Tool types: CX_PRIORITY=N, MIDMOUSEPREVENTSACTIVATION

    LOCATION

        SYS:Tools/Commodities

    FUNCTION

        Automatically activates the window under the mouse pointer, once
        the pointer has rested for a timer tick (0.1 s).

    INPUTS

        CX_PRIORITY  --  The priority of the commodity

        MIDMOUSEPREVENTSACTIVATION  --  Don't activate the window under the
                                        mouse if middle mouse button is pressed

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
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <libraries/commodities.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/layers.h>
#include <proto/commodities.h>
#include <proto/alib.h>
#include <proto/icon.h>

/***************************************************************************/

struct DosLibrary     *DOSBase;
struct IntuitionBase  *IntuitionBase;
struct Library        *LayersBase;
struct Library        *IconBase;
struct Library        *CxBase;

static struct NewBroker nb =
{
    NB_VERSION,
    "AutoPoint",
    "Auto Point",
    "Activates the window under the pointer",
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

typedef struct AP
{
    struct Window *ai_thisWindow;
    struct Window *ai_lastActivatedWindow;
    BOOL           ai_mouseHasMoved;
    BOOL           ai_leftButtonDown;
    BOOL           ai_rightButtonDown;
    BOOL           ai_middleButtonDown;
    BOOL           ai_midMousePreventsActivation;
} AP;

static AP apInfo;

/************************************************************************************/

static void autoActivateLag(CxMsg *msg, CxObj *co);

/************************************************************************************/

static BOOL initiate(struct WBStartup *wbs, APState *as)
{
    CxObj *customObj;

    as->as_broker = NULL;

    if (wbs != NULL)
    {
        UBYTE **array = ArgArrayInit(0, (UBYTE **)wbs);

        nb.nb_Pri = ArgInt(array, "CX_PRIORITY", 0);

        if (ArgString(array, "MIDMOUSEPREVENTSACTIVATION", 0))
        {
            apInfo.ai_midMousePreventsActivation = TRUE;
        }

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

    customObj = CxCustom(autoActivateLag, 0);

    if (customObj == NULL)
    {
        return FALSE;
    }

    AttachCxObj(as->as_broker, customObj);
    ActivateCxObj(as->as_broker, TRUE);

    apInfo.ai_thisWindow = IntuitionBase->ActiveWindow;

    return TRUE;
}

/************************************************************************************/

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

/************************************************************************************/

/* Our CxCustom() function that is invoked everytime an imputevent is
   routed to our broker.  A window is activated at a timer event that
   comes with no mouse movement since the previous one. */
static void autoActivateLag(CxMsg *msg, CxObj *co)
{
    struct InputEvent *ie = (struct InputEvent *)CxMsgData(msg);

    if (ie->ie_Class == IECLASS_TIMER)
    {
        struct Screen *screen;
        struct Layer  *layer;

        if (apInfo.ai_leftButtonDown  ||
            apInfo.ai_rightButtonDown ||
            apInfo.ai_middleButtonDown  )
            return;

        if (apInfo.ai_mouseHasMoved)
        {
            apInfo.ai_mouseHasMoved = FALSE;
            return;
        }

        if (IntuitionBase->ActiveWindow != NULL)
        {
            screen = IntuitionBase->ActiveWindow->WScreen;
        }
        else
        {
            screen = IntuitionBase->ActiveScreen;
        }

        if (!screen)
            layer = NULL;
        else
        {
            LockLayerInfo(&screen->LayerInfo);
            layer = WhichLayer(&screen->LayerInfo, screen->MouseX, screen->MouseY);
            UnlockLayerInfo(&screen->LayerInfo);
        }

        apInfo.ai_thisWindow = (layer != NULL) ?
            (struct Window *)layer->Window : NULL;

        /* Once per rest: the activation is done when intuition gets to it */
        if ((IntuitionBase->ActiveWindow == apInfo.ai_thisWindow) ||
                (apInfo.ai_lastActivatedWindow == apInfo.ai_thisWindow))
        {
            return;
        }

        if (apInfo.ai_thisWindow != NULL)
        {
            apInfo.ai_lastActivatedWindow = apInfo.ai_thisWindow;
            ActivateWindow(apInfo.ai_thisWindow);
        }

        return;
    }

    if (ie->ie_Class == IECLASS_RAWMOUSE)
    {
        switch (ie->ie_Code)
        {
            case IECODE_NOBUTTON:
                apInfo.ai_mouseHasMoved = TRUE;
                apInfo.ai_lastActivatedWindow = NULL;
                break;

            case MIDDLEDOWN:
                if (apInfo.ai_midMousePreventsActivation)
                    apInfo.ai_middleButtonDown = TRUE;
                break;

            case SELECTDOWN:
                apInfo.ai_leftButtonDown = TRUE;
                break;

            case MENUDOWN:
                apInfo.ai_rightButtonDown = TRUE;
                break;

            case MIDDLEUP:
                apInfo.ai_middleButtonDown = FALSE;
                break;

            case SELECTUP:
                apInfo.ai_leftButtonDown = FALSE;
                break;

            case MENUUP:
                apInfo.ai_rightButtonDown = FALSE;
                break;
        }
    }
}

/************************************************************************************/

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

/************************************************************************************/

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
    IntuitionBase = (struct IntuitionBase *)OpenLibrary(INTUITIONNAME, 39L);
    LayersBase = OpenLibrary("layers.library", 0);
    IconBase = OpenLibrary(ICONNAME, 0);
    CxBase = OpenLibrary("commodities.library", 0);

    aState.as_msgPort = NULL;
    if (DOSBase && IntuitionBase && LayersBase && IconBase && CxBase &&
        initiate(wbs, &aState))
    {
        handleCx(&aState);
        error = RETURN_OK;
    }

    if (CxBase)
        freeResources(&aState);

    CloseLibrary(CxBase);
    CloseLibrary(IconBase);
    CloseLibrary(LayersBase);
    CloseLibrary((struct Library *)IntuitionBase);
    CloseLibrary((struct Library *)DOSBase);

    if (wbs != NULL)
    {
        Forbid();
        ReplyMsg((struct Message *)wbs);
    }

    return error;
}
