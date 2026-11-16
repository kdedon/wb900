/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.
    Copyright (C) 2001-2003, The MorphOS Development Team. All Rights Reserved.
*/

#include <devices/input.h>
#include <devices/inputevent.h>
#include <devices/rawkeycodes.h>
#include <devices/timer.h>
#include <proto/graphics.h>

#include "intuition_intern.h"

/* What the alert's input handler saw */
struct AlertInput
{
    struct Task *ai_Task;
    BYTE         ai_Sig;
    UWORD        ai_Down;   /* the button or key pressed */
    UWORD        ai_Code;   /* the one released after it: the answer */
};

/*
 * Ahead of intuition's handler: every event goes, and the release of a
 * mouse button, Return or Escape pressed while the alert is up answers it.
 */
static struct InputEvent *alerthandler(struct InputEvent *ie, struct AlertInput *ai)
{
    UWORD code;

    for (; ie; ie = ie->ie_NextEvent)
    {
        code = ie->ie_Code & ~IECODE_UP_PREFIX;
        if (ie->ie_Class == IECLASS_RAWMOUSE ?
            code != IECODE_LBUTTON && code != IECODE_RBUTTON :
            ie->ie_Class != IECLASS_RAWKEY ||
            (code != RAWKEY_RETURN && code != RAWKEY_ESCAPE))
            continue;
        if (!(ie->ie_Code & IECODE_UP_PREFIX))
            ai->ai_Down = code;
        else if (code == ai->ai_Down && !ai->ai_Code)
        {
            ai->ai_Code = code;
            Signal(ai->ai_Task, 1L << ai->ai_Sig);
        }
    }
    return NULL;
}

/* The next line of an alert's text and its position, or NULL */
static UBYTE *alertline(UBYTE **p, WORD *x, WORD *y)
{
    UBYTE *s = *p, *t;

    if (s == NULL)
        return NULL;
    *x = (s[0] << 8) | s[1];
    *y = s[2];
    t = s + 3;
    s = t + strlen(t) + 1;
    *p = *s ? s + 1 : NULL;
    return t;
}

/*****************************************************************************

    NAME */
#include <proto/intuition.h>

        AROS_LH4(BOOL, TimedDisplayAlert,

/*  SYNOPSIS */
        AROS_LHA(ULONG  , alertnumber, D0),
        AROS_LHA(UBYTE *, string     , A0),
        AROS_LHA(UWORD  , height     , D1),
        AROS_LHA(ULONG  , time       , A1),

/*  LOCATION */
        struct IntuitionBase *, IntuitionBase, 137, Intuition)

/*  FUNCTION
        Display an alert with automatic time-out. See DisplayAlert()
        documentation.

    INPUTS
        alertnumber - Alert code
        string - Text data to display
        height - Total height of alert display in pixels
        time   - Timeout measured in display frame refresh periods, of
                 1/50 s; 0 waits for the user.

    RESULT
        TRUE or FALSE depending on user's reaction. FALSE in case of timeout.

    NOTES
        See DisplayAlert() documentation for detailed description of
        parameters.

    EXAMPLE

    BUGS
        Note that this function is obsolete and strongly deprecated for use
        in software. It is present only for backwards compatibility with
        AmigaOS(tm).

    SEE ALSO
        DisplayAlert()

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct GfxBase *GfxBase = GetPrivIBase(IntuitionBase)->GfxBase;
    static struct TextAttr topaz8 = { "topaz.font", 8, FS_NORMAL, 0 };
    static struct TagItem quiet[] = {
        { SA_Depth, 1 }, { SA_Quiet, TRUE }, { SA_ShowTitle, FALSE }, { TAG_DONE, 0 }
    };
    struct AlertInput ai;
    struct Interrupt handler;
    struct MsgPort *port;
    struct IOStdReq *io;
    struct timerequest *tr = NULL;
    struct Screen *scr, *own = NULL;
    struct BitMap *bm, *save = NULL;
    struct RastPort rp;
    struct TextFont *font;
    struct ColorMap *cm;
    UBYTE *p, *t;
    WORD x, y, x0, y0, w, h;
    ULONG fg, bg, maxpen;
    BOOL ret = FALSE;

    xKPrintf("intuition: alert %08lx\n", alertnumber);
    for (p = string; (t = alertline(&p, &x, &y)) != NULL; )
        xKPrintf("  %s\n", t);

    /* The input task can't wait for its own events */
    if (FindTask(NULL) == FindTask("input.device"))
        return FALSE;

    if ((ai.ai_Sig = AllocSignal(-1)) < 0)
        return FALSE;
    ai.ai_Task = FindTask(NULL);
    ai.ai_Down = ai.ai_Code = 0;
    port = CreateMsgPort();
    io = (struct IOStdReq *)CreateIORequest(port, sizeof(struct IOStdReq));
    if (io == NULL || OpenDevice("input.device", 0, (struct IORequest *)io, 0))
        goto noinput;

    handler.is_Node.ln_Type = NT_INTERRUPT;
    handler.is_Node.ln_Pri = 100;
    handler.is_Node.ln_Name = "intuition alert";
    handler.is_Code = (int (*)())alerthandler;
    handler.is_Data = &ai;
    io->io_Command = IND_ADDHANDLER;
    io->io_Data = &handler;
    DoIO((struct IORequest *)io);

    scr = IntuitionBase->FirstScreen;
    if (scr == NULL)
        scr = own = OpenScreenTagList(NULL, quiet);
    if (scr == NULL)
        goto noscreen;

    /* The top of what the screen shows, saved */
    bm = scr->ViewPort.RasInfo->BitMap;
    x0 = scr->ViewPort.RasInfo->RxOffset;
    y0 = scr->ViewPort.RasInfo->RyOffset;
    w = min(scr->Width, GetBitMapAttr(bm, BMA_WIDTH) - x0);
    h = min(height, bm->Rows - y0);
    if (!own)
    {
        save = AllocBitMap(w, h, GetBitMapAttr(bm, BMA_DEPTH), 0, bm);
        if (save == NULL)
            goto noscreen;
        BltBitMap(bm, x0, y0, save, 0, 0, w, h, 0xC0, 0xFF, NULL);
    }

    /* Red on black, as the Amiga's; another pen where red is black */
    InitRastPort(&rp);
    rp.BitMap = bm;
    cm = scr->ViewPort.ColorMap;
    maxpen = (1L << GetBitMapAttr(bm, BMA_DEPTH)) - 1;
    bg = FindColor(cm, 0, 0, 0, maxpen);
    fg = FindColor(cm, 0xFFFFFFFF, 0, 0, maxpen);
    if (fg == bg)
        fg = bg ^ 1;
    SetAPen(&rp, bg);
    RectFill(&rp, x0, y0, x0 + w - 1, y0 + h - 1);
    SetAPen(&rp, fg);
    if (w > 16 && h > 16)
    {
        RectFill(&rp, x0 + 4, y0 + 4, x0 + w - 5, y0 + 7);
        RectFill(&rp, x0 + 4, y0 + h - 8, x0 + w - 5, y0 + h - 5);
        RectFill(&rp, x0 + 4, y0 + 8, x0 + 7, y0 + h - 9);
        RectFill(&rp, x0 + w - 8, y0 + 8, x0 + w - 5, y0 + h - 9);
    }
    font = OpenFont(&topaz8);
    if (font)
        SetFont(&rp, font);
    SetDrMd(&rp, JAM1);
    for (p = string; (t = alertline(&p, &x, &y)) != NULL; )
    {
        Move(&rp, x0 + x, y0 + y);
        Text(&rp, t, strlen(t));
    }
    /* A screen on a bitmap of another size shows by a copy */
    ScrollVPort(&scr->ViewPort);

    if (time)
    {
        tr = (struct timerequest *)CreateIORequest(port, sizeof(struct timerequest));
        if (tr && OpenDevice(TIMERNAME, UNIT_VBLANK, (struct IORequest *)tr, 0))
        {
            DeleteIORequest((struct IORequest *)tr);
            tr = NULL;
        }
        if (tr)
        {
            tr->tr_node.io_Command = TR_ADDREQUEST;
            tr->tr_time.tv_secs = time / 50;
            tr->tr_time.tv_micro = (time % 50) * 20000;
            SendIO((struct IORequest *)tr);
        }
    }
    while (!ai.ai_Code && !(tr && CheckIO((struct IORequest *)tr)))
        Wait((1L << ai.ai_Sig) | (1L << port->mp_SigBit));
    if (tr)
    {
        AbortIO((struct IORequest *)tr);
        WaitIO((struct IORequest *)tr);
        CloseDevice((struct IORequest *)tr);
        DeleteIORequest((struct IORequest *)tr);
    }

    ret = !(alertnumber & AT_DeadEnd) &&
          (ai.ai_Code == IECODE_LBUTTON || ai.ai_Code == RAWKEY_RETURN);

    if (font)
        CloseFont(font);
    if (save)
    {
        BltBitMap(save, 0, 0, bm, x0, y0, w, h, 0xC0, 0xFF, NULL);
        FreeBitMap(save);
        ScrollVPort(&scr->ViewPort);
    }

noscreen:
    io->io_Command = IND_REMHANDLER;
    io->io_Data = &handler;
    DoIO((struct IORequest *)io);
    if (own)
        CloseScreen(own);
    CloseDevice((struct IORequest *)io);
noinput:
    DeleteIORequest((struct IORequest *)io);
    if (port)
        DeleteMsgPort(port);
    FreeSignal(ai.ai_Sig);

    return ret;

    AROS_LIBFUNC_EXIT
} /* TimedDisplayAlert */
