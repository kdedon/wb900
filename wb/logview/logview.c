/* SPDX-License-Identifier: MIT */
/*
 * LogView: the end of a text log in a list, after AROS's LogView (APL),
 * on intuition and gadtools.  Refresh reads the file again and shows its
 * last line.  The log is the FILE argument, the project Workbench opens
 * it with, or the path typed in the window.
 *
 * Usage: LogView [FILE] [PUBSCREEN name]
 */
#include <exec/exec.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <graphics/gfxbase.h>
#include <intuition/intuition.h>
#include <libraries/gadtools.h>
#include <workbench/startup.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/gadtools.h>

const char *version = "$VER: LogView 1.0 (15.11.2026)";

#define ARG_TEMPLATE "FILE,PUBSCREEN/K"
enum { ARG_FILE, ARG_PUBSCREEN, NUM_ARGS };

struct DosLibrary *DOSBase;
struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *GadToolsBase;

/* The end of the file shown, and its lines; each fits one segment */
#define MAXBYTES    32768L
#define MAXLINES    2048
#define COLUMNS     72
#define ROWS        20
#define PATHLEN     256

enum { GAD_LIST = 1, GAD_PATH, GAD_REFRESH };

static struct Screen *scr;
static APTR vi;
static struct Gadget *glist, *lv, *pathgad;
static struct Window *win;
static struct TextAttr ta;
static struct List list;
static struct Node *nodes;
static char *text;
static char path[PATHLEN], pubscr[64];
static char errbuf[100];

static void set_tags(struct Gadget *g, Tag t1, IPTR d1, Tag t2, IPTR d2)
{
    struct TagItem t[3];

    t[0].ti_Tag = t1;  t[0].ti_Data = d1;
    t[1].ti_Tag = t2;  t[1].ti_Data = d2;
    t[2].ti_Tag = TAG_DONE;
    GT_SetGadgetAttrsA(g, win, NULL, t);
}

static void copy(char *d, const char *s, WORD n)
{
    while (--n > 0 && *s)
        *d++ = *s++;
    *d = 0;
}

static void show_error(const char *msg)
{
    struct EasyStruct es;

    es.es_StructSize   = sizeof(es);
    es.es_Flags        = 0;
    es.es_Title        = "LogView";
    es.es_TextFormat   = "%s";
    es.es_GadgetFormat = "OK";
    EasyRequestArgs(win, &es, NULL, (APTR)&msg);
}

/* The file's last MAXBYTES and at most MAXLINES lines; the list ends on
 * its last line */
static void load(void)
{
    BPTR fh;
    LONG len, start, n, i, lines;
    char *p, *end;

    set_tags(lv, GTLV_Labels, ~0, TAG_DONE, 0);
    NewList(&list);
    if (text) FreeVec(text);
    if (nodes) FreeVec(nodes);
    text = NULL;
    nodes = NULL;
    if (!path[0])
    {
        set_tags(lv, GTLV_Labels, (IPTR)&list, TAG_DONE, 0);
        return;
    }

    if (!(fh = Open(path, MODE_OLDFILE)))
    {
        Fault(IoErr(), path, errbuf, sizeof(errbuf));
        show_error(errbuf);
        set_tags(lv, GTLV_Labels, (IPTR)&list, TAG_DONE, 0);
        return;
    }
    Seek(fh, 0, OFFSET_END);
    len = Seek(fh, 0, OFFSET_BEGINNING);
    start = len > MAXBYTES ? len - MAXBYTES : 0;
    if (len < 0 || Seek(fh, start, OFFSET_BEGINNING) < 0)
        start = len = 0;
    if ((text = AllocVec(len - start + 1, MEMF_ANY)))
        n = Read(fh, text, len - start);
    Close(fh);
    if (!text || n < 0)
    {
        set_tags(lv, GTLV_Labels, (IPTR)&list, TAG_DONE, 0);
        return;
    }
    p = text;
    end = text + n;
    *end = 0;
    /* A cut start drops its partial line */
    if (start > 0)
        while (p < end && *p++ != '\n')
            ;

    for (lines = 0, i = 0; p + i < end; i++)
        if (p[i] == '\n')
            lines++;
    if (p < end && end[-1] != '\n')
        lines++;
    /* The last MAXLINES */
    for (; lines > MAXLINES; lines--)
        while (*p++ != '\n')
            ;
    if (lines > 0 && (nodes = AllocVec(lines * sizeof(struct Node), MEMF_CLEAR)))
    {
        for (i = 0; i < lines; i++)
        {
            nodes[i].ln_Name = p;
            for (; p < end && *p != '\n'; p++)
                if ((UBYTE)*p < ' ')
                    *p = ' ';
            *p++ = 0;
            AddTail(&list, &nodes[i]);
        }
    }
    set_tags(lv, GTLV_Labels, (IPTR)&list, GTLV_MakeVisible, lines > 0 ? lines - 1 : 0);
}

static struct Gadget *make_gadgets(void)
{
    static struct TagItem strtags[] = {
        { GTST_String, 0 }, { GTST_MaxChars, PATHLEN - 1 }, { TAG_DONE, 0 } };
    static struct TagItem buttags[] = { { GT_Underscore, '_' }, { TAG_DONE, 0 } };
    static struct TagItem lvtags[] = {
        { GTLV_Labels, 0 }, { GTLV_ReadOnly, TRUE }, { TAG_DONE, 0 } };
    struct TextFont *font = GfxBase->DefaultFont;
    struct NewGadget ng;
    struct Gadget *g;
    WORD fw = font->tf_XSize, fh = font->tf_YSize;
    WORD x0, y0, lw, bw;

    ta.ta_Name = font->tf_Message.mn_Node.ln_Name;
    ta.ta_YSize = fh;
    ta.ta_Style = 0;
    ta.ta_Flags = 0;
    x0 = scr->WBorLeft + 6;
    y0 = scr->WBorTop + scr->Font->ta_YSize + 1 + 4;
    lw = COLUMNS * fw + 8 + 20;
    bw = 9 * fw + 8;

    g = CreateContext(&glist);
    ng.ng_TextAttr = &ta;
    ng.ng_VisualInfo = vi;
    ng.ng_UserData = NULL;

    ng.ng_LeftEdge = x0 + 5 * fw + 8;
    ng.ng_TopEdge = y0;
    ng.ng_Width = lw - 5 * fw - 8 - bw - 6;
    ng.ng_Height = fh + 6;
    ng.ng_GadgetText = (UBYTE *)"File";
    ng.ng_Flags = PLACETEXT_LEFT;
    ng.ng_GadgetID = GAD_PATH;
    strtags[0].ti_Data = (IPTR)path;
    g = pathgad = CreateGadgetA(STRING_KIND, g, &ng, strtags);

    ng.ng_LeftEdge = x0 + lw - bw;
    ng.ng_Width = bw;
    ng.ng_GadgetText = (UBYTE *)"_Refresh";
    ng.ng_Flags = PLACETEXT_IN;
    ng.ng_GadgetID = GAD_REFRESH;
    g = CreateGadgetA(BUTTON_KIND, g, &ng, buttags);

    ng.ng_LeftEdge = x0;
    ng.ng_TopEdge = y0 + fh + 6 + 4;
    ng.ng_Width = lw;
    ng.ng_Height = ROWS * fh + 6;
    ng.ng_GadgetText = NULL;
    ng.ng_GadgetID = GAD_LIST;
    g = lv = CreateGadgetA(LISTVIEW_KIND, g, &ng, lvtags);
    return g;
}

static char *setup(void)
{
    struct TagItem t[13];
    struct Gadget *g;
    WORD w = 0, h = 0;

    if (!(IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 39)) ||
        !(GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 39)) ||
        !(GadToolsBase = OpenLibrary("gadtools.library", 39)))
        return "Can't open gadtools.library";
    if (!((pubscr[0] && (scr = LockPubScreen(pubscr))) || (scr = LockPubScreen(NULL))) ||
        !(vi = GetVisualInfoA(scr, NULL)))
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
    t[0].ti_Tag = WA_Title;         t[0].ti_Data = (IPTR)"LogView";
    t[1].ti_Tag = WA_PubScreen;     t[1].ti_Data = (IPTR)scr;
    t[2].ti_Tag = WA_Width;         t[2].ti_Data = w;
    t[3].ti_Tag = WA_Height;        t[3].ti_Data = h;
    t[4].ti_Tag = WA_Left;          t[4].ti_Data = w < scr->Width ? (scr->Width - w) / 2 : 0;
    t[5].ti_Tag = WA_Top;           t[5].ti_Data = h < scr->Height ? (scr->Height - h) / 3 : 0;
    t[6].ti_Tag = WA_Gadgets;       t[6].ti_Data = (IPTR)glist;
    t[7].ti_Tag = WA_IDCMP;         t[7].ti_Data = IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW |
                                                   IDCMP_VANILLAKEY | BUTTONIDCMP |
                                                   STRINGIDCMP | LISTVIEWIDCMP;
    t[8].ti_Tag = WA_DragBar;       t[8].ti_Data = TRUE;
    t[9].ti_Tag = WA_DepthGadget;   t[9].ti_Data = TRUE;
    t[10].ti_Tag = WA_CloseGadget;  t[10].ti_Data = TRUE;
    t[11].ti_Tag = WA_Activate;     t[11].ti_Data = TRUE;
    t[12].ti_Tag = TAG_DONE;
    if (!(win = OpenWindowTagList(NULL, t)))
        return "Can't open the window";
    GT_RefreshWindow(win, NULL);
    return NULL;
}

static void cleanup(char *msg)
{
    if (msg)
    {
        if (IntuitionBase && !((struct Process *)FindTask(NULL))->pr_CLI)
            show_error(msg);
        else
        {
            PutStr("LogView: ");
            PutStr(msg);
            PutStr("\n");
        }
    }
    if (win) CloseWindow(win);
    if (glist) FreeGadgets(glist);
    if (vi) FreeVisualInfo(vi);
    if (scr) UnlockPubScreen(NULL, scr);
    if (nodes) FreeVec(nodes);
    if (text) FreeVec(text);
    if (GadToolsBase) CloseLibrary(GadToolsBase);
    if (GfxBase) CloseLibrary((struct Library *)GfxBase);
    if (IntuitionBase) CloseLibrary((struct Library *)IntuitionBase);
}

static void handle_all(void)
{
    struct IntuiMessage *msg;
    struct TagItem t[2];
    STRPTR s;
    ULONG sigs, class;
    UWORD code, id;

    load();
    for (;;)
    {
        sigs = Wait((1L << win->UserPort->mp_SigBit) | SIGBREAKF_CTRL_C);
        if (sigs & SIGBREAKF_CTRL_C)
            return;
        while ((msg = GT_GetIMsg(win->UserPort)))
        {
            class = msg->Class;
            code = msg->Code;
            id = class == IDCMP_GADGETUP ? ((struct Gadget *)msg->IAddress)->GadgetID : 0;
            GT_ReplyIMsg(msg);
            switch (class)
            {
                case IDCMP_CLOSEWINDOW:
                    return;
                case IDCMP_REFRESHWINDOW:
                    GT_BeginRefresh(win);
                    GT_EndRefresh(win, TRUE);
                    break;
                case IDCMP_VANILLAKEY:
                    if (code == 'r' || code == 'R')
                        load();
                    else if (code == 27)
                        return;
                    break;
                case IDCMP_GADGETUP:
                    if (id == GAD_PATH)
                    {
                        t[0].ti_Tag = GTST_String;  t[0].ti_Data = (IPTR)&s;
                        t[1].ti_Tag = TAG_DONE;
                        GT_GetGadgetAttrsA(pathgad, win, NULL, t);
                        copy(path, s, PATHLEN);
                        load();
                    }
                    else if (id == GAD_REFRESH)
                        load();
                    break;
            }
        }
    }
}

static BOOL get_arguments(struct WBStartup *startup)
{
    struct RDArgs *rdargs;
    IPTR args[NUM_ARGS];
    BPTR lock;

    if (startup)
    {
        /* A project's full path, as Refresh opens it again */
        if (startup->sm_NumArgs > 1 && startup->sm_ArgList[1].wa_Name)
        {
            lock = startup->sm_ArgList[1].wa_Lock;
            if (!lock || !NameFromLock(lock, path, PATHLEN) ||
                !AddPart(path, startup->sm_ArgList[1].wa_Name, PATHLEN))
                copy(path, startup->sm_ArgList[1].wa_Name, PATHLEN);
        }
        return TRUE;
    }
    args[ARG_FILE] = args[ARG_PUBSCREEN] = 0;
    if (!(rdargs = ReadArgs(ARG_TEMPLATE, (LONG *)args, NULL)))
    {
        Fault(IoErr(), NULL, errbuf, sizeof(errbuf));
        return FALSE;
    }
    if (args[ARG_FILE])
        copy(path, (char *)args[ARG_FILE], PATHLEN);
    if (args[ARG_PUBSCREEN])
        copy(pubscr, (char *)args[ARG_PUBSCREEN], sizeof(pubscr));
    FreeArgs(rdargs);
    return TRUE;
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
    if (!(DOSBase = (struct DosLibrary *)OpenLibrary(DOSNAME, 36)))
        err = "";
    else
    {
        if (!get_arguments(startup))
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
