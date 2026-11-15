/* SPDX-License-Identifier: MIT */
/*
 * ScreenMode: the Workbench screen's mode, size and look, on intuition
 * and gadtools.  The mode is ENV:Sys/screenmode.prefs, an IFF PREF with
 * an SCRM chunk; Use writes it, Save also writes ENVARC:.  The look is
 * OLD_LOOK and the 1.x colours in DEVS:system-configuration, which Save
 * writes and Use sets.  Workbench reads both when it opens its screen.
 *
 * Usage: ScreenMode
 */
#include <exec/exec.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <graphics/displayinfo.h>
#include <graphics/gfxbase.h>
#include <intuition/intuition.h>
#include <intuition/preferences.h>
#include <libraries/gadtools.h>
#include <prefs/prefhdr.h>
#include <prefs/screenmode.h>
#include <workbench/startup.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/gadtools.h>

const char *version = "$VER: ScreenMode 1.0 (15.11.2026)";

struct DosLibrary *DOSBase;
struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *GadToolsBase;

#define MAXMODES	16
#define NAMELEN		40

enum { GAD_LIST = 1, GAD_SIZE, GAD_WIDTH, GAD_HEIGHT, GAD_LOOK, GAD_SAVE,
	GAD_USE, GAD_CANCEL };

struct mode {
	struct Node m_Node;
	ULONG m_ID;
	UWORD m_MinW, m_MinH, m_MaxW, m_MaxH, m_Depth;
	char m_Name[NAMELEN];
};

static struct mode modes[MAXMODES];
static UWORD nmodes, cur;
static struct List list;
static struct Screen *scr;
static APTR vi;
static struct Gadget *glist, *lvgad, *sizegad, *wgad, *hgad, *lookgad;
static struct Window *win;
static struct ScreenModePrefs smp;
static struct Preferences prefs;
static BOOL look1x, oldlook;
static char sizetext[32];

static STRPTR looks[] = { "Default", "1.x", NULL };

/* The Workbench 1.0 colours: blue, white, black, orange */
static UWORD colours1x[4] = { 0x05A, 0xFFF, 0x002, 0xF80 };

static void set1(struct Gadget *g, Tag t, IPTR d)
{
	struct TagItem tl[2];

	tl[0].ti_Tag = t;	tl[0].ti_Data = d;
	tl[1].ti_Tag = TAG_DONE;
	GT_SetGadgetAttrsA(g, win, NULL, tl);
}

static char *putnum(char *s, ULONG n)
{
	char d[10];
	WORD i = 0;

	do {
		d[i++] = '0' + n % 10;
		n /= 10;
	} while (n);
	while (i > 0)
		*s++ = d[--i];
	*s = 0;
	return s;
}

static char *putstr(char *d, const char *s)
{
	while ((*d = *s++) != 0)
		d++;
	return d;
}

/* The display's modes, with "(not detected)" on one it has no monitor for */
static void getmodes(void)
{
	struct DimensionInfo dim;
	struct NameInfo ni;
	struct mode *m;
	ULONG id;
	char *p;

	NewList(&list);
	for (id = NextDisplayInfo(INVALID_ID); id != INVALID_ID &&
	    nmodes < MAXMODES; id = NextDisplayInfo(id)) {
		if (!GetDisplayInfoData(NULL, (UBYTE *)&dim, sizeof(dim),
		    DTAG_DIMS, id))
			continue;
		m = &modes[nmodes++];
		m->m_ID = id;
		m->m_MinW = dim.MinRasterWidth;
		m->m_MinH = dim.MinRasterHeight;
		m->m_MaxW = dim.Nominal.MaxX + 1;
		m->m_MaxH = dim.Nominal.MaxY + 1;
		m->m_Depth = dim.MaxDepth;
		if (GetDisplayInfoData(NULL, (UBYTE *)&ni, sizeof(ni),
		    DTAG_NAME, id))
			p = putstr(m->m_Name, (char *)ni.Name);
		else
			p = putnum(m->m_Name, id);
		if (ModeNotAvailable(id))
			putstr(p, " (not detected)");
		m->m_Node.ln_Name = m->m_Name;
		AddTail(&list, &m->m_Node);
	}
}

/* The saved FORM PREF's SCRM chunk */
static BOOL loadprefs(char *path)
{
	ULONG buf[24];
	UBYTE *b = (UBYTE *)buf;
	LONG len, at, size;
	BPTR fh;

	if ((fh = Open(path, MODE_OLDFILE)) == 0)
		return FALSE;
	len = Read(fh, buf, sizeof(buf));
	Close(fh);
	if (len < 12 || buf[0] != ID_FORM || buf[2] != ID_PREF)
		return FALSE;
	for (at = 12; at + 8 <= len; at += 8 + ((size + 1) & ~1L)) {
		size = *(LONG *)(b + at + 4);
		if (*(ULONG *)(b + at) == ID_SCRM &&
		    size >= sizeof(smp) && at + 8 + sizeof(smp) <= len) {
			CopyMem(b + at + 8, &smp, sizeof(smp));
			return TRUE;
		}
	}
	return FALSE;
}

static UBYTE *put32(UBYTE *p, ULONG v)
{
	CopyMem(&v, p, 4);
	return p + 4;
}

/* FORM PREF: a PRHD of version 0, then the SCRM chunk */
static BOOL saveprefs(char *dir, char *path)
{
	UBYTE buf[34 + sizeof(struct ScreenModePrefs)], *p;
	BPTR fh;
	BOOL ok;

	if ((fh = Lock(dir, SHARED_LOCK)) == 0)
		fh = CreateDir(dir);
	UnLock(fh);
	p = put32(buf, ID_FORM);
	p = put32(p, sizeof(buf) - 8);
	p = put32(p, ID_PREF);
	p = put32(p, ID_PRHD);
	p = put32(p, 6);
	p = put32(p, 0);
	*p++ = 0;
	*p++ = 0;
	p = put32(p, ID_SCRM);
	p = put32(p, sizeof(smp));
	CopyMem(&smp, p, sizeof(smp));
	if ((fh = Open(path, MODE_NEWFILE)) == 0)
		return FALSE;
	ok = Write(fh, buf, sizeof(buf)) == sizeof(buf);
	Close(fh);
	return ok;
}

static BOOL savelook(void)
{
	BPTR fh;
	LONG n;

	if ((fh = Open("DEVS:system-configuration", MODE_NEWFILE)) == 0)
		return FALSE;
	n = Write(fh, &prefs, sizeof(prefs)) == sizeof(prefs);
	Close(fh);
	return n;
}

static void showmode(void)
{
	struct mode *m = &modes[cur];
	char *p;

	p = putnum(sizetext, m->m_MaxW);
	*p++ = 'x';
	p = putnum(p, m->m_MaxH);
	p = putstr(p, ", ");
	p = putnum(p, 1L << m->m_Depth);
	putstr(p, " colours");
	set1(sizegad, GTTX_Text, (IPTR)sizetext);
	set1(wgad, GA_Disabled, m->m_MinW == m->m_MaxW);
	set1(hgad, GA_Disabled, m->m_MinH == m->m_MaxH);
}

static void pick(UWORD n)
{
	cur = n;
	showmode();
	set1(wgad, GTIN_Number, modes[n].m_MaxW);
	set1(hgad, GTIN_Number, modes[n].m_MaxH);
}

static LONG clamp(LONG v, UWORD lo, UWORD hi)
{
	return v < lo ? lo : v > hi ? hi : v;
}

static struct Gadget *makegadgets(void)
{
	static struct TagItem lvtags[] = {
		{ GTLV_Labels, 0 }, { GTLV_ShowSelected, 0 },
		{ GTLV_Selected, 0 }, { GTLV_MakeVisible, 0 }, { TAG_DONE, 0 } };
	static struct TagItem txtags[] = {
		{ GTTX_Text, 0 }, { GTTX_Border, TRUE }, { TAG_DONE, 0 } };
	static struct TagItem intags[] = {
		{ GTIN_Number, 0 }, { GTIN_MaxChars, 5 }, { TAG_DONE, 0 } };
	static struct TagItem cytags[] = {
		{ GTCY_Labels, 0 }, { GTCY_Active, 0 }, { TAG_DONE, 0 } };
	static struct TagItem notags[] = {
		{ GTTX_Text, (IPTR)"Changes show after a reboot" }, { TAG_DONE, 0 } };
	struct NewGadget ng;
	struct Gadget *g;
	WORD fw = GfxBase->DefaultFont->tf_XSize, fh = scr->Font->ta_YSize;
	WORD x0, y, lw, bw, lab = 7 * fw, row = fh + 6;

	x0 = scr->WBorLeft + 6;
	y = scr->WBorTop + fh + 1 + 4;
	lw = 32 * fw + 28;
	bw = (lw - 16) / 3;

	g = CreateContext(&glist);
	ng.ng_TextAttr = scr->Font;
	ng.ng_VisualInfo = vi;
	ng.ng_UserData = NULL;
	ng.ng_Flags = PLACETEXT_LEFT;

	ng.ng_LeftEdge = x0;
	ng.ng_TopEdge = y;
	ng.ng_Width = lw;
	ng.ng_Height = 6 * fh + 6;
	ng.ng_GadgetText = NULL;
	ng.ng_GadgetID = GAD_LIST;
	lvtags[0].ti_Data = (IPTR)&list;
	lvtags[2].ti_Data = lvtags[3].ti_Data = cur;
	g = lvgad = CreateGadgetA(LISTVIEW_KIND, g, &ng, lvtags);
	y += ng.ng_Height + 4;

	ng.ng_LeftEdge = x0 + lab;
	ng.ng_TopEdge = y;
	ng.ng_Width = lw - lab;
	ng.ng_Height = row;
	ng.ng_GadgetText = (UBYTE *)"Size";
	ng.ng_GadgetID = GAD_SIZE;
	txtags[0].ti_Data = (IPTR)"";
	g = sizegad = CreateGadgetA(TEXT_KIND, g, &ng, txtags);
	y += row + 4;

	ng.ng_TopEdge = y;
	ng.ng_Width = 6 * fw + 12;
	ng.ng_GadgetText = (UBYTE *)"Width";
	ng.ng_GadgetID = GAD_WIDTH;
	intags[0].ti_Data = smp.smp_Width;
	g = wgad = CreateGadgetA(INTEGER_KIND, g, &ng, intags);
	ng.ng_LeftEdge = x0 + lw - ng.ng_Width;
	ng.ng_GadgetText = (UBYTE *)"Height";
	ng.ng_GadgetID = GAD_HEIGHT;
	intags[0].ti_Data = smp.smp_Height;
	g = hgad = CreateGadgetA(INTEGER_KIND, g, &ng, intags);
	y += row + 4;

	ng.ng_LeftEdge = x0 + lab;
	ng.ng_TopEdge = y;
	ng.ng_Width = 10 * fw + 28;
	ng.ng_GadgetText = (UBYTE *)"Look";
	ng.ng_GadgetID = GAD_LOOK;
	cytags[0].ti_Data = (IPTR)looks;
	cytags[1].ti_Data = look1x;
	g = lookgad = CreateGadgetA(CYCLE_KIND, g, &ng, cytags);
	y += row + 4;

	ng.ng_LeftEdge = x0;
	ng.ng_TopEdge = y;
	ng.ng_Width = lw;
	ng.ng_GadgetText = NULL;
	ng.ng_GadgetID = 0;
	g = CreateGadgetA(TEXT_KIND, g, &ng, notags);
	y += row + 4;

	ng.ng_TopEdge = y;
	ng.ng_Width = bw;
	ng.ng_Flags = PLACETEXT_IN;
	ng.ng_GadgetText = (UBYTE *)"Save";
	ng.ng_GadgetID = GAD_SAVE;
	g = CreateGadgetA(BUTTON_KIND, g, &ng, NULL);
	ng.ng_LeftEdge = x0 + (lw - bw) / 2;
	ng.ng_GadgetText = (UBYTE *)"Use";
	ng.ng_GadgetID = GAD_USE;
	g = CreateGadgetA(BUTTON_KIND, g, &ng, NULL);
	ng.ng_LeftEdge = x0 + lw - bw;
	ng.ng_GadgetText = (UBYTE *)"Cancel";
	ng.ng_GadgetID = GAD_CANCEL;
	return CreateGadgetA(BUTTON_KIND, g, &ng, NULL);
}

static char *setup(void)
{
	struct TagItem t[12];
	struct Gadget *g;
	ULONG id;
	WORD w = 0, h = 0, i;

	if (!(IntuitionBase = (struct IntuitionBase *)
	    OpenLibrary("intuition.library", 39)) ||
	    !(GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 39)) ||
	    !(GadToolsBase = OpenLibrary("gadtools.library", 39)))
		return "Can't open gadtools.library";
	if (!(scr = LockPubScreen(NULL)) || !(vi = GetVisualInfoA(scr, NULL)))
		return "Can't lock the screen";
	getmodes();
	if (nmodes == 0)
		return "No display modes";

	/* The saved mode, else the screen's */
	if (loadprefs("ENV:Sys/screenmode.prefs"))
		id = smp.smp_DisplayID;
	else {
		id = GetVPModeID(&scr->ViewPort);
		smp.smp_Width = scr->Width;
		smp.smp_Height = scr->Height;
	}
	for (i = 0; i < nmodes; i++)
		if (modes[i].m_ID == id)
			cur = i;
	if (modes[cur].m_ID != id) {
		smp.smp_Width = modes[cur].m_MaxW;
		smp.smp_Height = modes[cur].m_MaxH;
	}
	GetPrefs(&prefs, sizeof(prefs));
	look1x = oldlook = (prefs.EnableCLI & OLD_LOOK) != 0;

	if (!makegadgets())
		return "Can't create the gadgets";
	for (g = glist; g; g = g->NextGadget) {
		if (g->LeftEdge + g->Width > w)
			w = g->LeftEdge + g->Width;
		if (g->TopEdge + g->Height > h)
			h = g->TopEdge + g->Height;
	}
	w += 6 + scr->WBorRight;
	h += 4 + scr->WBorBottom;
	t[0].ti_Tag = WA_Title;		t[0].ti_Data = (IPTR)"ScreenMode";
	t[1].ti_Tag = WA_PubScreen;	t[1].ti_Data = (IPTR)scr;
	t[2].ti_Tag = WA_Width;		t[2].ti_Data = w;
	t[3].ti_Tag = WA_Height;	t[3].ti_Data = h;
	t[4].ti_Tag = WA_Left;		t[4].ti_Data = w < scr->Width ?
	    (scr->Width - w) / 2 : 0;
	t[5].ti_Tag = WA_Top;		t[5].ti_Data = h < scr->Height ?
	    (scr->Height - h) / 3 : 0;
	t[6].ti_Tag = WA_Gadgets;	t[6].ti_Data = (IPTR)glist;
	t[7].ti_Tag = WA_IDCMP;		t[7].ti_Data = IDCMP_CLOSEWINDOW |
	    IDCMP_REFRESHWINDOW | IDCMP_VANILLAKEY | BUTTONIDCMP |
	    INTEGERIDCMP | CYCLEIDCMP | LISTVIEWIDCMP;
	t[8].ti_Tag = WA_DragBar;	t[8].ti_Data = TRUE;
	t[9].ti_Tag = WA_DepthGadget;	t[9].ti_Data = TRUE;
	t[10].ti_Tag = WA_Activate;	t[10].ti_Data = TRUE;
	t[11].ti_Tag = TAG_DONE;
	if (!(win = OpenWindowTagList(NULL, t)))
		return "Can't open the window";
	GT_RefreshWindow(win, NULL);
	showmode();
	return NULL;
}

static void error(char *msg)
{
	struct EasyStruct es;

	es.es_StructSize = sizeof(es);
	es.es_Flags = 0;
	es.es_Title = "ScreenMode";
	es.es_TextFormat = "%s";
	es.es_GadgetFormat = "OK";
	EasyRequestArgs(win, &es, NULL, (APTR)&msg);
}

/* Use, and with save also Save: FALSE when a file can't be written */
static BOOL apply(BOOL save)
{
	struct mode *m = &modes[cur];
	struct Preferences def;
	WORD i;

	smp.smp_DisplayID = m->m_ID;
	smp.smp_Width = clamp(((struct StringInfo *)wgad->SpecialInfo)->LongInt,
	    m->m_MinW, m->m_MaxW);
	smp.smp_Height = clamp(((struct StringInfo *)hgad->SpecialInfo)->LongInt,
	    m->m_MinH, m->m_MaxH);
	smp.smp_Depth = m->m_Depth;
	smp.smp_Control = 0;
	if (!saveprefs("ENV:Sys", "ENV:Sys/screenmode.prefs")) {
		error("Can't write ENV:Sys/screenmode.prefs");
		return FALSE;
	}
	if (save && !saveprefs("ENVARC:Sys", "ENVARC:Sys/screenmode.prefs")) {
		error("Can't write ENVARC:Sys/screenmode.prefs");
		return FALSE;
	}
	if (look1x == oldlook)
		return TRUE;
	GetDefPrefs(&def, sizeof(def));
	for (i = 0; i < 4; i++)
		(&prefs.color0)[i] = look1x ? colours1x[i] : (&def.color0)[i];
	prefs.EnableCLI = look1x ? prefs.EnableCLI | OLD_LOOK :
	    prefs.EnableCLI & ~OLD_LOOK;
	SetPrefs(&prefs, sizeof(prefs), TRUE);
	if (save && !savelook()) {
		error("Can't write DEVS:system-configuration");
		return FALSE;
	}
	return TRUE;
}

static void run(void)
{
	struct IntuiMessage *msg;
	ULONG class;
	UWORD code, id;

	for (;;) {
		Wait((1L << win->UserPort->mp_SigBit));
		while ((msg = GT_GetIMsg(win->UserPort)) != NULL) {
			class = msg->Class;
			code = msg->Code;
			id = class == IDCMP_GADGETUP ?
			    ((struct Gadget *)msg->IAddress)->GadgetID : 0;
			GT_ReplyIMsg(msg);
			switch (class) {
			case IDCMP_CLOSEWINDOW:
				return;
			case IDCMP_REFRESHWINDOW:
				GT_BeginRefresh(win);
				GT_EndRefresh(win, TRUE);
				break;
			case IDCMP_VANILLAKEY:
				if (code == 27)
					return;
				break;
			case IDCMP_GADGETUP:
				switch (id) {
				case GAD_LIST:
					pick(code);
					break;
				case GAD_LOOK:
					look1x = code;
					break;
				case GAD_SAVE:
				case GAD_USE:
					if (apply(id == GAD_SAVE))
						return;
					break;
				case GAD_CANCEL:
					return;
				}
				break;
			}
		}
	}
}

static void cleanup(char *msg)
{
	if (msg) {
		if (IntuitionBase &&
		    !((struct Process *)FindTask(NULL))->pr_CLI)
			error(msg);
		else {
			PutStr("ScreenMode: ");
			PutStr(msg);
			PutStr("\n");
		}
	}
	if (win)
		CloseWindow(win);
	FreeGadgets(glist);
	if (vi)
		FreeVisualInfo(vi);
	if (scr)
		UnlockPubScreen(NULL, scr);
	if (GadToolsBase)
		CloseLibrary(GadToolsBase);
	if (GfxBase)
		CloseLibrary((struct Library *)GfxBase);
	if (IntuitionBase)
		CloseLibrary((struct Library *)IntuitionBase);
}

int main(void)
{
	struct Process *me = (struct Process *)FindTask(NULL);
	struct WBStartup *startup = NULL;
	char *err = "";

	if (!me->pr_CLI) {
		WaitPort(&me->pr_MsgPort);
		startup = (struct WBStartup *)GetMsg(&me->pr_MsgPort);
	}
	if ((DOSBase = (struct DosLibrary *)OpenLibrary(DOSNAME, 36)) != NULL) {
		if ((err = setup()) == NULL)
			run();
		cleanup(err);
		CloseLibrary((struct Library *)DOSBase);
	}
	if (startup) {
		Forbid();
		ReplyMsg((struct Message *)startup);
	}
	return err ? RETURN_FAIL : RETURN_OK;
}
