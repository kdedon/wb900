/* SPDX-License-Identifier: MIT */
/*
 * Clock: an analogue and a digital clock in a small window, after AROS's
 * Clock (APL), on intuition and gadtools.  Each second (each minute
 * without the seconds hand) it erases the hands that moved and draws the
 * hands again; the face is drawn only when the window needs it.
 *
 * Usage: Clock [LEFT n] [TOP n] [WIDTH n] [HEIGHT n] [PUBSCREEN name]
 *        [NOSECONDS]
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <devices/timer.h>
#include <intuition/intuition.h>
#include <libraries/gadtools.h>
#include <workbench/startup.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/gadtools.h>
#include <proto/timer.h>

const char *version = "$VER: Clock 1.0 (15.11.2026)";

#define ARG_TEMPLATE "LEFT/N,TOP/N,WIDTH/N,HEIGHT/N,PUBSCREEN/K,NOSECONDS/S"

enum { ARG_LEFT, ARG_TOP, ARG_WIDTH, ARG_HEIGHT, ARG_PUBSCREEN, ARG_NOSECONDS, NUM_ARGS };

struct DosLibrary *DOSBase;
struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *GadToolsBase;
struct Device *TimerBase;

static struct Screen *scr;
static struct DrawInfo *dri;
static APTR vi;
static struct Menu *menus;
static struct Window *win;
static struct RastPort *rp;
static struct MsgPort *timerport;
static struct timerequest *tr;
static BOOL timeropen, pending;
static char errbuf[100];

static LONG optleft = -1, opttop = -1, optwidth = 150, optheight = 150;
static char *optpubscr;
static BOOL seconds = TRUE;

enum { MEN_SECONDS = 1, MEN_QUIT };

static struct NewMenu newmenu[] =
{
    { NM_TITLE, "Project", NULL, 0, 0, NULL },
    { NM_ITEM, "Seconds", "S", CHECKIT | MENUTOGGLE | CHECKED, 0, (APTR)MEN_SECONDS },
    { NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL },
    { NM_ITEM, "Quit", "Q", 0, 0, (APTR)MEN_QUIT },
    { NM_END, NULL, NULL, 0, 0, NULL }
};

/* sin(i * 6 degrees) * 1024, for a quarter turn */
static const WORD sintab[16] =
{
    0, 107, 213, 316, 416, 512, 602, 685, 761, 828, 887, 935, 974, 1002, 1018, 1024
};

/* sin of p sixtieths of a turn, * 1024 */
static WORD sin60(WORD p)
{
    p %= 60;
    switch (p / 15)
    {
        case 0: return sintab[p];
        case 1: return sintab[30 - p];
        case 2: return -sintab[p - 30];
        default: return -sintab[60 - p];
    }
}

/* The face: centre, radius; the digital line's baseline */
static WORD cx, cy, radius, textx, texty, textw;

/* The time shown: hand positions in sixtieths, and the digits */
static WORD shown[3] = { -1, -1, -1 };
static char digits[9];

enum { HOUR, MINUTE, SECOND };

/* A hand's length in 1/16 of the radius and its width */
static const UBYTE handlen[3] = { 8, 12, 13 };
static const UBYTE handwidth[3] = { 3, 2, 1 };

static void hand(WORD which, WORD pos, WORD pen)
{
    LONG len = (LONG)radius * handlen[which] / 16;
    WORD x = cx + (WORD)(len * sin60(pos) / 1024);
    WORD y = cy - (WORD)(len * sin60(pos + 15) / 1024);
    WORD dx = x > cx ? x - cx : cx - x, dy = y > cy ? y - cy : cy - y;
    WORD i, o;

    SetAPen(rp, pen);
    for (i = 0; i < handwidth[which]; i++)
    {
        /* Widen across the hand */
        o = i - handwidth[which] / 2;
        if (dx > dy)
        {
            Move(rp, cx, cy + o);
            Draw(rp, x, y + o);
        }
        else
        {
            Move(rp, cx + o, cy);
            Draw(rp, x + o, y);
        }
    }
}

static void hands(void)
{
    if (shown[HOUR] < 0)
        return;
    hand(HOUR, shown[HOUR], dri->dri_Pens[TEXTPEN]);
    hand(MINUTE, shown[MINUTE], dri->dri_Pens[TEXTPEN]);
    if (seconds)
        hand(SECOND, shown[SECOND], dri->dri_Pens[FILLPEN]);
}

static void layout(void)
{
    WORD w = win->Width - win->BorderLeft - win->BorderRight;
    WORD h = win->Height - win->BorderTop - win->BorderBottom;
    WORD th = rp->TxHeight + 4;

    cx = win->BorderLeft + w / 2;
    cy = win->BorderTop + (h - th) / 2;
    radius = ((w < h - th ? w : h - th) / 2) - 2;
    if (radius < 8)
        radius = 8;
    textw = TextLength(rp, "00:00:00", seconds ? 8 : 5);
    textx = cx - textw / 2;
    texty = win->BorderTop + h - 2 - rp->TxHeight + rp->TxBaseline;
}

static void face(void)
{
    WORD i, x, y, x2, y2, r1;

    SetAPen(rp, dri->dri_Pens[BACKGROUNDPEN]);
    RectFill(rp, win->BorderLeft, win->BorderTop,
             win->Width - win->BorderRight - 1, win->Height - win->BorderBottom - 1);
    SetAPen(rp, dri->dri_Pens[SHADOWPEN]);
    for (i = 0; i < 60; i++)
    {
        x = cx + (WORD)((LONG)radius * sin60(i) / 1024);
        y = cy - (WORD)((LONG)radius * sin60(i + 15) / 1024);
        if (i % 5)
        {
            WritePixel(rp, x, y);
            continue;
        }
        r1 = radius - radius / 8 - 1;
        x2 = cx + (WORD)((LONG)r1 * sin60(i) / 1024);
        y2 = cy - (WORD)((LONG)r1 * sin60(i + 15) / 1024);
        Move(rp, x, y);
        Draw(rp, x2, y2);
    }
    hands();
}

static void digital(void)
{
    SetABPenDrMd(rp, dri->dri_Pens[TEXTPEN], dri->dri_Pens[BACKGROUNDPEN], JAM2);
    Move(rp, textx, texty);
    Text(rp, digits, seconds ? 8 : 5);
    SetDrMd(rp, JAM1);
}

static void redraw(void)
{
    layout();
    face();
    digital();
}

/* The time now, and the hands and digits that changed */
static void tick(void)
{
    struct timeval tv;
    ULONG t;
    WORD now[3], i;
    BOOL moved = FALSE;

    GetSysTime(&tv);
    t = tv.tv_secs % 86400;
    now[SECOND] = t % 60;
    now[MINUTE] = t / 60 % 60;
    now[HOUR] = t / 3600 % 12 * 5 + now[MINUTE] / 12;

    for (i = HOUR; i <= SECOND; i++)
    {
        if (now[i] == shown[i] || (i == SECOND && !seconds))
            continue;
        if (shown[i] >= 0)
            hand(i, shown[i], dri->dri_Pens[BACKGROUNDPEN]);
        moved = TRUE;
    }
    for (i = HOUR; i <= SECOND; i++)
        shown[i] = now[i];
    if (moved)
        hands();

    digits[0] = '0' + t / 36000;
    digits[1] = '0' + t / 3600 % 10;
    digits[2] = ':';
    digits[3] = '0' + now[MINUTE] / 10;
    digits[4] = '0' + now[MINUTE] % 10;
    digits[5] = ':';
    digits[6] = '0' + now[SECOND] / 10;
    digits[7] = '0' + now[SECOND] % 10;
    digital();

    /* Wake at the next second, or the next minute */
    tr->tr_node.io_Command = TR_ADDREQUEST;
    tr->tr_time.tv_secs = seconds ? 0 : 59 - now[SECOND];
    tr->tr_time.tv_micro = 1000000 - tv.tv_micro;
    if (tr->tr_time.tv_micro == 1000000)
    {
        tr->tr_time.tv_secs++;
        tr->tr_time.tv_micro = 0;
    }
    SendIO((struct IORequest *)tr);
    pending = TRUE;
}

static void stoptimer(void)
{
    if (pending)
    {
        AbortIO((struct IORequest *)tr);
        WaitIO((struct IORequest *)tr);
        pending = FALSE;
    }
}

static void cleanup(char *msg)
{
    struct EasyStruct es;

    if (msg)
    {
        if (IntuitionBase && !((struct Process *)FindTask(NULL))->pr_CLI)
        {
            es.es_StructSize   = sizeof(es);
            es.es_Flags        = 0;
            es.es_Title        = "Clock";
            es.es_TextFormat   = msg;
            es.es_GadgetFormat = "OK";
            EasyRequestArgs(NULL, &es, NULL, NULL);
        }
        else
        {
            PutStr("Clock: ");
            PutStr(msg);
            PutStr("\n");
        }
    }
    if (timeropen)
    {
        stoptimer();
        CloseDevice((struct IORequest *)tr);
    }
    if (tr) DeleteIORequest((struct IORequest *)tr);
    if (timerport) DeleteMsgPort(timerport);
    if (win)
    {
        ClearMenuStrip(win);
        CloseWindow(win);
    }
    if (menus) FreeMenus(menus);
    if (vi) FreeVisualInfo(vi);
    if (dri) FreeScreenDrawInfo(scr, dri);
    if (scr) UnlockPubScreen(NULL, scr);
    if (optpubscr) FreeVec(optpubscr);
    if (GadToolsBase) CloseLibrary(GadToolsBase);
    if (GfxBase) CloseLibrary((struct Library *)GfxBase);
    if (IntuitionBase) CloseLibrary((struct Library *)IntuitionBase);
}

static BOOL get_arguments(void)
{
    struct RDArgs *rdargs;
    IPTR args[NUM_ARGS];
    char *s;
    WORD i, n;

    for (i = 0; i < NUM_ARGS; i++) args[i] = 0;
    if (!(rdargs = ReadArgs(ARG_TEMPLATE, (LONG *)args, NULL)))
    {
        Fault(IoErr(), NULL, errbuf, sizeof(errbuf));
        return FALSE;
    }
    if (args[ARG_LEFT])   optleft   = *(LONG *)args[ARG_LEFT];
    if (args[ARG_TOP])    opttop    = *(LONG *)args[ARG_TOP];
    if (args[ARG_WIDTH])  optwidth  = *(LONG *)args[ARG_WIDTH];
    if (args[ARG_HEIGHT]) optheight = *(LONG *)args[ARG_HEIGHT];
    if (args[ARG_NOSECONDS]) seconds = FALSE;
    if ((s = (char *)args[ARG_PUBSCREEN]))
    {
        for (n = 0; s[n]; n++)
            ;
        if ((optpubscr = AllocVec(n + 1, MEMF_ANY)))
            for (i = 0; i <= n; i++)
                optpubscr[i] = s[i];
    }
    FreeArgs(rdargs);
    return TRUE;
}

static char *setup(void)
{
    struct TagItem t[17];
    WORD w, h;

    if (!(IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 39)) ||
        !(GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 39)) ||
        !(GadToolsBase = OpenLibrary("gadtools.library", 39)))
        return "Can't open gadtools.library";
    if (!(timerport = CreateMsgPort()) ||
        !(tr = (struct timerequest *)CreateIORequest(timerport, sizeof(*tr))) ||
        OpenDevice(TIMERNAME, UNIT_VBLANK, (struct IORequest *)tr, 0))
        return "Can't open timer.device";
    timeropen = TRUE;
    TimerBase = tr->tr_node.io_Device;

    if (!((optpubscr && (scr = LockPubScreen(optpubscr))) || (scr = LockPubScreen(NULL))) ||
        !(dri = GetScreenDrawInfo(scr)) || !(vi = GetVisualInfoA(scr, NULL)))
        return "Can't lock the screen";
    if (!seconds)
        newmenu[1].nm_Flags &= ~CHECKED;
    if (!(menus = CreateMenusA(newmenu, NULL)) || !LayoutMenusA(menus, vi, NULL))
        return "Can't create the menus";

    w = optwidth;
    h = optheight;
    t[0].ti_Tag = WA_Title;         t[0].ti_Data = (IPTR)"Clock";
    t[1].ti_Tag = WA_PubScreen;     t[1].ti_Data = (IPTR)scr;
    t[2].ti_Tag = WA_Width;         t[2].ti_Data = w;
    t[3].ti_Tag = WA_Height;        t[3].ti_Data = h;
    t[4].ti_Tag = WA_Left;          t[4].ti_Data = optleft >= 0 ? optleft : (scr->Width - w) / 2;
    t[5].ti_Tag = WA_Top;           t[5].ti_Data = opttop >= 0 ? opttop : (scr->Height - h) / 2;
    t[6].ti_Tag = WA_IDCMP;         t[6].ti_Data = IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW |
                                                   IDCMP_NEWSIZE | IDCMP_MENUPICK;
    t[7].ti_Tag = WA_DragBar;       t[7].ti_Data = TRUE;
    t[8].ti_Tag = WA_DepthGadget;   t[8].ti_Data = TRUE;
    t[9].ti_Tag = WA_CloseGadget;   t[9].ti_Data = TRUE;
    t[10].ti_Tag = WA_SizeGadget;   t[10].ti_Data = TRUE;
    t[11].ti_Tag = WA_SimpleRefresh; t[11].ti_Data = TRUE;
    t[12].ti_Tag = WA_MinWidth;     t[12].ti_Data = 80;
    t[13].ti_Tag = WA_MinHeight;    t[13].ti_Data = 80;
    t[14].ti_Tag = WA_MaxWidth;     t[14].ti_Data = (IPTR)~0;
    t[15].ti_Tag = WA_MaxHeight;    t[15].ti_Data = (IPTR)~0;
    t[16].ti_Tag = TAG_DONE;
    if (!(win = OpenWindowTagList(NULL, t)))
        return "Can't open the window";
    SetMenuStrip(win, menus);
    rp = win->RPort;
    SetFont(rp, dri->dri_Font);
    return NULL;
}

static void handle_all(void)
{
    struct IntuiMessage *msg;
    struct MenuItem *item;
    ULONG sigs, class;
    UWORD code;

    layout();
    face();
    tick();
    for (;;)
    {
        sigs = Wait((1L << win->UserPort->mp_SigBit) | (1L << timerport->mp_SigBit) |
                    SIGBREAKF_CTRL_C);
        if (sigs & SIGBREAKF_CTRL_C)
            return;
        if (pending && CheckIO((struct IORequest *)tr))
        {
            WaitIO((struct IORequest *)tr);
            pending = FALSE;
            tick();
        }
        while ((msg = GT_GetIMsg(win->UserPort)))
        {
            class = msg->Class;
            code = msg->Code;
            GT_ReplyIMsg(msg);
            switch (class)
            {
                case IDCMP_CLOSEWINDOW:
                    return;
                case IDCMP_NEWSIZE:
                    redraw();
                    break;
                case IDCMP_REFRESHWINDOW:
                    GT_BeginRefresh(win);
                    redraw();
                    GT_EndRefresh(win, TRUE);
                    break;
                case IDCMP_MENUPICK:
                    while (code != MENUNULL)
                    {
                        item = ItemAddress(menus, code);
                        if ((IPTR)GTMENUITEM_USERDATA(item) == MEN_QUIT)
                            return;
                        if ((IPTR)GTMENUITEM_USERDATA(item) == MEN_SECONDS)
                        {
                            seconds = (item->Flags & CHECKED) != 0;
                            stoptimer();
                            redraw();
                            tick();
                        }
                        code = item->NextSelect;
                    }
                    break;
            }
        }
    }
}

int main(void)
{
    struct Process *me = (struct Process *)FindTask(NULL);
    struct WBStartup *startup = NULL;
    char *err = NULL;

    if (!me->pr_CLI)
    {
        WaitPort(&me->pr_MsgPort);
        startup = (struct WBStartup *)GetMsg(&me->pr_MsgPort);
    }
    if ((DOSBase = (struct DosLibrary *)OpenLibrary(DOSNAME, 36)))
    {
        if (!startup && !get_arguments())
            err = errbuf;
        else if (!(err = setup()))
            handle_all();
        cleanup(err);
        CloseLibrary((struct Library *)DOSBase);
    }
    if (startup)
    {
        Forbid();
        ReplyMsg((struct Message *)startup);
    }
    return err ? RETURN_FAIL : RETURN_OK;
}
