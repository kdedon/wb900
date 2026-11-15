/*
    Copyright (C) 1995-2006, The AROS Development Team. All rights reserved.

    Blanker commodity -- blanks the display after a time without input.
*/

/******************************************************************************

    NAME

        Blanker

    SYNOPSIS

        Tool types: CX_PRIORITY=N, SECONDS=N, BLANKKEY=HOTKEY

    LOCATION

        SYS:Tools/Commodities

    FUNCTION

        Blanks the display after SECONDS (30) without mouse or key input,
        or at once on BLANKKEY.  The HR card's mono display is switched
        off; any other is covered by a black window.  A key press or
        mouse input brings the display back.

******************************************************************************/

#include <exec/exec.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <workbench/startup.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <graphics/layers.h>
#include <libraries/commodities.h>
#include <resources/framebuf.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/commodities.h>
#include <proto/alib.h>
#include <proto/icon.h>

/************************************************************************************/

#define CMD_STARTBLANK  1
#define CMD_STOPBLANK   2

/* The hotkey's sender ID */
#define EVT_BLANKKEY    1

struct DosLibrary    *DOSBase;
struct IntuitionBase *IntuitionBase;
struct GfxBase       *GfxBase;
struct Library       *IconBase;
struct Library       *CxBase;
APTR                  FrameBufBase;

static struct NewBroker nb =
{
   NB_VERSION,
   "Blanker",
   "Screen Blanker",
   "Blanks the display when idle",
   NBU_NOTIFY | NBU_UNIQUE,
   0,
   0,
   NULL,
   0
};

static struct MsgPort   *cxport;
static struct Window    *win;
static struct ColorMap  *cm;
static struct View      *view;
static struct Task      *maintask;
static CxObj            *cxbroker, *cxcust;
static ULONG            cxmask, actionmask;
static WORD             actioncmd;
static WORD             blankwait = 30;
static LONG             blackpen;
static BYTE             actionsig = -1;
static BOOL             blanked, quitme, disabled, mono;

/************************************************************************************/

static void Cleanup(void)
{
    struct Message  *cxmsg;

    if(win)
    {
        ReleasePen(cm, blackpen);
        CloseWindow(win);
    }
    if(view)
        LoadView(view);

    if(cxbroker)
        DeleteCxObjAll(cxbroker);
    if(cxport)
    {
        while((cxmsg = GetMsg(cxport)))
        {
            ReplyMsg(cxmsg);
        }

        DeleteMsgPort(cxport);
    }

    if(actionsig != -1)
        FreeSignal(actionsig);
}

/************************************************************************************/

static BOOL Init(void)
{
    struct FBInfo *fi;

    maintask = FindTask(0);
    actionsig = AllocSignal(-1);
    actionmask = 1L << actionsig;

    /* The HR card's mono video can be switched off */
    FrameBufBase = OpenResource(FRAMEBUFNAME);
    if (FrameBufBase != NULL)
    {
        fi = GetFBInfo();
        mono = fi->fbi_Card == FBC_HR &&
            fi->fbi_Modes[fi->fbi_Mode].fbm_Format == FBF_MONO;
    }

    return actionsig != -1;
}

/************************************************************************************/

/* The tool types, and the hotkey, which needs the broker */
static BOOL GetArguments(struct WBStartup *wbs)
{
    UBYTE **array = NULL;
    STRPTR key;
    CxObj *hot;
    BOOL ok = TRUE;

    if (wbs != NULL)
    {
        array = ArgArrayInit(0, (UBYTE **)wbs);
        nb.nb_Pri = ArgInt(array, "CX_PRIORITY", 0);
        blankwait = ArgInt(array, "SECONDS"    , blankwait);
    }

    if (!(cxport = CreateMsgPort()))
    {
        ok = FALSE;
    }
    else
    {
        nb.nb_Port = cxport;
        cxmask = 1L << cxport->mp_SigBit;

        if (!(cxbroker = CxBroker(&nb, 0)))
        {
            ok = FALSE;
        }
        else if ((key = ArgString(array, "BLANKKEY", NULL)) != NULL)
        {
            hot = HotKey(key, cxport, EVT_BLANKKEY);
            AttachCxObj(cxbroker, hot);
        }
    }

    if (wbs != NULL)
        ArgArrayDone();

    return ok;
}

/************************************************************************************/

static void BlankerAction(CxMsg *msg,CxObj *obj)
{
    struct InputEvent *ie = (struct InputEvent *)CxMsgData(msg);
    static ULONG       timecounter = 0;

    if (ie->ie_Class == IECLASS_TIMER)
    {
        if (disabled)
        {
            timecounter = 0;
        }
        else if (!blanked)
        {
            timecounter++;
            if(timecounter >= blankwait * 10)
            {
                actioncmd = CMD_STARTBLANK;
                Signal(maintask, actionmask);
                blanked = TRUE;
            }
        }
    }
    else if (ie->ie_Class == IECLASS_RAWMOUSE ||
             (ie->ie_Class == IECLASS_RAWKEY &&
              !(ie->ie_Code & IECODE_UP_PREFIX) &&
              !(ie->ie_Qualifier & IEQUALIFIER_REPEAT)))
    {
        /* A key's release or repeat does not count: the blank key
           is still held */
        timecounter = 0;
        if (blanked)
        {
            actioncmd = CMD_STOPBLANK;
            Signal(maintask, actionmask);
            blanked = FALSE;
        }
    }
}

/************************************************************************************/

static BOOL InitCX(void)
{
    if (!(cxcust = CxCustom(BlankerAction, 0)))
    {
        return FALSE;
    }

    AttachCxObj(cxbroker, cxcust);
    ActivateCxObj(cxbroker, 1);

    return TRUE;
}

/************************************************************************************/

/* The mono display goes dark with no view; any other gets a black window */
static void MakeWin(void)
{
    struct Screen  *screenPtr;
    struct TagItem  t[9];

    if (mono)
    {
        view = GfxBase->ActiView;
        LoadView(NULL);
        return;
    }

    if(!(screenPtr = LockPubScreen(NULL)))
        return;

    t[0].ti_Tag = WA_Left;          t[0].ti_Data = 0;
    t[1].ti_Tag = WA_Top;           t[1].ti_Data = 0;
    t[2].ti_Tag = WA_Width;         t[2].ti_Data = screenPtr->Width;
    t[3].ti_Tag = WA_Height;        t[3].ti_Data = screenPtr->Height;
    t[4].ti_Tag = WA_BackFill;      t[4].ti_Data = (IPTR)LAYERS_NOBACKFILL;
    t[5].ti_Tag = WA_SimpleRefresh; t[5].ti_Data = TRUE;
    t[6].ti_Tag = WA_Borderless;    t[6].ti_Data = TRUE;
    t[7].ti_Tag = WA_PubScreen;     t[7].ti_Data = (IPTR)screenPtr;
    t[8].ti_Tag = TAG_DONE;
    win = OpenWindowTagList(NULL, t);

    UnlockPubScreen(NULL, screenPtr);

    if(win)
    {
        cm = win->WScreen->ViewPort.ColorMap;
        blackpen = ObtainBestPenA(cm, 0x00000000, 0x00000000, 0x00000000, NULL);
        SetAPen(win->RPort, blackpen);
        RectFill(win->RPort, 0, 0, win->Width - 1, win->Height - 1);
    }
}

/************************************************************************************/

static void HandleAction(void)
{
    switch(actioncmd)
    {
        case CMD_STARTBLANK:
            if (!win && !view) MakeWin();
            break;

        case CMD_STOPBLANK:
            if (win)
            {
                ReleasePen(cm, blackpen);
                CloseWindow(win);
                win = 0;
            }
            if (view)
            {
                LoadView(view);
                view = NULL;
            }
            break;
    }
}

/************************************************************************************/

static void HandleCx(void)
{
    CxMsg *msg;
    while((msg = (CxMsg *)GetMsg(cxport)))
    {
        switch(CxMsgType(msg))
        {
            case CXM_IEVENT:
                if (CxMsgID(msg) == EVT_BLANKKEY && !disabled)
                {
                    blanked = TRUE;
                    actioncmd = CMD_STARTBLANK;
                    HandleAction();
                }
                break;

            case CXM_COMMAND:
                switch(CxMsgID(msg))
                {
                    case CXCMD_DISABLE:
                        ActivateCxObj(cxbroker,0L);
                        disabled = TRUE;
                        break;

                    case CXCMD_ENABLE:
                        ActivateCxObj(cxbroker,1L);
                        disabled = FALSE;
                        break;

                    case CXCMD_UNIQUE:
                    case CXCMD_KILL:
                        quitme = TRUE;
                        break;

                } /* switch(CxMsgID(msg)) */
                break;
        } /* switch (CxMsgType(msg))*/

        ReplyMsg((struct Message *)msg);

    } /* while((msg = (CxMsg *)GetMsg(cxport))) */
}

/************************************************************************************/

static void HandleAll(void)
{
    ULONG sigs;

    while(!quitme)
    {
        sigs = Wait(cxmask | actionmask | SIGBREAKF_CTRL_C);

        if (sigs & cxmask) HandleCx();
        if (sigs & actionmask) HandleAction();
        if (sigs & SIGBREAKF_CTRL_C) quitme = TRUE;
    } /* while(!quitme) */
}

/************************************************************************************/

int main(void)
{
    struct Process *me = (struct Process *)FindTask(NULL);
    struct WBStartup *wbs = NULL;
    int error = RETURN_FAIL;

    if (me->pr_CLI == NULL)
    {
        WaitPort(&me->pr_MsgPort);
        wbs = (struct WBStartup *)GetMsg(&me->pr_MsgPort);
    }

    DOSBase = (struct DosLibrary *)OpenLibrary(DOSNAME, 36L);
    IntuitionBase = (struct IntuitionBase *)OpenLibrary(INTUITIONNAME, 39L);
    GfxBase = (struct GfxBase *)OpenLibrary(GRAPHICSNAME, 39L);
    IconBase = OpenLibrary(ICONNAME, 0);
    CxBase = OpenLibrary("commodities.library", 0);

    if (DOSBase && IntuitionBase && GfxBase && IconBase && CxBase &&
        Init() && GetArguments(wbs) && InitCX())
    {
        HandleAll();
        error = RETURN_OK;
    }

    if (CxBase)
        Cleanup();

    CloseLibrary(CxBase);
    CloseLibrary(IconBase);
    CloseLibrary((struct Library *)GfxBase);
    CloseLibrary((struct Library *)IntuitionBase);
    CloseLibrary((struct Library *)DOSBase);

    if (wbs != NULL)
    {
        Forbid();
        ReplyMsg((struct Message *)wbs);
    }

    return error;
}
