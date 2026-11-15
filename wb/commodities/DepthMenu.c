/*
    Copyright (C) 2007, The AROS Development Team. All rights reserved.

    DepthMenu commodity -- shows list of window/screens in popupmenu.
*/

/******************************************************************************

    NAME

        DepthMenu

    SYNOPSIS

        Tool types: CX_PRIORITY=N

    LOCATION

        SYS:Tools/Commodities

    FUNCTION

        Shows popup menu with list of screens and windows while the right
        mouse button is held on the depth gadget of a screen or window.
        Releasing it on an entry brings that window or screen to front.

    INPUTS

        CX_PRIORITY  --  The priority of the commodity

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
#include <intuition/screens.h>
#include <graphics/rastport.h>
#include <graphics/gfxbase.h>
#include <libraries/commodities.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/layers.h>
#include <proto/commodities.h>
#include <proto/alib.h>
#include <proto/icon.h>

/***************************************************************************/

#define DM_WINDOW       1
#define DM_SCREEN       2

#define DM_MAXENTRY     15
#define DM_MAXSTRLEN    30
#define DM_BORDERWIDTH  4

#define SYSGADTYPE(gad) ((gad)->GadgetType & GTYP_SYSTYPEMASK)

struct DosLibrary     *DOSBase;
struct IntuitionBase  *IntuitionBase;
struct GfxBase        *GfxBase;
struct Library        *LayersBase;
struct Library        *IconBase;
struct Library        *CxBase;

/* The Depth broker */
static struct NewBroker nb =
{
    NB_VERSION,
    "DepthMenu",
    "Depth Menu",
    "Window and screen list on the depth gadgets",
    NBU_NOTIFY | NBU_UNIQUE,
    0,
    0,
    NULL,
    0
};

typedef struct _DMState
{
    CxObj          *cs_broker;
    struct MsgPort *cs_msgPort;
} DMState;

/*
 * The input handler sees the right button go down on a depth gadget and
 * up again, and swallows both, so intuition shows no menus; the
 * commodity's task opens the popup and acts on the choice.
 */
typedef struct _DMData
{
    struct Window *popupwindow;
    WORD entries;
    WORD mode;
    void *address[DM_MAXENTRY]; /* struct Window * or struct Screen * */
    char title[DM_MAXENTRY][DM_MAXSTRLEN];
    WORD selected;  /* -1 means no entry selected */
    struct DrawInfo *drawinfo;
    UBYTE bgpen;
    UBYTE txtpen;
    UBYTE highpen;
    UBYTE shinepen;
    UBYTE shadowpen;
    struct Task *task;
    ULONG mask;
    struct Screen *screen;      /* where the button went down */
    WORD xpos, ypos;
    BOOL down;                  /* the button is held */
    BOOL tried;                 /* the popup was opened, or had no entries */
} DMData;

static DMData dmdata;

/************************************************************************************/

static void depthMenu(CxMsg *msg, CxObj *co);
static void showPopup(void);
static void closePopup(void);
static void handleMouseMove(void);
static void drawEntry(LONG entry, BOOL selstate);
static BOOL depthGadHit(struct Gadget *gad, WORD left, WORD top, WORD width,
    WORD height, UWORD type, WORD mousex, WORD mousey);

/************************************************************************************/

static BOOL initiate(struct WBStartup *wbs, DMState *cs)
{
    CxObj *customObj;
    BYTE sig;

    cs->cs_broker = NULL;

    if (wbs != NULL)
    {
        UBYTE  **array = ArgArrayInit(0, (UBYTE **)wbs);

        nb.nb_Pri = ArgInt(array, "CX_PRIORITY", 0);

        ArgArrayDone();
    }

    if ((sig = AllocSignal(-1)) == -1)
    {
        return FALSE;
    }
    dmdata.task = FindTask(NULL);
    dmdata.mask = 1L << sig;

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

    customObj = CxCustom(depthMenu, 0);

    if (customObj == NULL)
    {
        return FALSE;
    }

    AttachCxObj(cs->cs_broker, customObj);
    ActivateCxObj(cs->cs_broker, TRUE);

    return TRUE;
}

/************************************************************************************/

static void freeResources(DMState *cs)
{
    struct Message *cxm;

    if (cs->cs_broker != NULL)
    {
        DeleteCxObjAll(cs->cs_broker);
    }

    closePopup();

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

/* On the input task: whether the pointer is on a depth gadget */
static BOOL menuDown(void)
{
    struct Screen *screen;
    struct Layer  *layer;
    struct Window *window;

    screen = IntuitionBase->FirstScreen; /* frontmost screen */

    if (screen == NULL)
    {
        return FALSE;
    }

    LockLayerInfo(&screen->LayerInfo);
    layer = WhichLayer(&screen->LayerInfo, screen->MouseX, screen->MouseY);
    UnlockLayerInfo(&screen->LayerInfo);

    if (layer == NULL)
    {
        return FALSE;
    }

    dmdata.screen = screen;
    dmdata.xpos = screen->MouseX;
    dmdata.ypos = screen->MouseY;

    if (layer == screen->BarLayer) /* Title bar of screen */
    {
        dmdata.mode = DM_SCREEN;
        return depthGadHit(screen->FirstGadget, 0, 0, screen->Width,
            screen->Height, GTYP_SDEPTH, screen->MouseX, screen->MouseY);
    }

    window = layer->Window;
    dmdata.mode = DM_WINDOW;
    return window != NULL && depthGadHit(window->FirstGadget,
        window->LeftEdge, window->TopEdge, window->Width, window->Height,
        GTYP_WDEPTH, screen->MouseX, screen->MouseY);
}

/* On the input task, ahead of intuition */
static void depthMenu(CxMsg *cxm, CxObj *co)
{
    struct InputEvent *ie = (struct InputEvent *)CxMsgData(cxm);

    if (ie->ie_Class == IECLASS_RAWMOUSE)
    {
        switch (ie->ie_Code)
        {
            case MENUDOWN:
                if (!dmdata.down && menuDown())
                {
                    dmdata.down = TRUE;
                    DisposeCxMsg(cxm);
                    Signal(dmdata.task, dmdata.mask);
                }
                break;

            case MENUUP:
                if (dmdata.down)
                {
                    dmdata.down = FALSE;
                    DisposeCxMsg(cxm);
                    Signal(dmdata.task, dmdata.mask);
                }
                break;

            case IECODE_NOBUTTON:
                if (dmdata.down)
                {
                    Signal(dmdata.task, dmdata.mask);
                }
                break;
        }
    }
}

/************************************************************************************/

/* The button went down, moved or came up */
static void handleMenu(void)
{
    LONG entry;
    struct Screen *screen;
    struct Window *checkwin;
    struct Screen *checkscreen;
    ULONG lock;

    if (dmdata.down && !dmdata.tried)
    {
        dmdata.tried = TRUE;
        showPopup();
    }

    if (dmdata.popupwindow == NULL)
    {
        dmdata.tried = dmdata.down;
        return;
    }

    handleMouseMove();

    if (dmdata.down)
    {
        return;
    }

    entry = dmdata.selected;
    screen = dmdata.popupwindow->WScreen;
    closePopup();
    dmdata.tried = FALSE;

    if (entry == -1)
    {
        return;
    }

    if (dmdata.mode == DM_WINDOW)
    {
        /* does selected window still exist? */
        lock = LockIBase(0);
        for (checkwin = screen->FirstWindow; checkwin;
             checkwin = checkwin->NextWindow)
        {
            if (checkwin == dmdata.address[entry])
            {
                break;
            }
        }
        UnlockIBase(lock);

        if (checkwin)
        {
            WindowToFront(checkwin);
            ActivateWindow(checkwin);
        }
    }
    else
    {
        /* does selected screen still exist? */
        lock = LockIBase(0);
        for (checkscreen = IntuitionBase->FirstScreen; checkscreen;
             checkscreen = checkscreen->NextScreen)
        {
            if (checkscreen == dmdata.address[entry])
            {
                break;
            }
        }
        UnlockIBase(lock);

        if (checkscreen)
        {
            ScreenToFront(checkscreen);
        }
    }
}

/************************************************************************************/

static void closePopup(void)
{
    if (dmdata.popupwindow)
    {
        FreeScreenDrawInfo(dmdata.popupwindow->WScreen, dmdata.drawinfo);
        dmdata.drawinfo = NULL;
        CloseWindow(dmdata.popupwindow);
        dmdata.popupwindow = NULL;
    }
}

/************************************************************************************/

static void handleMouseMove(void)
{
    struct Window *pw = dmdata.popupwindow;
    struct Screen *screen = pw->WScreen;
    LONG entry = -1;

    /* are we within popupwindow? */
    if (
            (screen->MouseX > pw->LeftEdge)
            && (screen->MouseX < pw->LeftEdge + pw->Width)
            && (screen->MouseY > pw->TopEdge)
            && (screen->MouseY < pw->TopEdge + pw->Height)
       )
    {
        entry = (screen->MouseY - pw->TopEdge - DM_BORDERWIDTH)
            / pw->RPort->TxHeight;
        if ((entry < 0) || (entry >= dmdata.entries)) entry = -1;
    }

    if (entry != dmdata.selected)
    {
        /* undraw old entry */
        if (dmdata.selected != -1)
        {
            drawEntry(dmdata.selected, FALSE);
        }

        /* draw selected */
        if (entry != -1)
        {
            drawEntry(entry, TRUE);
        }

        dmdata.selected = entry;
    }
}

/************************************************************************************/

static void copyTitle(char *d, CONST_STRPTR s)
{
    WORD n;

    for (n = 0; n < DM_MAXSTRLEN - 1 && s[n]; n++)
        d[n] = s[n];
    d[n] = 0;
}

static WORD titleLength(char *s)
{
    WORD n = 0;

    while (s[n])
        n++;
    return n;
}

static void showPopup(void)
{
    struct Screen *screen = dmdata.screen;
    struct Screen *popupscreen;
    struct RastPort *rp;
    struct TagItem t[8];
    LONG maxtextwidth = 0;
    LONG textwidth;
    LONG width, height, left, top;
    ULONG lock;
    int i;

    dmdata.selected = -1;
    dmdata.entries = 0;

    lock = LockIBase(0);

    /* The screen may have closed since the button went down */
    for (popupscreen = IntuitionBase->FirstScreen; popupscreen;
         popupscreen = popupscreen->NextScreen)
    {
        if (popupscreen == screen)
        {
            break;
        }
    }

    if (popupscreen == NULL || popupscreen->RastPort.Font == NULL)
    {
        UnlockIBase(lock);
        return;
    }

    if (dmdata.mode == DM_WINDOW)
    {
        struct Window *window = screen->FirstWindow;

        while (window && (dmdata.entries < DM_MAXENTRY))
        {
            if (window->Title && (window->WScreen == screen))
            {
                dmdata.address[dmdata.entries] = window;
                copyTitle(dmdata.title[dmdata.entries], window->Title);
                dmdata.entries++;
            }
            window = window->NextWindow;
        }
    }
    else
    {
        screen = screen->NextScreen; /* Don't add frontmost screen, start with second screen */
        while (screen && (dmdata.entries < DM_MAXENTRY))
        {
            if (screen->Title)
            {
                dmdata.address[dmdata.entries] = screen;
                copyTitle(dmdata.title[dmdata.entries], screen->Title);
                dmdata.entries++;
            }
            screen = screen->NextScreen;
        }
    }

    /* calculate max. text length */
    for (i=0 ; i < dmdata.entries ; i++)
    {
        textwidth = TextLength(&popupscreen->RastPort, dmdata.title[i], titleLength(dmdata.title[i]));
        if (textwidth > maxtextwidth) maxtextwidth = textwidth;
    }

    UnlockIBase(lock);

    if (dmdata.entries == 0)
    {
        return;
    }

    width = maxtextwidth + 2 * DM_BORDERWIDTH;
    height = dmdata.entries * popupscreen->RastPort.TxHeight + 2 * DM_BORDERWIDTH;
    left = dmdata.xpos - width / 2;
    top = dmdata.ypos - height / 2;
    if (left + width > popupscreen->Width) left = popupscreen->Width - width;
    if (top + height > popupscreen->Height) top = popupscreen->Height - height;
    if (left < 0) left = 0;
    if (top < 0) top = 0;

    t[0].ti_Tag = WA_Left;          t[0].ti_Data = left;
    t[1].ti_Tag = WA_Top;           t[1].ti_Data = top;
    t[2].ti_Tag = WA_Width;         t[2].ti_Data = width;
    t[3].ti_Tag = WA_Height;        t[3].ti_Data = height;
    t[4].ti_Tag = WA_Borderless;    t[4].ti_Data = TRUE;
    t[5].ti_Tag = WA_SimpleRefresh; t[5].ti_Data = TRUE;
    t[6].ti_Tag = WA_CustomScreen;  t[6].ti_Data = (IPTR)popupscreen;
    t[7].ti_Tag = TAG_DONE;
    dmdata.popupwindow = OpenWindowTagList(NULL, t);

    if (dmdata.popupwindow == NULL)
    {
        return;
    }

    rp = dmdata.popupwindow->RPort;
    SetFont(rp, popupscreen->RastPort.Font);    /* use the screen's font for the menu */
    SetDrMd(rp, JAM1); /* for text rendering */
    dmdata.drawinfo = GetScreenDrawInfo(popupscreen);
    if (dmdata.drawinfo)
    {
        dmdata.bgpen = dmdata.drawinfo->dri_Pens[BACKGROUNDPEN];
        dmdata.txtpen = dmdata.drawinfo->dri_Pens[TEXTPEN];
        dmdata.highpen = dmdata.drawinfo->dri_Pens[FILLPEN];
        dmdata.shinepen =dmdata.drawinfo->dri_Pens[SHINEPEN];
        dmdata.shadowpen = dmdata.drawinfo->dri_Pens[SHADOWPEN];
    }
    else
    {
        dmdata.bgpen = 0;
        dmdata.txtpen = 1;
        dmdata.highpen = 3;
        dmdata.shinepen = 1;
        dmdata.shadowpen = 1;
    }

    SetAPen(rp, dmdata.bgpen);
    RectFill(rp, 0, 0, width - 1, height - 1);
    SetAPen(rp, dmdata.shinepen);
    Move(rp, width-1, 0);
    Draw(rp, 0, 0);
    Draw(rp, 0, height-1);
    SetAPen(rp, dmdata.shadowpen);
    Draw(rp, width-1, height-1);
    Draw(rp, width-1, 0);

    for(i=0; i<dmdata.entries; i++)
    {
        drawEntry(i, FALSE);
    }
}

/************************************************************************************/

static void drawEntry(LONG entry, BOOL selstate)
{
    struct RastPort *rp = dmdata.popupwindow->RPort;

    if (selstate)
    {
        SetAPen(rp, dmdata.highpen);
    }
    else
    {
        SetAPen(rp, dmdata.bgpen);
    }

    RectFill(rp, DM_BORDERWIDTH, entry * rp->TxHeight + DM_BORDERWIDTH,
            dmdata.popupwindow->Width - DM_BORDERWIDTH - 1,
            (entry+1) * rp->TxHeight + DM_BORDERWIDTH - 1);

    SetAPen(rp, dmdata.txtpen);
    Move(rp, DM_BORDERWIDTH, entry * rp->TxHeight + rp->TxBaseline + DM_BORDERWIDTH);
    Text(rp, dmdata.title[entry], titleLength(dmdata.title[entry]));
}

/************************************************************************************/

/* Whether (mousex, mousey) is on an enabled system gadget of the type in
   the list, in a box at left, top of width and height */
static BOOL depthGadHit(struct Gadget *gad, WORD left, WORD top, WORD width,
    WORD height, UWORD type, WORD mousex, WORD mousey)
{
    for( ; gad; gad = gad->NextGadget)
    {
        if ( ! (gad->Flags & GFLG_DISABLED) && SYSGADTYPE(gad) == type)
        {
            WORD x = left + gad->LeftEdge;
            WORD y = top + gad->TopEdge;
            WORD w = gad->Width;
            WORD h = gad->Height;

            if (gad->Flags & GFLG_RELRIGHT)  x += width  - 1;
            if (gad->Flags & GFLG_RELBOTTOM) y += height - 1;
            if (gad->Flags & GFLG_RELWIDTH)  w += width;
            if (gad->Flags & GFLG_RELHEIGHT) h += height;

            if (
                    (mousex >= x) &&
                    (mousey >= y) &&
                    (mousex < x + w) &&
                    (mousey < y + h)
               )
            {
                return TRUE;
            }
        }
    }
    return FALSE;
}

/************************************************************************************/

static void handleCx(DMState *cs)
{
    CxMsg *msg;
    BOOL   quit = FALSE;
    LONG   signals;

    while (!quit)
    {
        signals = Wait((1L << nb.nb_Port->mp_SigBit) | dmdata.mask |
            SIGBREAKF_CTRL_C);

        if (signals & dmdata.mask)
        {
            handleMenu();
        }

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
    DMState cState;
    int     error = RETURN_FAIL;

    if (me->pr_CLI == NULL)
    {
        WaitPort(&me->pr_MsgPort);
        wbs = (struct WBStartup *)GetMsg(&me->pr_MsgPort);
    }

    DOSBase = (struct DosLibrary *)OpenLibrary(DOSNAME, 36L);
    IntuitionBase = (struct IntuitionBase *)OpenLibrary(INTUITIONNAME, 39L);
    GfxBase = (struct GfxBase *)OpenLibrary(GRAPHICSNAME, 39L);
    LayersBase = OpenLibrary("layers.library", 0);
    IconBase = OpenLibrary(ICONNAME, 0);
    CxBase = OpenLibrary("commodities.library", 0);

    cState.cs_msgPort = NULL;
    if (DOSBase && IntuitionBase && GfxBase && LayersBase && IconBase &&
        CxBase && initiate(wbs, &cState))
    {
        handleCx(&cState);
        error = RETURN_OK;
    }

    if (CxBase)
        freeResources(&cState);

    CloseLibrary(CxBase);
    CloseLibrary(IconBase);
    CloseLibrary(LayersBase);
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
