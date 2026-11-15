/*
    Copyright (C) 1995-2013, The AROS Development Team. All rights reserved.

    ClickToFront commodity -- puts windows to front when clicked in.
*/

/******************************************************************************

    NAME

        ClickToFront

    SYNOPSIS

        Tool types: CX_PRIORITY=N, QUALIFIER=KEYS, NUMCLICKS=N

    LOCATION

        SYS:Tools/Commodities

    FUNCTION

        Automatically raises and activates a window when clicking in it.

    INPUTS

        CX_PRIORITY  --  The priority of the commodity
        
        QUALIFIER    --  Qualifiers to match the clicks, as in a hotkey
                         (LEFT_ALT, RIGHT_ALT, CTRL, "lshift alt" or NONE).
        
        NUMCLICKS    --  Number of clicks to bring window to front. Value
                         must be greater than 0.

    RESULT

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

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

/* The key qualifiers a click must have exactly; Caps Lock does not count */
#define CF_QUALS (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT | \
                  IEQUALIFIER_CONTROL | IEQUALIFIER_LALT | IEQUALIFIER_RALT | \
                  IEQUALIFIER_LCOMMAND | IEQUALIFIER_RCOMMAND)

/* The ClickToFront broker */
static struct NewBroker nb =
{
    NB_VERSION,
    "ClickToFront",
    "Click To Front",
    "Double-click a window to bring it to front",
    NBU_NOTIFY | NBU_UNIQUE,
    0,
    0,
    NULL,
    0
};


typedef struct _CFState
{
    CxObj          *cs_broker;
    struct MsgPort *cs_msgPort;
} CFState;


typedef struct CF
{
    struct Window *ci_thisWindow;
    struct Window *ci_lastWindow;
    UWORD          ci_qualifiers; /* Qualifiers that must match */
    ULONG          ci_clicksToDo; /* Bring to front after how many clicks? */
    ULONG          ci_clicksDone; /* How many clicks are we already aware of? */
    ULONG          ci_lcSeconds;  /* Time stamp for the last click */
    ULONG          ci_lcMicros;
} CF;


static CF cfInfo;

/************************************************************************************/

static void getQualifier(STRPTR qualString);
static void clicktoFront(CxMsg *msg, CxObj *co);

/************************************************************************************/

static BOOL initiate(struct WBStartup *wbs, CFState *cs)
{
    CxObj *customObj;

    cs->cs_broker = NULL;

    if (wbs != NULL)
    {
        UBYTE  **array = ArgArrayInit(0, (UBYTE **)wbs);

        nb.nb_Pri = ArgInt(array, "CX_PRIORITY", 0);

        cfInfo.ci_clicksToDo = ArgInt(array, "NUMCLICKS", 0);

        getQualifier(ArgString(array, "QUALIFIER", NULL));

        ArgArrayDone();
    }

    if (cfInfo.ci_clicksToDo == 0)
        cfInfo.ci_clicksToDo = 2; /* Default value is 2 */

    cs->cs_msgPort = CreateMsgPort();

    if (cs->cs_msgPort == NULL)
    {
        return FALSE;
    }

    nb.nb_Port = cs->cs_msgPort;

    cs->cs_broker = CxBroker(&nb, 0);

    if (cs->cs_broker == NULL)
    {
        return FALSE;
    }

    customObj = CxCustom(clicktoFront, 0);

    if (customObj == NULL)
    {
        return FALSE;
    }

    AttachCxObj(cs->cs_broker, customObj);
    ActivateCxObj(cs->cs_broker, TRUE);

    cfInfo.ci_thisWindow = IntuitionBase->ActiveWindow;

    return TRUE;
}

/************************************************************************************/

/* The qualifiers of a hotkey description; NONE (no match) means none */
static void getQualifier(STRPTR qualString)
{
    IX ix;

    if (qualString == NULL)
    {
        return;
    }

    ix.ix_Version = IX_VERSION;

    if (ParseIX(qualString, &ix) == 0)
    {
        cfInfo.ci_qualifiers = ix.ix_Qualifier & CF_QUALS;
    }
}

/************************************************************************************/

static void freeResources(CFState *cs)
{
    struct Message *cxm;

    if (cs->cs_broker != NULL)
    {
        DeleteCxObjAll(cs->cs_broker);
    }

    if (cs->cs_msgPort != NULL)
    {
        while ((cxm = GetMsg(cs->cs_msgPort)))
        {
            ReplyMsg(cxm);
        }

        DeleteMsgPort(cs->cs_msgPort);
    }
}

/************************************************************************************/

/* On the input task, ahead of intuition */
static void clicktoFront(CxMsg *cxm, CxObj *co)
{
    /* NOTE! Should use arbitration for IntuitionBase... */

    struct InputEvent *ie = (struct InputEvent *)CxMsgData(cxm);
    struct Screen *screen;
    struct Layer  *layer, *l;

    if (ie->ie_Class != IECLASS_RAWMOUSE || ie->ie_Code != SELECTDOWN)
    {
        return;
    }

    if ((ie->ie_Qualifier & CF_QUALS) != cfInfo.ci_qualifiers)
    {
        return;
    }

    cfInfo.ci_lastWindow = cfInfo.ci_thisWindow;

    if (IntuitionBase->ActiveWindow != NULL)
    {
        screen = IntuitionBase->ActiveWindow->WScreen;
    }
    else
    {
        screen = IntuitionBase->ActiveScreen;
    }

    if (screen == NULL)
    {
        return;
    }

    LockLayerInfo(&screen->LayerInfo);
    layer = WhichLayer(&screen->LayerInfo, screen->MouseX, screen->MouseY);

    /* Only a window behind another window comes to front, not one
       behind just the screen's title bar */
    for (l = layer ? layer->front : NULL; l != NULL; l = l->front)
    {
        if (l->Window != NULL)
        {
            break;
        }
    }
    UnlockLayerInfo(&screen->LayerInfo);

    cfInfo.ci_thisWindow = (layer != NULL) ?
        (struct Window *)layer->Window : NULL;

    if (cfInfo.ci_thisWindow == NULL || l == NULL)
    {
        return;
    }

    /*
       Counting clicks is only meaningful if cfInfo.ci_clicksToDo
       is no less than 2
    */
    if (cfInfo.ci_clicksToDo > 1)
    {
        cfInfo.ci_clicksDone++;

        /*
           Return if the delay between two clicks is longer than
           Input-Preferences-set double-click delay
        */
        if (!DoubleClick(cfInfo.ci_lcSeconds,
                         cfInfo.ci_lcMicros,
                         ie->ie_TimeStamp.tv_secs,
                         ie->ie_TimeStamp.tv_micro))
        {
            cfInfo.ci_lcSeconds = ie->ie_TimeStamp.tv_secs;
            cfInfo.ci_lcMicros  = ie->ie_TimeStamp.tv_micro;
            cfInfo.ci_clicksDone = 1L;
            return;
        }

        cfInfo.ci_lcSeconds = ie->ie_TimeStamp.tv_secs;
        cfInfo.ci_lcMicros  = ie->ie_TimeStamp.tv_micro;

        /* Return if the user didn't make enough clicks */
        if (cfInfo.ci_clicksDone < cfInfo.ci_clicksToDo)
        {
            return;
        }

        /* Return if the clicks weren't made in the same window */
        if (cfInfo.ci_lastWindow != cfInfo.ci_thisWindow)
        {
            cfInfo.ci_clicksDone = 1L;
            return;
        }

        /*
           If we didn't return yet, that means that all conditions
           are good to bring the window to front, and it will be
           done now. We just reset cfInfo.ci_clicksDone to 0 in
           order to be ready for another bring-to-front loop...
        */
        cfInfo.ci_clicksDone = 0L;
    }

    WindowToFront(cfInfo.ci_thisWindow);

    if (cfInfo.ci_thisWindow != IntuitionBase->ActiveWindow)
    {
        ActivateWindow(cfInfo.ci_thisWindow);
    }
}

/************************************************************************************/

static void handleCx(CFState *cs)
{
    CxMsg *msg;
    BOOL   quit = FALSE;
    LONG   signals;

    while (!quit)
    {
        signals = Wait((1L << nb.nb_Port->mp_SigBit)  | SIGBREAKF_CTRL_C);

        if (signals & (1L << nb.nb_Port->mp_SigBit))
        {
            while ((msg = (CxMsg *)GetMsg(cs->cs_msgPort)))
            {
                switch (CxMsgType(msg))
                {
                    case CXM_COMMAND:
                        switch (CxMsgID(msg))
                        {
                            case CXCMD_DISABLE:
                                ActivateCxObj(cs->cs_broker, FALSE);
                                break;

                            case CXCMD_ENABLE:
                                ActivateCxObj(cs->cs_broker, TRUE);
                                break;

                            case CXCMD_UNIQUE:
                                /* Running the program twice is the same as shutting
                                   down the existing program... */
                                /* Fall through */

                            case CXCMD_KILL:
                                quit = TRUE;
                                break;

                        } /* switch (CxMsgID(msg)) */
                        break;
                } /* switch (CxMsgType(msg))*/

                ReplyMsg((struct Message *)msg);

            } /* while ((msg = (CxMsg *)GetMsg(cs->cs_msgPort))) */
        }

        if (signals & SIGBREAKF_CTRL_C)
        {
            quit = TRUE;
        }

    } /* while(!quit) */
}

/************************************************************************************/

int main(void)
{
    struct Process *me = (struct Process *)FindTask(NULL);
    struct WBStartup *wbs = NULL;
    CFState cState;
    int      error = RETURN_FAIL;

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

    cState.cs_msgPort = NULL;
    if (DOSBase && IntuitionBase && LayersBase && IconBase && CxBase &&
        initiate(wbs, &cState))
    {
        handleCx(&cState);
        error = RETURN_OK;
    }

    if (CxBase)
        freeResources(&cState);

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
