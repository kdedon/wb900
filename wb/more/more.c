/*
    Copyright (C) 1995-2012, The AROS Development Team. All rights reserved.
*/

/****************************************************************************************/

#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/var.h>
#include <intuition/intuition.h>
#include <intuition/imageclass.h>
#include <intuition/gadgetclass.h>
#include <libraries/gadtools.h>
#include <devices/rawkeycodes.h>
#include <workbench/workbench.h>
#include <workbench/startup.h>
#include <graphics/gfx.h>
#include <graphics/layers.h>
#include <graphics/regions.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/gadtools.h>
#include <proto/utility.h>
#include <proto/workbench.h>

#include "global.h"
#include "req.h"

static LONG abs(LONG x)
{
    if (x < 0)
        return -x;
    return x;
}

/****************************************************************************************/

#define DEFAULT_TABSIZE 8

#define INNER_SPACING_X 2
#define INNER_SPACING_Y 2

#define MAX_TEXTLINELEN 4096

/* The file is read through a window of up to WINLEN bytes */
#define WINLEN          0x4000

/* The file offset of each line and of the end, in blocks of LINEBLOCK */
#define LINESHIFT       8
#define LINEBLOCK       (1L << LINESHIFT)
#define LINEOFF(n)      (lineindex.blocks[(n) >> LINESHIFT][(n) & (LINEBLOCK - 1)])

#define ARG_TEMPLATE "FILE"

#define DEFAULT_EDITOR "SYS:Utilities/Edit"

enum
{
    ARG_FILE,
    NUM_ARGS
};

enum
{
    GAD_UPARROW,
    GAD_DOWNARROW,
    GAD_LEFTARROW,
    GAD_RIGHTARROW,
    GAD_VERTSCROLL,
    GAD_HORIZSCROLL,
    NUM_GADGETS
};

enum
{
    IMG_UPARROW,
    IMG_DOWNARROW,
    IMG_LEFTARROW,
    IMG_RIGHTARROW,
    IMG_SIZE,
    NUM_IMAGES
};

/****************************************************************************************/

struct LineIndex
{
    LONG **blocks;
    LONG cap, count;    /* block pointers, offsets */
};

/****************************************************************************************/

struct DosLibrary       *DOSBase;
struct IntuitionBase    *IntuitionBase;
struct GfxBase          *GfxBase;
struct Library          *GadToolsBase;
struct Library          *UtilityBase;
struct Library          *WorkbenchBase;
struct Screen           *scr;
struct DrawInfo         *dri;
APTR                    vi;
struct Menu             *menus;
struct Window           *win;
struct MsgPort          *msgport;
struct AppWindow        *appwindow;

ULONG                   gotomask, findmask;
UBYTE                   filenamebuffer[300];

/****************************************************************************************/

static struct RastPort  *rp;
static struct RDArgs    *MyArgs;

static struct Gadget    *gad[NUM_GADGETS], *firstgadget, *activearrowgad;
static struct Image     *img[NUM_IMAGES];

static struct LineIndex lineindex;

/* winlen bytes of the file from winstart, in a buffer of winsize */
static UBYTE            *window;
static LONG             winsize, winstart, winlen;
static UBYTE            textname[300];

static char             *filename, s[256], *searchtext;
static CONST_STRPTR     errmsg;

static BPTR             fh;

static UWORD            tabsize = DEFAULT_TABSIZE;

static WORD             fontwidth, fontheight, borderleft, bordertop;
static WORD             shinepen, bgpen, textpen;
static LONG             borderright, borderbottom, visiblex, visibley;
static LONG             fontbaseline, textstartx, textstarty, textendx, textendy;
static LONG             viewstartx, viewstarty;
static WORD             winwidth, winheight;

static ULONG            winmask, msgmask;
static LONG             filelen, num_lines, max_textlen;
static LONG             search_startline, found_line = -1;
static WORD             arrowticker;

static BOOL             in_main_loop;

static IPTR             Args[NUM_ARGS];

static BPTR             oldlock = (BPTR)-1;

#define SETTAG(t, i, tag, data) ((t)[i].ti_Tag = (tag), (t)[i].ti_Data = (IPTR)(data))

/*********************************************************************************************/

WORD ShowMessage(CONST_STRPTR title, CONST_STRPTR text, CONST_STRPTR gadtext)
{
    struct EasyStruct es;

    es.es_StructSize   = sizeof(es);
    es.es_Flags        = 0;
    es.es_Title        = title;
    es.es_TextFormat   = text;
    es.es_GadgetFormat = gadtext;

    return EasyRequestArgs(win, &es, NULL, NULL);
}

/****************************************************************************************/

static void FreeIndex(struct LineIndex *li)
{
    LONG n = (li->count + LINEBLOCK - 1) >> LINESHIFT;

    if (!li->blocks) return;

    while (n--) FreeVec(li->blocks[n]);
    FreeVec(li->blocks);
}

/****************************************************************************************/

/* An error during setup: Cleanup() reports it */
BOOL Fail(CONST_STRPTR msg)
{
    errmsg = msg;
    return FALSE;
}

/****************************************************************************************/

static LONG Cleanup(CONST_STRPTR msg)
{
    WORD rc, i;

    if (msg)
    {
        if (IntuitionBase && !((struct Process *)FindTask(NULL))->pr_CLI)
        {
            ShowMessage("More", msg, MSG(MSG_OK));
        } else if (DOSBase) {
            PutStr("More: ");
            PutStr(msg);
            PutStr("\n");
        }
        rc = RETURN_WARN;
    } else {
        rc = RETURN_OK;
    }

    CleanupRequesters();

    KillMenus();

    if (appwindow)
    {
        RemoveAppWindow(appwindow);
    }
    if (msgport)
    {
        DeleteMsgPort(msgport);
    }

    if (win)
    {
        for(i = 0; i < NUM_GADGETS;i++)
        {
            if (gad[i]) RemoveGadget(win, gad[i]);
        }

        CloseWindow(win);
    }

    for(i = 0; i < NUM_GADGETS;i++)
    {
        if (gad[i]) DisposeObject(gad[i]);
    }
    for(i = 0; i < NUM_IMAGES;i++)
    {
        if (img[i]) DisposeObject(img[i]);
    }

    if (vi) FreeVisualInfo(vi);
    if (dri) FreeScreenDrawInfo(scr, dri);
    if (scr) UnlockPubScreen(NULL, scr);

    FreeIndex(&lineindex);
    if (window) FreeVec(window);

    if (fh) Close(fh);

    if (oldlock != (BPTR)-1) CurrentDir(oldlock);

    if (WorkbenchBase) CloseLibrary(WorkbenchBase);
    if (UtilityBase) CloseLibrary(UtilityBase);
    if (GadToolsBase) CloseLibrary(GadToolsBase);
    if (GfxBase) CloseLibrary((struct Library *)GfxBase);
    if (IntuitionBase) CloseLibrary((struct Library *)IntuitionBase);

    if (MyArgs) FreeArgs(MyArgs);

    return rc;
}

/****************************************************************************************/

static BOOL DosError(void)
{
    Fault(IoErr(), NULL, s, 255);
    if (in_main_loop)
    {
        ShowMessage("More", s, MSG(MSG_OK));
        return FALSE;
    }
    return Fail(s);
}

/****************************************************************************************/

static BOOL GetArguments(struct WBStartup *startup)
{
    if (!startup)
    {
        if (!(MyArgs = ReadArgs(ARG_TEMPLATE, (LONG *)Args, NULL)))
        {
            return DosError();
        }

        filename = (char *)Args[ARG_FILE];
    }
    else if (startup->sm_NumArgs > 1)
    {
        filename = startup->sm_ArgList[1].wa_Name;
        if ((startup->sm_ArgList[1].wa_Lock == NULL) || (filename == NULL))
            return FALSE;

        oldlock = CurrentDir(startup->sm_ArgList[1].wa_Lock);
    }

    return TRUE;
}

/****************************************************************************************/

static BOOL OpenLibs(void)
{
    if (!(IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 39)))
    {
        sprintf(s, MSG(MSG_CANT_OPEN_LIB), "intuition.library", 39L);
        return Fail(s);
    }

    if (!(GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 39)))
    {
        sprintf(s, MSG(MSG_CANT_OPEN_LIB), "graphics.library", 39L);
        return Fail(s);
    }

    if (!(GadToolsBase = OpenLibrary("gadtools.library", 39)))
    {
        sprintf(s, MSG(MSG_CANT_OPEN_LIB), "gadtools.library", 39L);
        return Fail(s);
    }

    if (!(UtilityBase = OpenLibrary("utility.library", 39)))
    {
        sprintf(s, MSG(MSG_CANT_OPEN_LIB), "utility.library", 39L);
        return Fail(s);
    }

    WorkbenchBase = OpenLibrary("workbench.library", 39);

    return TRUE;
}

/****************************************************************************************/

/* The column after a tab at col */
#define TABSTOP(col)    ((col) + tabsize - (col) % tabsize)

static BOOL AddOffset(struct LineIndex *li, LONG off)
{
    LONG **nb, b = li->count >> LINESHIFT;

    if ((li->count & (LINEBLOCK - 1)) == 0)
    {
        if (b == li->cap)
        {
            if (!(nb = AllocVec((b + 16) * sizeof(*nb), MEMF_PUBLIC))) return FALSE;
            if (li->blocks)
            {
                CopyMem(li->blocks, nb, b * sizeof(*nb));
                FreeVec(li->blocks);
            }
            li->blocks = nb;
            li->cap = b + 16;
        }
        if (!(li->blocks[b] = AllocVec(LINEBLOCK * sizeof(LONG), MEMF_PUBLIC))) return FALSE;
    }
    li->blocks[b][li->count++ & (LINEBLOCK - 1)] = off;
    return TRUE;
}

/****************************************************************************************/

/* len bytes from off, at most winsize; zeros past the end of the file */
static UBYTE *GetText(LONG off, LONG len)
{
    BPTR f;
    LONG n = 0;

    if (off < winstart || off + len > winstart + winlen)
    {
        /* Lines before off stay in the window for scrolling back */
        winstart = off - winsize / 4;
        if (winstart < 0 || off + len > winstart + winsize) winstart = off;

        if ((f = Open(textname, MODE_OLDFILE)))
        {
            if (Seek(f, winstart, OFFSET_BEGINNING) >= 0) n = Read(f, window, winsize);
            Close(f);
        }
        if (n < 0) n = 0;
        while (n < winsize) window[n++] = 0;
        winlen = winsize;
    }

    return window + (off - winstart);
}

/****************************************************************************************/

static BOOL OpenFile(void)
{
    struct LineIndex    new_index;
    UBYTE               *new_window = NULL;
    LONG                pos = 0, col = 0, new_max = 0, size, n, i;
    UBYTE               c = '\n';
    BOOL                ok;

    if (!(fh = Open(filename, MODE_OLDFILE)))
    {
        return DosError();
    }

    if (Seek(fh, 0, OFFSET_END) < 0 || (size = Seek(fh, 0, OFFSET_BEGINNING)) < 0)
    {
        Close(fh); fh = 0;
        return DosError();
    }
    size = size < WINLEN ? size + 1 : WINLEN;

    /* Index the lines, reading through the new window */
    new_index.blocks = NULL;
    new_index.cap = new_index.count = 0;
    ok = (new_window = AllocVec(size, MEMF_PUBLIC)) && AddOffset(&new_index, 0);
    while (ok && (n = Read(fh, new_window, size)) > 0)
    {
        for (i = 0; i < n && ok; i++)
        {
            c = new_window[i];
            pos++;
            if (c == '\n')
            {
                if (col > new_max) new_max = col;
                col = 0;
                ok = AddOffset(&new_index, pos);
            } else if (col < MAX_TEXTLINELEN) {
                col = c == '\t' ? TABSTOP(col) : col + 1;
                if (col > MAX_TEXTLINELEN) col = MAX_TEXTLINELEN;
            }
        }
    }

    Close(fh); fh = 0;

    if (ok && n < 0)
    {
        FreeIndex(&new_index);
        FreeVec(new_window);
        return DosError();
    }

    /* A last line without a newline ends one past the file */
    if (ok && (pos == 0 || c != '\n'))
    {
        if (col > new_max) new_max = col;
        ok = AddOffset(&new_index, pos + 1);
    }

    if (!ok)
    {
        FreeIndex(&new_index);
        if (new_window) FreeVec(new_window);
        if (in_main_loop)
        {
            ShowMessage("More", MSG(MSG_NO_MEM), MSG(MSG_OK));
            return FALSE;
        }
        return Fail(MSG(MSG_NO_MEM));
    }

    FreeIndex(&lineindex);
    if (window) FreeVec(window);

    lineindex   = new_index;
    window      = new_window;
    winsize     = size;
    winlen      = 0;
    filelen     = pos;
    num_lines   = new_index.count - 1;
    max_textlen = new_max;
    found_line  = -1;
    search_startline = 0;

    if (filename != (char *)filenamebuffer) strncpy(filenamebuffer, filename, 299);
    filename = filenamebuffer;
    strncpy(textname, filename, 299);

    return TRUE;
}

/****************************************************************************************/

static BOOL GetVisual(void)
{
    if (!(scr = LockPubScreen(NULL)))
    {
        return Fail(MSG(MSG_CANT_LOCK_SCR));
    }

    if (!(dri = GetScreenDrawInfo(scr)))
    {
        return Fail(MSG(MSG_CANT_GET_DRI));
    }

    if (!(vi = GetVisualInfoA(scr, NULL)))
    {
        return Fail(MSG(MSG_CANT_GET_VI));
    }

    shinepen = dri->dri_Pens[SHINEPEN];
    textpen = dri->dri_Pens[TEXTPEN];
    bgpen = dri->dri_Pens[BACKGROUNDPEN];

    return TRUE;
}

/****************************************************************************************/

/* An arrow button in the right or bottom border */
static struct Gadget *MakeArrow(WORD which, LONG relright, LONG relbottom, ULONG border,
                                struct Gadget *previous)
{
    struct TagItem t[9];

    SETTAG(t, 0, GA_Image, img[which]);
    SETTAG(t, 1, GA_RelRight, relright);
    SETTAG(t, 2, GA_RelBottom, relbottom);
    SETTAG(t, 3, GA_ID, which);
    SETTAG(t, 4, border, TRUE);
    SETTAG(t, 5, GA_Immediate, TRUE);
    SETTAG(t, 6, GA_RelVerify, TRUE);
    SETTAG(t, 7, previous ? GA_Previous : TAG_IGNORE, previous);
    SETTAG(t, 8, TAG_DONE, 0);

    return (struct Gadget *)NewObjectA(NULL, BUTTONGCLASS, t);
}

/* A borderless scroller; the knob reports as it moves */
static struct Gadget *MakeScroller(WORD id, struct TagItem *pos, ULONG border, ULONG freedom,
                                   struct Gadget *previous)
{
    struct TagItem t[15];

    SETTAG(t, 0, GA_ID, id);
    SETTAG(t, 1, GA_Previous, previous);
    SETTAG(t, 2, border, TRUE);
    SETTAG(t, 3, GA_RelVerify, TRUE);
    SETTAG(t, 4, GA_Immediate, TRUE);
    SETTAG(t, 5, GA_FollowMouse, TRUE);
    SETTAG(t, 6, PGA_NewLook, TRUE);
    SETTAG(t, 7, PGA_Borderless, TRUE);
    SETTAG(t, 8, PGA_Total, 100);
    SETTAG(t, 9, PGA_Visible, 100);
    SETTAG(t, 10, PGA_Freedom, freedom);
    SETTAG(t, 11, TAG_MORE, pos);

    return (struct Gadget *)NewObjectA(NULL, PROPGCLASS, t);
}

static BOOL MakeGadgets(void)
{
    static WORD img2which[] =
    {
        UPIMAGE,
        DOWNIMAGE,
        LEFTIMAGE,
        RIGHTIMAGE,
        SIZEIMAGE
    };

    struct TagItem t[5];
    IPTR imagew[NUM_IMAGES], imageh[NUM_IMAGES];
    LONG v_offset, h_offset, btop;
    WORD i;

    for(i = 0;i < NUM_IMAGES;i++)
    {
        SETTAG(t, 0, SYSIA_DrawInfo, dri);
        SETTAG(t, 1, SYSIA_Which, img2which[i]);
        SETTAG(t, 2, TAG_DONE, 0);
        img[i] = (struct Image *)NewObjectA(NULL, SYSICLASS, t);

        if (!img[i]) return Fail(MSG(MSG_CANT_CREATE_SYSIMAGE));

        GetAttr(IA_Width, (Object *)img[i], &imagew[i]);
        GetAttr(IA_Height, (Object *)img[i], &imageh[i]);
    }

    btop = scr->WBorTop + dri->dri_Font->tf_YSize + 1;

    v_offset = imagew[IMG_DOWNARROW] / 4;
    h_offset = imageh[IMG_LEFTARROW] / 4;

    firstgadget =
    gad[GAD_UPARROW] = MakeArrow(IMG_UPARROW, -(LONG)imagew[IMG_UPARROW] + 1,
                                 -(LONG)(imageh[IMG_DOWNARROW] + imageh[IMG_UPARROW] + imageh[IMG_SIZE]) + 1,
                                 GA_RightBorder, NULL);

    gad[GAD_DOWNARROW] = MakeArrow(IMG_DOWNARROW, -(LONG)imagew[IMG_UPARROW] + 1,
                                   -(LONG)(imageh[IMG_UPARROW] + imageh[IMG_SIZE]) + 1,
                                   GA_RightBorder, gad[GAD_UPARROW]);

    SETTAG(t, 0, GA_Top, btop + 1);
    SETTAG(t, 1, GA_RelRight, -(LONG)imagew[IMG_DOWNARROW] + v_offset + 1);
    SETTAG(t, 2, GA_Width, imagew[IMG_DOWNARROW] - v_offset * 2);
    SETTAG(t, 3, GA_RelHeight, -(LONG)(imageh[IMG_DOWNARROW] + imageh[IMG_UPARROW] + imageh[IMG_SIZE]) - btop - 2);
    SETTAG(t, 4, TAG_DONE, 0);
    gad[GAD_VERTSCROLL] = MakeScroller(GAD_VERTSCROLL, t, GA_RightBorder, FREEVERT, gad[GAD_DOWNARROW]);

    gad[GAD_RIGHTARROW] = MakeArrow(IMG_RIGHTARROW, -(LONG)(imagew[IMG_SIZE] + imagew[IMG_RIGHTARROW]) + 1,
                                    -(LONG)imageh[IMG_RIGHTARROW] + 1,
                                    GA_BottomBorder, gad[GAD_VERTSCROLL]);

    gad[GAD_LEFTARROW] = MakeArrow(IMG_LEFTARROW,
                                   -(LONG)(imagew[IMG_SIZE] + imagew[IMG_RIGHTARROW] + imagew[IMG_LEFTARROW]) + 1,
                                   -(LONG)imageh[IMG_RIGHTARROW] + 1,
                                   GA_BottomBorder, gad[GAD_RIGHTARROW]);

    SETTAG(t, 0, GA_Left, scr->WBorLeft);
    SETTAG(t, 1, GA_RelBottom, -(LONG)imageh[IMG_LEFTARROW] + h_offset + 1);
    SETTAG(t, 2, GA_RelWidth, -(LONG)(imagew[IMG_LEFTARROW] + imagew[IMG_RIGHTARROW] + imagew[IMG_SIZE]) - scr->WBorRight - 2);
    SETTAG(t, 3, GA_Height, imageh[IMG_LEFTARROW] - (h_offset * 2));
    SETTAG(t, 4, TAG_DONE, 0);
    gad[GAD_HORIZSCROLL] = MakeScroller(GAD_HORIZSCROLL, t, GA_BottomBorder, FREEHORIZ, gad[GAD_LEFTARROW]);

    for(i = 0;i < NUM_GADGETS;i++)
    {
        if (!gad[i]) return Fail(MSG(MSG_CANT_CREATE_GADGET));
    }

    return TRUE;
}

/****************************************************************************************/

static void SetProp(WORD gadid, LONG top, LONG visible, LONG total)
{
    struct TagItem t[4];

    SETTAG(t, 0, PGA_Top, top);
    SETTAG(t, 1, visible >= 0 ? PGA_Visible : TAG_IGNORE, visible);
    SETTAG(t, 2, total >= 0 ? PGA_Total : TAG_IGNORE, total);
    SETTAG(t, 3, TAG_DONE, 0);
    SetGadgetAttrsA(gad[gadid], win, NULL, t);
}

/****************************************************************************************/

static void CalcVisible(void)
{
    visiblex = (win->Width - borderleft - borderright -
               INNER_SPACING_X * 2) / fontwidth;

    visibley = (win->Height - bordertop - borderbottom -
               INNER_SPACING_Y * 2) / fontheight;

    if (visiblex < 1) visiblex = 1;
    if (visibley < 1) visibley = 1;

    textendx = textstartx + visiblex * fontwidth - 1;
    textendy = textstarty + visibley * fontheight - 1;
}

/****************************************************************************************/

static void MySetAPen(struct RastPort *rp, LONG reg)
{
    static LONG oldreg = -1;

    if (reg != oldreg)
    {
        oldreg = reg;
        SetAPen(rp, reg);
    }
}

/****************************************************************************************/

static void MySetBPen(struct RastPort *rp, LONG reg)
{
    static LONG oldreg = -1;

    if (reg != oldreg)
    {
        oldreg = reg;
        SetBPen(rp, reg);
    }
}

/****************************************************************************************/

static void DrawTextLine(WORD viewline, WORD columns, BOOL clearright)
{
    static char tempstring[MAX_TEXTLINELEN + 1];
    static char c, *stringpos, *text;
    LONG realline = viewline + viewstarty;
    LONG len, k = 0;
    WORD textlen, i = 0, x;
    BOOL inverted;

    /* column < 0 means draw only first -column chars
    **
    ** column > 0 means draw only last column chars
    **
    ** column = 0 means draw whole line
    */

    if (columns != 0)
    {
        clearright = FALSE; /* because already cleared by ScrollRaster */
    }

    if ((viewline >= 0) && (viewline < visibley) &&
        (realline >= 0) && (realline < num_lines))
    {
        inverted = realline == found_line;

        /* Columns right of the view are not expanded */
        textlen = viewstartx + visiblex;
        if (textlen > MAX_TEXTLINELEN) textlen = MAX_TEXTLINELEN;

        len = LINEOFF(realline + 1) - LINEOFF(realline) - 1;
        if (len > textlen) len = textlen;
        text = (char *)GetText(LINEOFF(realline), len);

        stringpos = tempstring;

        while((k++ < len) && (c = *text++) && (i < textlen))
        {
            if (c == '\t')
            {
                do
                {
                    *stringpos++ = ' ';
                    i++;
                } while ((i % tabsize) && (i < textlen));
            } else {
                *stringpos++ = c;
                i++;
            }
        } /* while((c = *text++) && (i < textlen)) */

        stringpos = tempstring + viewstartx;
        i -= viewstartx;

        if (i < 0)
        {
            i = 0;
        } else {
            if (i > visiblex) i = visiblex;

            x = textstartx;
            if (columns < 0)
            {
                if (i > (-columns)) i = (-columns);
            } else if (columns > 0) {
                x = textstartx + (visiblex - columns) * fontwidth;
                stringpos += (visiblex - columns);
                i -= (visiblex - columns);
            }

            if (i > 0)
            {
                MySetAPen(rp, textpen);
                MySetBPen(rp, inverted ? shinepen : bgpen);

                Move(rp, x,
                        textstarty + (viewline * fontheight) + fontbaseline);

                Text(rp, stringpos, i);
            }
        }

    } /* if ((realline >= 0) && (realline < num_lines)) */

    if ((i < visiblex) && clearright)
    {
        MySetAPen(rp, bgpen);
        RectFill(rp, textstartx + (i * fontwidth),
                     textstarty + (viewline * fontheight),
                     textendx,
                     textstarty + (viewline * fontheight) + fontheight - 1);
    }
}

/****************************************************************************************/

static void DrawLines(LONG first, LONG last)
{
    LONG y;

    if (first < 0) first = 0;
    if (last >= visibley) last = visibley - 1;

    for(y = first;y <= last;y++)
    {
        DrawTextLine(y, 0, TRUE);
    }
}

static void DrawAllText(void)
{
    DrawLines(0, visibley - 1);
}

/****************************************************************************************/

static void SetWinTitle(void)
{
    static UBYTE wintitle[256];
    BPTR         lock;

    strncpy(s, filename, 200);
    s[200] = 0;
    if ((lock = Lock(filename, SHARED_LOCK)))
    {
        NameFromLock(lock, s, 200);
        UnLock(lock);
    }
    sprintf(wintitle, MSG(MSG_WIN_TITLE), s, num_lines, filelen);

    SetWindowTitles(win, wintitle, (UBYTE *)~0L);
}

/****************************************************************************************/

static void HandleFileChange(void)
{
    SetWinTitle();
    MySetAPen(rp, bgpen);
    RectFill(rp, textstartx, textstarty, textendx, textendy);

    CalcVisible();

    viewstartx = viewstarty = 0;

    SetProp(GAD_HORIZSCROLL, viewstartx, visiblex, max_textlen);
    SetProp(GAD_VERTSCROLL, viewstarty, visibley, num_lines);

    DrawAllText();

}

/****************************************************************************************/

static BOOL MakeWin(void)
{
    struct TagItem t[22];

    SETTAG(t, 0, WA_PubScreen, scr);
    SETTAG(t, 1, WA_Left, 0);
    SETTAG(t, 2, WA_Top, scr->BarHeight + 1);
    SETTAG(t, 3, WA_Width, 640);
    SETTAG(t, 4, WA_Height, 480);
    SETTAG(t, 5, WA_AutoAdjust, TRUE);
    SETTAG(t, 6, WA_SimpleRefresh, TRUE);
    SETTAG(t, 7, WA_CloseGadget, TRUE);
    SETTAG(t, 8, WA_DepthGadget, TRUE);
    SETTAG(t, 9, WA_DragBar, TRUE);
    SETTAG(t, 10, WA_SizeGadget, TRUE);
    SETTAG(t, 11, WA_SizeBBottom, TRUE);
    SETTAG(t, 12, WA_SizeBRight, TRUE);
    SETTAG(t, 13, WA_Activate, TRUE);
    SETTAG(t, 14, WA_Gadgets, firstgadget);
    SETTAG(t, 15, WA_MinWidth, 100);
    SETTAG(t, 16, WA_MinHeight, 100);
    SETTAG(t, 17, WA_MaxWidth, scr->Width);
    SETTAG(t, 18, WA_MaxHeight, scr->Height);
    SETTAG(t, 19, WA_NewLookMenus, TRUE);
    SETTAG(t, 20, WA_IDCMP, IDCMP_CLOSEWINDOW   |
                            IDCMP_NEWSIZE       |
                            IDCMP_GADGETDOWN    |
                            IDCMP_GADGETUP      |
                            IDCMP_MOUSEMOVE     |
                            IDCMP_VANILLAKEY    |
                            IDCMP_INTUITICKS    |
                            IDCMP_REFRESHWINDOW |
                            IDCMP_RAWKEY        |
                            IDCMP_MENUPICK);
    SETTAG(t, 21, TAG_DONE, 0);

    if (!(win = OpenWindowTagList(NULL, t)))
    {
        return Fail(MSG(MSG_CANT_CREATE_WIN));
    }

    SetWinTitle();

    winmask = 1L << win->UserPort->mp_SigBit;

    if (WorkbenchBase && (msgport = CreateMsgPort()))
    {
        if ((appwindow = AddAppWindowA(0, 0, win, msgport, NULL)))
            msgmask = 1L << msgport->mp_SigBit;
    }

    winwidth = win->Width;
    winheight = win->Height;

    rp = win->RPort;

    SetDrMd(rp, JAM2);

    fontwidth = rp->TxWidth;
    fontheight = rp->TxHeight;
    fontbaseline = rp->TxBaseline;

    borderleft = win->BorderLeft;
    bordertop = win->BorderTop;
    borderright = win->BorderRight;
    borderbottom = win->BorderBottom;

    textstartx = borderleft + INNER_SPACING_X;
    textstarty = bordertop + INNER_SPACING_Y;

    CalcVisible();

    SetProp(GAD_HORIZSCROLL, 0, visiblex, max_textlen);
    SetProp(GAD_VERTSCROLL, 0, visibley, num_lines);

    DrawAllText();

    SetMenuStrip(win, menus);

    return TRUE;
}

/****************************************************************************************/

static void NewWinSize(void)
{
    WORD new_winwidth, new_winheight;

    new_winwidth = win->Width;
    new_winheight = win->Height;

    CalcVisible();

    if ((viewstartx + visiblex) > max_textlen)
    {
        viewstartx = max_textlen - visiblex;
        if (viewstartx < 0) viewstartx = 0;
    }

    if ((viewstarty + visibley) > num_lines)
    {
        viewstarty = num_lines - visibley;
        if (viewstarty < 0) viewstarty = 0;
    }

    SetProp(GAD_HORIZSCROLL, viewstartx, visiblex, -1);
    SetProp(GAD_VERTSCROLL, viewstarty, visibley, -1);

    if (new_winwidth < winwidth)
    {
        MySetAPen(rp, bgpen);
        RectFill(rp, textendx + 1,
                     bordertop + INNER_SPACING_Y,
                     new_winwidth - borderright - 1,
                     new_winheight - borderbottom - 1);
    }

    if (new_winheight < winheight)
    {
        MySetAPen(rp, bgpen);
        RectFill(rp, borderleft + INNER_SPACING_X,
                     textendy + 1,
                     new_winwidth - borderright - 1,
                     new_winheight - borderbottom - 1);
    }

    if ((new_winwidth > winwidth) ||
        (new_winheight > winheight))
    {
        DrawAllText();
    }

    winwidth  = new_winwidth;
    winheight = new_winheight;
}

/****************************************************************************************/

/* Redraw only the lines the damage touches */
static void HandleRefresh(void)
{
    struct Rectangle *b = &win->WLayer->DamageList->bounds;
    LONG first = (b->MinY - textstarty) / fontheight;
    LONG last = (b->MaxY - textstarty) / fontheight;

    BeginRefresh(win);
    DrawLines(first, last);
    EndRefresh(win, TRUE);
}

/****************************************************************************************/

static void ScrollTo(WORD gadid, LONG top, BOOL refreshprop)
{
    LONG y, dx, dy;

    MySetBPen(rp, bgpen);

    switch(gadid)
    {
        case GAD_VERTSCROLL:
            if (top + visibley > num_lines)
            {
                top = num_lines - visibley;
            }
            if (top < 0) top = 0;

            if (top != viewstarty )
            {
                dy = top - viewstarty;
                viewstarty = top;

                if (refreshprop)
                {
                    SetProp(gadid, viewstarty, -1, -1);
                }

                if (abs(dy) >= visibley)
                {
                    DrawAllText();
                } else {
                    ScrollRaster(rp,
                                 0,
                                 fontheight * dy,
                                 textstartx,
                                 textstarty,
                                 textendx,
                                 textendy);

                    if (rp->Layer->Flags & LAYERREFRESH)
                    {
                        HandleRefresh();
                    }

                    /* ScrollRaster cleared the new lines */
                    if (dy > 0)
                    {
                        for (y = visibley - dy;y < visibley;y++)
                        {
                            DrawTextLine(y, 0, FALSE);
                        }
                    } else {
                        for (y = 0;y < -dy;y++)
                        {
                            DrawTextLine(y, 0, FALSE);
                        }
                    }
                }

            } /* if (top != viewstarty ) */
            break;

        case GAD_HORIZSCROLL:
            if (top + visiblex > max_textlen)
            {
                top = max_textlen - visiblex;
            }
            if (top < 0) top = 0;

            if (top != viewstartx )
            {
                dx = top - viewstartx;
                viewstartx = top;

                if (refreshprop)
                {
                    SetProp(gadid, viewstartx, -1, -1);
                }

                if (abs(dx) >= visiblex)
                {
                    DrawAllText();
                } else {
                    ScrollRaster(rp,
                                 fontwidth * dx,
                                 0,
                                 textstartx,
                                 textstarty,
                                 textendx,
                                 textendy);

                    if (rp->Layer->Flags & LAYERREFRESH)
                    {
                        HandleRefresh();
                    }

                    for (y = 0;y < visibley;y++)
                    {
                        DrawTextLine(y, dx, TRUE);
                    }
                }

            } /* if (top != viewstartx ) */
            break;

    } /* switch(gadid) */

}

/****************************************************************************************/

static void HandleScrollGadget(WORD gadid)
{
    struct IntuiMessage *msg;
    IPTR top;
    BOOL ok=FALSE;

    while (!ok)
    {
        WaitPort(win->UserPort);
        while ((msg = (struct IntuiMessage *)GetMsg(win->UserPort)))
        {
            switch (msg->Class)
            {
                case IDCMP_GADGETUP:
                    ok=TRUE;
                    /* fall through */

                case IDCMP_MOUSEMOVE:
                    GetAttr(PGA_Top, (Object *)gad[gadid], &top);
                    ScrollTo(gadid, top, FALSE);
                    break;

                case IDCMP_REFRESHWINDOW:
                    HandleRefresh();
                    break;

            } /* switch (msg->Class) */
            ReplyMsg((struct Message *)msg);

        } /* while ((msg = (struct IntuiMessage *)GetMsg(win->UserPort))) */

    } /* while (!ok) */
}

/****************************************************************************************/

static BOOL FindString(LONG line, char *search, LONG searchlen)
{
    LONG off = LINEOFF(line);
    LONG textlen = LINEOFF(line + 1) - off - 1;
    LONG i, j, n;
    UBYTE *text;

    /* A line longer than the window, in pieces that overlap by searchlen - 1 */
    while(textlen >= searchlen)
    {
        n = textlen < winsize ? textlen : winsize;
        text = GetText(off, n);

        for(i = 0;i + searchlen <= n;i++)
        {
            for(j = 0;j < searchlen;j++)
            {
                if (ToUpper(text[i + j]) != ToUpper(search[j])) break;
            }
            if (j == searchlen) return TRUE;
        }

        if (n == textlen) break;
        off += n - searchlen + 1;
        textlen -= n - searchlen + 1;
    }

    return FALSE;
}

/****************************************************************************************/

static void DoSearch(WORD kind)
{
    LONG line, searchlen, n;
    BOOL done = FALSE;

    if (!searchtext) return;

    searchlen = strlen(searchtext);
    if (searchlen == 0) return;

    line = search_startline;

    if (kind == SEARCH_NEW)
    {
        line = 0;
        kind = SEARCH_NEXT;
    }
    else if (found_line >= 0)
    {
        /* Past the current match, or no further match */
        line += (kind == SEARCH_NEXT) ? 1 : -1;
        if (line < 0 || line >= num_lines)
        {
            DisplayBeep(NULL);
            return;
        }
    }

    while(!done)
    {
        if (FindString(line, searchtext, searchlen))
        {
            done = TRUE;
            if (found_line >= 0)
            {
                n = found_line;
                found_line = -1;
                DrawTextLine(n - viewstarty, 0, TRUE);
            }

            ScrollTo(GAD_VERTSCROLL, line - visibley / 2, TRUE);

            found_line = line;
            search_startline = line;
            DrawTextLine(found_line - viewstarty, 0, TRUE);
            break;
        }

        if (kind == SEARCH_NEXT)
        {
            line++;
            if (line >= num_lines)
            {
                done = TRUE;
                DisplayBeep(NULL);
            }
        } else {
            line--;
            if (line < 0)
            {
                done = TRUE;
                DisplayBeep(NULL);
            }
        }

    } /* while(!done) */

}

/****************************************************************************************/

/* Open the file in the editor, as Workbench opens a project */
static void StartEditor(void)
{
    struct TagItem t[3];
    TEXT editor[100];
    BPTR lock, dir;

    if (!WorkbenchBase || !(lock = Lock(filename, SHARED_LOCK)))
    {
        DisplayBeep(NULL);
        return;
    }
    dir = ParentDir(lock);
    UnLock(lock);

    if (GetVar("EDITOR", editor, sizeof(editor), GVF_GLOBAL_ONLY) < 0)
        strncpy(editor, DEFAULT_EDITOR, sizeof(editor));

    SETTAG(t, 0, WBOPENA_ArgLock, dir);
    SETTAG(t, 1, WBOPENA_ArgName, FilePart(filename));
    SETTAG(t, 2, TAG_DONE, 0);
    if (!dir || !OpenWorkbenchObjectA(editor, t))
        DisplayBeep(NULL);

    if (dir) UnLock(dir);
}

/****************************************************************************************/

static BOOL HandleWin(void)
{
    struct IntuiMessage *msg;
    struct MenuItem     *item;
    WORD                gadid, delta;
    UWORD               men, code;
    UBYTE               key;
    BOOL                pagescroll, maxscroll, quitme = FALSE;
    struct AppMessage  *appmsg;

    while (msgport && (appmsg = (struct AppMessage *) GetMsg(msgport)))
    {
        BOOL newfile = FALSE;

        if (appmsg->am_Type == AMTYPE_APPWINDOW)
        {
            if (appmsg->am_NumArgs >= 1)
            {
                NameFromLock(appmsg->am_ArgList->wa_Lock, filenamebuffer, 299);
                AddPart(filenamebuffer, appmsg->am_ArgList->wa_Name, 299);
                filename = filenamebuffer;
                newfile = TRUE;
            }
        }

        ReplyMsg ((struct Message *) appmsg);
        ActivateWindow(win);

        if (newfile && OpenFile())
            HandleFileChange();

    } /* while ((appmsg = (struct AppMessage *) GetMsg(msgport))) */

    while ((msg = (struct IntuiMessage *)GetMsg(win->UserPort)))
    {
        switch(msg->Class)
        {
            case IDCMP_CLOSEWINDOW:
                quitme = TRUE;
                break;

            case IDCMP_NEWSIZE:
                NewWinSize();
                break;

            case IDCMP_REFRESHWINDOW:
                HandleRefresh();
                break;

            case IDCMP_GADGETDOWN:
                gadid = ((struct Gadget *)msg->IAddress)->GadgetID;
                arrowticker = 3;

                switch(gadid)
                {
                    case GAD_HORIZSCROLL:
                    case GAD_VERTSCROLL:
                        HandleScrollGadget(gadid);
                        break;

                    case GAD_UPARROW:
                        activearrowgad = (struct Gadget *)msg->IAddress;
                        ScrollTo(GAD_VERTSCROLL, viewstarty - 1, TRUE);
                        break;

                    case GAD_DOWNARROW:
                        activearrowgad = (struct Gadget *)msg->IAddress;
                        ScrollTo(GAD_VERTSCROLL, viewstarty + 1, TRUE);
                        break;

                    case GAD_LEFTARROW:
                        activearrowgad = (struct Gadget *)msg->IAddress;
                        ScrollTo(GAD_HORIZSCROLL, viewstartx - 1, TRUE);
                        break;

                    case GAD_RIGHTARROW:
                        activearrowgad = (struct Gadget *)msg->IAddress;
                        ScrollTo(GAD_HORIZSCROLL, viewstartx + 1, TRUE);
                        break;
                };
                break;

            case IDCMP_INTUITICKS:
                if (activearrowgad)
                {
                    if (arrowticker)
                    {
                        arrowticker--;
                    }
                    else if (activearrowgad->Flags & GFLG_SELECTED)
                    {
                        switch(activearrowgad->GadgetID)
                        {
                            case GAD_UPARROW:
                                ScrollTo(GAD_VERTSCROLL, viewstarty - 1, TRUE);
                                break;

                            case GAD_DOWNARROW:
                                ScrollTo(GAD_VERTSCROLL, viewstarty + 1, TRUE);
                                break;

                            case GAD_LEFTARROW:
                                ScrollTo(GAD_HORIZSCROLL, viewstartx - 1, TRUE);
                                break;

                            case GAD_RIGHTARROW:
                                ScrollTo(GAD_HORIZSCROLL, viewstartx + 1, TRUE);
                                break;
                        }
                    }
                }
                break;

            case IDCMP_GADGETUP:
                activearrowgad = NULL;
                break;

            case IDCMP_VANILLAKEY:
                key = ToUpper(msg->Code);
                if (key == 27)
                {
                    quitme = TRUE;
                }
                else if (key == 8 || (key == ' ' && (0 != (msg->Qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)))))
                {
                    ScrollTo(GAD_VERTSCROLL, viewstarty - (visibley - 1), TRUE);
                }
                else if (key == ' ')
                {
                    ScrollTo(GAD_VERTSCROLL, viewstarty + visibley - 1, TRUE);
                }
                else if (key == 13)
                {
                    ScrollTo(GAD_VERTSCROLL, viewstarty + 1, TRUE);
                }
                else if (strchr(MSG(MSG_SHORTCUT_TOP), key))
                {
                    ScrollTo(GAD_VERTSCROLL, 0, TRUE);
                }
                else if (strchr(MSG(MSG_SHORTCUT_BOTTOM), key))
                {
                    ScrollTo(GAD_VERTSCROLL, num_lines, TRUE);
                }
                else if (strchr(MSG(MSG_SHORTCUT_JUMP), key))
                {
                    Make_Goto_Requester();
                }
                else if (strchr(MSG(MSG_SHORTCUT_FIND), key))
                {
                    Make_Find_Requester();
                }
                else if (strchr(MSG(MSG_SHORTCUT_NEXT), key))
                {
                    DoSearch(SEARCH_NEXT);
                }
                else if (strchr(MSG(MSG_SHORTCUT_PREV), key))
                {
                    DoSearch(SEARCH_PREV);
                }
                else if (strchr(MSG(MSG_SHORTCUT_EDITOR), key))
                {
                    StartEditor();
                }
                break;

            case IDCMP_RAWKEY:
                pagescroll = (0 != (msg->Qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)));
                maxscroll  = (0 != (msg->Qualifier & (IEQUALIFIER_LALT | IEQUALIFIER_RALT | IEQUALIFIER_CONTROL)));

                code = msg->Code; delta = 1;

                switch(code)
                {
                    case RAWKEY_NM_WHEEL_UP:
                        code = CURSORUP;
                        delta = 3;
                        break;

                    case RAWKEY_NM_WHEEL_DOWN:
                        code = CURSORDOWN;
                        delta = 3;
                        break;

                    case RAWKEY_NM_WHEEL_LEFT:
                        code = CURSORLEFT;
                        delta = 3;
                        break;

                    case RAWKEY_NM_WHEEL_RIGHT:
                        code = CURSORRIGHT;
                        delta = 3;
                        break;
                }

                switch(code)
                {
                    case CURSORUP:
                        ScrollTo(GAD_VERTSCROLL,
                                 maxscroll ? 0 : viewstarty - (pagescroll ? visibley - 1 : delta),
                                 TRUE);
                        break;

                    case CURSORDOWN:
                        ScrollTo(GAD_VERTSCROLL,
                                 maxscroll ? num_lines : viewstarty + (pagescroll ? visibley - 1 : delta),
                                 TRUE);
                        break;

                    case CURSORLEFT:
                        ScrollTo(GAD_HORIZSCROLL,
                                 maxscroll ? 0 : viewstartx - (pagescroll ? visiblex - 1 : delta),
                                 TRUE);
                        break;

                    case CURSORRIGHT:
                        ScrollTo(GAD_HORIZSCROLL,
                                 maxscroll ? max_textlen : viewstartx + (pagescroll ? visiblex - 1 : delta),
                                 TRUE);
                        break;

                    case RAWKEY_HOME:
                        ScrollTo(GAD_VERTSCROLL, 0, TRUE);
                        break;

                    case RAWKEY_END:
                        ScrollTo(GAD_VERTSCROLL, num_lines, TRUE);
                        break;

                    case RAWKEY_PAGEUP:
                        ScrollTo(GAD_VERTSCROLL, viewstarty - (visibley - 1), TRUE);
                        break;

                    case RAWKEY_PAGEDOWN:
                        ScrollTo(GAD_VERTSCROLL, viewstarty + (visibley - 1), TRUE);
                        break;

                } /* switch(msg->Code) */
                break;

            case IDCMP_MENUPICK:
                men = msg->Code;
                while(men != MENUNULL)
                {
                    if ((item = ItemAddress(menus, men)))
                    {
                        switch((IPTR)GTMENUITEM_USERDATA(item))
                        {
                            case MEN_EDIT:
                                StartEditor();
                                break;

                            case MEN_ABOUT:
                                About();
                                break;

                            case MEN_QUIT:
                                quitme = TRUE;
                                break;

                            case MEN_OPEN:
                                if ((filename = GetFile()) && OpenFile())
                                {
                                    HandleFileChange();
                                }
                                filename = filenamebuffer;

                                break;

                            case MEN_FIND:
                                Make_Find_Requester();
                                break;

                            case MEN_FIND_NEXT:
                                DoSearch(SEARCH_NEXT);
                                break;

                            case MEN_FIND_PREV:
                                DoSearch(SEARCH_PREV);
                                break;

                            case MEN_JUMP:
                                Make_Goto_Requester();
                                break;

                        } /* switch(GTMENUITEM_USERDATA(item)) */

                        men = item->NextSelect;
                    } else {
                        men = MENUNULL;
                    }

                } /* while(men != MENUNULL) */
                break;

        } /* switch(msg->Class) */

        ReplyMsg((struct Message *)msg);

    } /* while ((msg = (struct IntuiMessage *)GetMsg(win->UserPort))) */

    return quitme;
}

/****************************************************************************************/

static void HandleAll(void)
{
    ULONG sigs;
    LONG  line;
    WORD  search_kind;
    BOOL  quitme = FALSE;

    in_main_loop = TRUE;

    ScreenToFront(win->WScreen);

    while(!quitme)
    {
        sigs = Wait(msgmask | winmask | gotomask | findmask | SIGBREAKF_CTRL_C);

        if ( (sigs & winmask) || (sigs & msgmask) )
            quitme = HandleWin();

        if (sigs & gotomask)
        {
            if (Handle_Goto_Requester(&line))
            {
                ScrollTo(GAD_VERTSCROLL, line - 1, TRUE);
            }
        }

        if (sigs & findmask)
        {
            if ((search_kind = Handle_Find_Requester(&searchtext)))
            {
                DoSearch(search_kind);
            }
        }

        if (sigs & SIGBREAKF_CTRL_C)
            quitme = TRUE;

    } /* while(!quitme) */
}

/****************************************************************************************/

int main(void)
{
    struct Process *me = (struct Process *)FindTask(NULL);
    struct WBStartup *startup = NULL;
    LONG rc;

    if (!me->pr_CLI)
    {
        WaitPort(&me->pr_MsgPort);
        startup = (struct WBStartup *)GetMsg(&me->pr_MsgPort);
    }

    if ((DOSBase = (struct DosLibrary *)OpenLibrary(DOSNAME, 36)))
    {
        if (GetArguments(startup) &&
            OpenLibs() &&
            GetVisual() &&
            (filename || (filename = GetFile())) &&
            OpenFile() &&
            MakeGadgets() &&
            MakeMenus() &&
            MakeWin())
        {
            HandleAll();
        }
        rc = Cleanup(errmsg);
        CloseLibrary((struct Library *)DOSBase);
    }
    else rc = RETURN_FAIL;

    if (startup)
    {
        Forbid();
        ReplyMsg((struct Message *)startup);
    }

    return rc;
}

/****************************************************************************************/
