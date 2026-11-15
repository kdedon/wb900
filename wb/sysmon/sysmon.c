/* SPDX-License-Identifier: MIT */
/*
 * SysMon: free memory by type and the tasks and processes with their
 * priority, state and stack use, after AROS's SysMon (APL), on intuition
 * and gadtools.  It samples once a second and redraws only the rows and
 * numbers that changed.  Stack use is the stack's top less the first
 * nonzero byte above the guard word, as C:StackUse counts it.
 */
#include <exec/exec.h>
#include <exec/execbase.h>
#include <exec/tasks.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <devices/timer.h>
#include <graphics/gfxbase.h>
#include <intuition/intuition.h>
#include <libraries/gadtools.h>
#include <workbench/startup.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/gadtools.h>

const char *version = "$VER: SysMon 1.0 (15.11.2026)";

struct DosLibrary *DOSBase;
struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *GadToolsBase;

#define MAXTASKS    48
#define NAMELEN     16
/* name, priority, state, stack used and size */
#define ROWLEN      (NAMELEN + 5 + 6 + 7 + 7)
#define LISTROWS    14

static const char header[] = "Name              Pri State   Used   Size";

struct Row
{
    struct Node node;
    struct Task *task;
    char text[ROWLEN + 1];
};

struct Sample
{
    struct Task *task;
    char name[NAMELEN + 1];
    BYTE pri;
    UBYTE state;
    UBYTE *lower, *upper;
};

static struct Row rows[MAXTASKS];
static struct Sample samples[MAXTASKS];
static WORD nrows, nsamples, selected = -1;
static struct List list;

/* The memory types and their numbers: free, largest, total, in KB */
#define NMEM 3
static const ULONG memtypes[NMEM] = { MEMF_CHIP, MEMF_FAST, MEMF_STACK };
static const char *memnames[NMEM] = { "Chip", "Fast", "Stack" };
static const char *memcols[3] = { "Free", "Largest", "Total" };
static LONG memshown[NMEM][3];

static struct Screen *scr;
static APTR vi;
static struct Gadget *glist, *lv, *summary, *memgad[NMEM][3];
static struct Window *win;
static struct TextAttr ta;
static struct MsgPort *timerport;
static struct timerequest *tr;
static BOOL timeropen, pending;
static char summarytext[48];

/* n right-aligned in w characters at p */
static void putnum(char *p, LONG n, WORD w)
{
    BOOL neg = n < 0;

    if (neg) n = -n;
    do
    {
        p[--w] = '0' + n % 10;
        n /= 10;
    } while (n && w > 0);
    if (neg && w > 0)
        p[--w] = '-';
    while (w > 0)
        p[--w] = ' ';
}

static void putstr(char *p, const char *s, WORD w)
{
    while (w > 0 && *s)
    {
        *p++ = *s++;
        w--;
    }
    while (w-- > 0)
        *p++ = ' ';
}

static int compare(struct Sample *a, struct Sample *b)
{
    char *p = a->name, *q = b->name;

    for (; *p && *p == *q; p++, q++)
        ;
    if (*p != *q)
        return (UBYTE)*p < (UBYTE)*q ? -1 : 1;
    return a->task < b->task ? -1 : a->task > b->task;
}

static void add_sample(struct Task *t)
{
    struct Sample *s = &samples[nsamples];
    char *n = t->tc_Node.ln_Name;
    WORD i;

    if (nsamples == MAXTASKS)
        return;
    s->task = t;
    for (i = 0; n && n[i] && i < NAMELEN; i++)
        s->name[i] = n[i];
    s->name[i] = 0;
    s->pri = t->tc_Node.ln_Pri;
    s->state = t->tc_State;
    s->lower = (UBYTE *)t->tc_SPLower;
    s->upper = (UBYTE *)t->tc_SPUpper;
    nsamples++;
}

/* The tasks, by name; counts of the ready and waiting ones */
static void sample_tasks(WORD *ready, WORD *waiting)
{
    struct Task *me = FindTask(NULL), *t;
    struct Sample tmp;
    WORD i, j;

    nsamples = 0;
    *ready = *waiting = 0;
    Disable();
    add_sample(me);
    for (t = (struct Task *)SysBase->TaskReady.lh_Head; t->tc_Node.ln_Succ;
         t = (struct Task *)t->tc_Node.ln_Succ, (*ready)++)
        add_sample(t);
    for (t = (struct Task *)SysBase->TaskWait.lh_Head; t->tc_Node.ln_Succ;
         t = (struct Task *)t->tc_Node.ln_Succ, (*waiting)++)
        add_sample(t);
    Enable();

    for (i = 1; i < nsamples; i++)
    {
        tmp = samples[i];
        for (j = i; j > 0 && compare(&samples[j - 1], &tmp) > 0; j--)
            samples[j] = samples[j - 1];
        samples[j] = tmp;
    }
}

/* Under Forbid, so that the task and its stack stay */
static LONG stack_used(struct Sample *s)
{
    UBYTE *p = s->lower + 8;

    while (p < s->upper && *p == 0)
        p++;
    return s->upper - p;
}

static void format_row(struct Sample *s, LONG used, char *p)
{
    static const char *states[] = { "inval", "added", "run", "ready", "wait", "excep", "remov" };

    putstr(p, s->name, NAMELEN);
    putnum(p + NAMELEN, s->pri, 5);
    p[NAMELEN + 5] = ' ';
    putstr(p + NAMELEN + 6, s->state <= TS_REMOVED ? states[s->state] : "?", 5);
    putnum(p + NAMELEN + 11, used, 7);
    putnum(p + NAMELEN + 18, s->upper - s->lower, 7);
    p[ROWLEN] = 0;
}

static void set_tags(struct Gadget *g, Tag t1, IPTR d1, Tag t2, IPTR d2)
{
    struct TagItem t[3];

    t[0].ti_Tag = t1;  t[0].ti_Data = d1;
    t[1].ti_Tag = t2;  t[1].ti_Data = d2;
    t[2].ti_Tag = TAG_DONE;
    GT_SetGadgetAttrsA(g, win, NULL, t);
}

/*
 * The list view redraws the old and the new selected row when the
 * selection moves; with nothing shown as selected, that redraws one row.
 */
static void redraw_row(WORD i)
{
    set_tags(lv, GTLV_Selected, i, TAG_DONE, 0);
    selected = i;
}

static void update_tasks(void)
{
    char text[ROWLEN + 1];
    struct TagItem t[2];
    LONG top = 0;
    WORD ready, waiting, i, j;
    BOOL same;

    Forbid();
    sample_tasks(&ready, &waiting);
    same = nsamples == nrows;
    for (i = 0; same && i < nrows; i++)
        same = rows[i].task == samples[i].task;
    if (same)
    {
        for (i = 0; i < nrows; i++)
        {
            format_row(&samples[i], stack_used(&samples[i]), text);
            for (j = 0; j <= ROWLEN && text[j] == rows[i].text[j]; j++)
                ;
            if (j <= ROWLEN)
            {
                /* Under Forbid, as input.device's task may be drawing it */
                CopyMem(text, rows[i].text, ROWLEN + 1);
                Permit();
                redraw_row(i);
                Forbid();
            }
        }
        Permit();
        if (selected >= 0)
            redraw_row(-1);
    }
    else
    {
        Permit();
        t[0].ti_Tag = GTLV_Top;  t[0].ti_Data = (IPTR)&top;
        t[1].ti_Tag = TAG_DONE;
        GT_GetGadgetAttrsA(lv, win, NULL, t);
        set_tags(lv, GTLV_Labels, ~0, TAG_DONE, 0);
        NewList(&list);
        Forbid();
        for (i = 0; i < nsamples; i++)
        {
            rows[i].task = samples[i].task;
            rows[i].node.ln_Name = rows[i].text;
            format_row(&samples[i], stack_used(&samples[i]), rows[i].text);
            AddTail(&list, &rows[i].node);
        }
        Permit();
        nrows = nsamples;
        set_tags(lv, GTLV_Labels, (IPTR)&list, GTLV_Top, top);
    }

    putnum(summarytext, nrows, 3);
    putstr(summarytext + 3, " tasks:", 7);
    putnum(summarytext + 10, ready, 3);
    putstr(summarytext + 13, " ready,", 7);
    putnum(summarytext + 20, waiting, 3);
    putstr(summarytext + 23, " waiting", 9);
    set_tags(summary, GTTX_Text, (IPTR)summarytext, TAG_DONE, 0);
}

static void update_memory(void)
{
    static const ULONG flags[3] = { 0, MEMF_LARGEST, MEMF_TOTAL };
    LONG kb;
    WORD i, j;

    for (i = 0; i < NMEM; i++)
        for (j = 0; j < 3; j++)
        {
            kb = AvailMem(memtypes[i] | flags[j]) / 1024;
            if (kb != memshown[i][j])
            {
                memshown[i][j] = kb;
                set_tags(memgad[i][j], GTNM_Number, kb, TAG_DONE, 0);
            }
        }
}

static void sample(void)
{
    update_memory();
    update_tasks();
    tr->tr_node.io_Command = TR_ADDREQUEST;
    tr->tr_time.tv_secs = 1;
    tr->tr_time.tv_micro = 0;
    SendIO((struct IORequest *)tr);
    pending = TRUE;
}

static struct Gadget *make_gadgets(void)
{
    static struct TagItem numtags[] = {
        { GTNM_Number, -1 }, { GTNM_Border, TRUE }, { GTNM_Format, (IPTR)"%ld K" },
        { GTNM_Justification, GTJ_RIGHT }, { TAG_DONE, 0 } };
    static struct TagItem texttags[] = {
        { GTTX_Text, 0 }, { GTTX_Border, TRUE }, { TAG_DONE, 0 } };
    static struct TagItem coltags[] = {
        { GTTX_Text, 0 }, { GTTX_Justification, GTJ_CENTER }, { TAG_DONE, 0 } };
    static struct TagItem headtags[] = {
        { GTTX_Text, (IPTR)header }, { TAG_DONE, 0 } };
    static struct TagItem lvtags[] = {
        { GTLV_Labels, 0 }, { GTLV_ReadOnly, TRUE }, { TAG_DONE, 0 } };
    struct TextFont *font = GfxBase->DefaultFont;
    struct NewGadget ng;
    struct Gadget *g;
    WORD fw = font->tf_XSize, fh = font->tf_YSize;
    WORD x0, y, nw, i, j;

    ta.ta_Name = font->tf_Message.mn_Node.ln_Name;
    ta.ta_YSize = fh;
    ta.ta_Style = 0;
    ta.ta_Flags = 0;
    x0 = scr->WBorLeft + 6;
    y = scr->WBorTop + scr->Font->ta_YSize + 1 + 4;
    nw = 11 * fw + 8;

    g = CreateContext(&glist);
    ng.ng_TextAttr = &ta;
    ng.ng_VisualInfo = vi;
    ng.ng_UserData = NULL;
    ng.ng_GadgetText = NULL;
    ng.ng_Flags = 0;
    ng.ng_GadgetID = 4;
    ng.ng_Height = fh;
    ng.ng_Width = nw;
    for (j = 0; j < 3; j++)
    {
        ng.ng_LeftEdge = x0 + 6 * fw + 8 + j * (nw + 4);
        ng.ng_TopEdge = y;
        coltags[0].ti_Data = (IPTR)memcols[j];
        g = CreateGadgetA(TEXT_KIND, g, &ng, coltags);
    }
    y += fh + 2;
    ng.ng_Height = fh + 4;
    for (i = 0; i < NMEM; i++)
    {
        for (j = 0; j < 3; j++)
        {
            ng.ng_LeftEdge = x0 + 6 * fw + 8 + j * (nw + 4);
            ng.ng_TopEdge = y;
            ng.ng_GadgetText = (UBYTE *)(j == 0 ? memnames[i] : NULL);
            ng.ng_Flags = PLACETEXT_LEFT;
            ng.ng_GadgetID = 10 + i * 3 + j;
            g = memgad[i][j] = CreateGadgetA(NUMBER_KIND, g, &ng, numtags);
            memshown[i][j] = -1;
        }
        y += fh + 6;
    }
    ng.ng_LeftEdge = x0 + 6 * fw + 8;
    ng.ng_TopEdge = y + 2;
    ng.ng_Width = 3 * nw + 8;
    ng.ng_GadgetText = (UBYTE *)"Tasks";
    ng.ng_Flags = PLACETEXT_LEFT;
    ng.ng_GadgetID = 2;
    g = summary = CreateGadgetA(TEXT_KIND, g, &ng, texttags);
    y += fh + 10;

    ng.ng_LeftEdge = x0 + 4;
    ng.ng_TopEdge = y;
    ng.ng_Width = ROWLEN * fw;
    ng.ng_Height = fh;
    ng.ng_GadgetText = NULL;
    ng.ng_GadgetID = 3;
    g = CreateGadgetA(TEXT_KIND, g, &ng, headtags);
    y += fh + 2;

    ng.ng_LeftEdge = x0;
    ng.ng_TopEdge = y;
    ng.ng_Width = ROWLEN * fw + 8 + 20;
    ng.ng_Height = LISTROWS * fh + 6;
    ng.ng_GadgetID = 1;
    g = lv = CreateGadgetA(LISTVIEW_KIND, g, &ng, lvtags);
    return g;
}

static char *setup(void)
{
    struct TagItem t[12];
    struct Gadget *g;
    WORD w = 0, h = 0;

    if (!(IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 39)) ||
        !(GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 39)) ||
        !(GadToolsBase = OpenLibrary("gadtools.library", 39)))
        return "Can't open gadtools.library";
    if (!(timerport = CreateMsgPort()) ||
        !(tr = (struct timerequest *)CreateIORequest(timerport, sizeof(*tr))) ||
        OpenDevice(TIMERNAME, UNIT_VBLANK, (struct IORequest *)tr, 0))
        return "Can't open timer.device";
    timeropen = TRUE;
    if (!(scr = LockPubScreen(NULL)) || !(vi = GetVisualInfoA(scr, NULL)))
        return "Can't lock the screen";
    NewList(&list);
    if (!make_gadgets())
        return "Can't create the gadgets";

    for (g = glist; g; g = g->NextGadget)
    {
        if (g->LeftEdge + g->Width > w) w = g->LeftEdge + g->Width;
        if (g->TopEdge + g->Height > h) h = g->TopEdge + g->Height;
    }
    w += 6 + scr->WBorRight;
    h += 4 + scr->WBorBottom;
    t[0].ti_Tag = WA_Title;         t[0].ti_Data = (IPTR)"SysMon";
    t[1].ti_Tag = WA_PubScreen;     t[1].ti_Data = (IPTR)scr;
    t[2].ti_Tag = WA_Width;         t[2].ti_Data = w;
    t[3].ti_Tag = WA_Height;        t[3].ti_Data = h;
    t[4].ti_Tag = WA_Left;          t[4].ti_Data = w < scr->Width ? (scr->Width - w) / 2 : 0;
    t[5].ti_Tag = WA_Top;           t[5].ti_Data = h < scr->Height ? (scr->Height - h) / 3 : 0;
    t[6].ti_Tag = WA_Gadgets;       t[6].ti_Data = (IPTR)glist;
    t[7].ti_Tag = WA_IDCMP;         t[7].ti_Data = IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW |
                                                   LISTVIEWIDCMP;
    t[8].ti_Tag = WA_DragBar;       t[8].ti_Data = TRUE;
    t[9].ti_Tag = WA_DepthGadget;   t[9].ti_Data = TRUE;
    t[10].ti_Tag = WA_CloseGadget;  t[10].ti_Data = TRUE;
    t[11].ti_Tag = TAG_DONE;
    if (!(win = OpenWindowTagList(NULL, t)))
        return "Can't open the window";
    GT_RefreshWindow(win, NULL);
    return NULL;
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
            es.es_Title        = "SysMon";
            es.es_TextFormat   = msg;
            es.es_GadgetFormat = "OK";
            EasyRequestArgs(NULL, &es, NULL, NULL);
        }
        else
        {
            PutStr("SysMon: ");
            PutStr(msg);
            PutStr("\n");
        }
    }
    if (timeropen)
    {
        if (pending)
        {
            AbortIO((struct IORequest *)tr);
            WaitIO((struct IORequest *)tr);
        }
        CloseDevice((struct IORequest *)tr);
    }
    if (tr) DeleteIORequest((struct IORequest *)tr);
    if (timerport) DeleteMsgPort(timerport);
    if (win) CloseWindow(win);
    if (glist) FreeGadgets(glist);
    if (vi) FreeVisualInfo(vi);
    if (scr) UnlockPubScreen(NULL, scr);
    if (GadToolsBase) CloseLibrary(GadToolsBase);
    if (GfxBase) CloseLibrary((struct Library *)GfxBase);
    if (IntuitionBase) CloseLibrary((struct Library *)IntuitionBase);
}

static void handle_all(void)
{
    struct IntuiMessage *msg;
    ULONG sigs, class;

    sample();
    for (;;)
    {
        sigs = Wait((1L << win->UserPort->mp_SigBit) | (1L << timerport->mp_SigBit) |
                    SIGBREAKF_CTRL_C);
        if (sigs & SIGBREAKF_CTRL_C)
            return;
        if (CheckIO((struct IORequest *)tr))
        {
            WaitIO((struct IORequest *)tr);
            pending = FALSE;
            sample();
        }
        while ((msg = GT_GetIMsg(win->UserPort)))
        {
            class = msg->Class;
            GT_ReplyIMsg(msg);
            if (class == IDCMP_CLOSEWINDOW)
                return;
            if (class == IDCMP_REFRESHWINDOW)
            {
                GT_BeginRefresh(win);
                GT_EndRefresh(win, TRUE);
            }
        }
    }
}

int main(void)
{
    struct Process *me = (struct Process *)FindTask(NULL);
    struct WBStartup *startup = NULL;
    char *err;

    if (!me->pr_CLI)
    {
        WaitPort(&me->pr_MsgPort);
        startup = (struct WBStartup *)GetMsg(&me->pr_MsgPort);
    }
    if (!(DOSBase = (struct DosLibrary *)OpenLibrary(DOSNAME, 36)))
        err = "";
    else
    {
        if (!(err = setup()))
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
