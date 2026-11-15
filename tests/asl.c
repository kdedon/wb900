/*
 * asl.library from LIBS: on screens in RAM bitmaps: mono 1024x800,
 * colour 640x400 and the 1.x look.  A process runs each requester while
 * the test drives it with events written to input.device: a file picked
 * in a drawer, a name typed in save mode, the parent drawer, the volumes
 * and Cancel, drawers only, multi-select, a font and a screen mode.
 */
#include <exec/exec.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <devices/rawkeycodes.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <graphics/gfxdriver.h>
#include <graphics/rastport.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <intuition/sghooks.h>
#include <intuition/preferences.h>
#include <libraries/asl.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/asl.h>
#include <proto/alib.h>

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct DosLibrary *DOSBase;
struct Library *AslBase;

static int fails;
static char *look;

static void check(int ok, char *what)
{
	if (!ok) {
		xKPrintf("FAIL asl: %s: %s\n", look, what);
		fails++;
	}
}

static int strcmp(const char *a, const char *b)
{
	while (*a && *a == *b)
		a++, b++;
	return (UBYTE)*a - (UBYTE)*b;
}

static int strlen(const char *s)
{
	const char *p = s;

	while (*p)
		p++;
	return p - s;
}

/* Events through input.device, which runs intuition's handler */
static struct MsgPort *port;
static struct IOStdReq *io;
static struct InputEvent ev;

static void event(UBYTE class, UWORD code, UWORD qual, WORD x, WORD y)
{
	ev.ie_NextEvent = NULL;
	ev.ie_Class = class;
	ev.ie_SubClass = 0;
	ev.ie_Code = code;
	ev.ie_Qualifier = qual;
	ev.ie_X = x;
	ev.ie_Y = y;
	io->io_Command = IND_WRITEEVENT;
	io->io_Data = &ev;
	io->io_Length = sizeof(ev);
	DoIO((struct IORequest *)io);
}

static struct Task *proc;

/* Until the requester's process waits for input, 2 s at most */
static void pause(void)
{
	int i, n = 0;

	for (i = 0; i < 100 && n < 2; i++) {
		TimeDelay(UNIT_VBLANK, 0, 20000);
		n = proc->tc_State == TS_WAIT ? n + 1 : 0;
	}
}

static void click(WORD x, WORD y, UWORD qual)
{
	event(IECLASS_RAWMOUSE, IECODE_NOBUTTON, qual, x, y);
	event(IECLASS_RAWMOUSE, IECODE_LBUTTON, qual, x, y);
	event(IECLASS_RAWMOUSE, IECODE_LBUTTON | IECODE_UP_PREFIX, qual, x, y);
	pause();
}

/* Raw key codes of the letters a to z */
static UBYTE letters[26] = {
	0x20, 0x35, 0x33, 0x22, 0x12, 0x23, 0x24, 0x25, 0x17, 0x26, 0x27,
	0x28, 0x37, 0x36, 0x18, 0x19, 0x10, 0x13, 0x21, 0x14, 0x16, 0x34,
	0x11, 0x32, 0x15, 0x31
};

/* The file gadget ends on each change, so one key at a time */
static void type(char *s)
{
	UWORD c;

	for (; *s; s++) {
		c = *s == '.' ? 0x39 : *s == '\n' ? RAWKEY_RETURN :
		    letters[*s - 'a'];
		event(IECLASS_RAWKEY, c, 0, 0, 0);
		event(IECLASS_RAWKEY, c | IECODE_UP_PREFIX, 0, 0, 0);
		pause();
	}
}

static struct Screen *scr;
static struct DrawInfo *dri;

#define PEN(p)	(dri->dri_Pens[p])

/* The requester's process: runs job's AslRequest(), then signals main */
static struct Task *maintask;
static APTR jobreq;
static struct TagItem *jobtags;
static BOOL jobres;
static WORD jobstack;

/* The task's stack: the pool clears a new stack; count above the last zero */
static WORD stackused(struct Task *t)
{
	UBYTE *p = (UBYTE *)t->tc_SPLower + 2;

	while (p < (UBYTE *)t->tc_SPUpper && *p == 0)
		p++;
	return (WORD)((UBYTE *)t->tc_SPUpper - p);
}

static LONG run(char *args, LONG len)
{
	for (;;) {
		Wait(SIGBREAKF_CTRL_E);
		if (jobreq == NULL)
			break;
		jobres = AslRequest(jobreq, jobtags);
		Signal(maintask, SIGBREAKF_CTRL_F);
	}
	jobstack = stackused(FindTask(NULL));
	Signal(maintask, SIGBREAKF_CTRL_F);
	return 0;
}

static void start(APTR req, struct TagItem *tags)
{
	jobreq = req;
	jobtags = tags;
	jobres = 99;
	SetSignal(0L, SIGBREAKF_CTRL_F);
	Signal(proc, SIGBREAKF_CTRL_E);
}

/* Wait up to 10 s for the requester to return; abort it after that */
static BOOL finish(void)
{
	int i;

	for (i = 0; i < 500; i++) {
		if (SetSignal(0L, SIGBREAKF_CTRL_F) & SIGBREAKF_CTRL_F)
			return TRUE;
		TimeDelay(UNIT_VBLANK, 0, 20000);
	}
	AbortAslRequest(jobreq);
	Wait(SIGBREAKF_CTRL_F);
	return FALSE;
}

static struct Gadget *gadget(struct Window *w, UWORD id)
{
	struct Gadget *g;

	for (g = w->FirstGadget; g; g = g->NextGadget)
		if (g->GadgetID == id && !(g->GadgetType & GTYP_SYSGADGET))
			return g;
	return NULL;
}

/* The requester window of a title with its OK button, after 2 s at most */
static struct Window *waitwin(char *title)
{
	struct Window *w = NULL, *f;
	int i;

	for (i = 0; i < 100 && w == NULL; i++) {
		TimeDelay(UNIT_VBLANK, 0, 20000);
		Forbid();
		for (f = scr->FirstWindow; f; f = f->NextWindow)
			if (f->Title && strcmp((char *)f->Title, title) == 0 &&
			    gadget(f, 102))
				w = f;
		Permit();
	}
	check(w != NULL, title);
	if (w)
		pause();
	return w;
}

/* A gadget's box on the screen, relative positions resolved */
static void gbox(struct Window *w, struct Gadget *g, WORD *b)
{
	b[0] = w->LeftEdge + g->LeftEdge +
	    ((g->Flags & GFLG_RELRIGHT) ? w->Width - 1 : 0);
	b[1] = w->TopEdge + g->TopEdge +
	    ((g->Flags & GFLG_RELBOTTOM) ? w->Height - 1 : 0);
	b[2] = g->Width + ((g->Flags & GFLG_RELWIDTH) ? w->Width : 0);
	b[3] = g->Height + ((g->Flags & GFLG_RELHEIGHT) ? w->Height : 0);
}

static void press(struct Window *w, UWORD id)
{
	struct Gadget *g = gadget(w, id);
	WORD b[4];

	check(g != NULL, "gadget");
	if (g == NULL)
		return;
	gbox(w, g, b);
	click(b[0] + b[2] / 2, b[1] + b[3] / 2, 0);
}

/* Row n of a listview, rows of the screen font plus 2 under a 4 pixel frame */
static void row(struct Window *w, UWORD id, WORD n, UWORD qual)
{
	struct Gadget *g = gadget(w, id);
	WORD b[4], h = scr->Font->ta_YSize + 2;

	check(g != NULL, "listview");
	if (g == NULL)
		return;
	gbox(w, g, b);
	click(b[0] + 20, b[1] + 4 + n * h + h / 2, qual);
}

static char *text(struct Window *w, UWORD id)
{
	struct Gadget *g = gadget(w, id);

	return g ? (char *)((struct StringInfo *)g->SpecialInfo)->Buffer : "";
}

/* Wait up to 2 s for a string gadget to show s */
static BOOL waittext(struct Window *w, UWORD id, char *s)
{
	int i;

	for (i = 0; i < 100; i++) {
		if (strcmp(text(w, id), s) == 0) {
			pause();
			return TRUE;
		}
		TimeDelay(UNIT_VBLANK, 0, 20000);
	}
	return FALSE;
}

/* Pixels of pen p along a line of the screen */
static WORD count(UWORD p, WORD x0, WORD y0, WORD x1, WORD y1)
{
	WORD x, y, n = 0;

	for (y = y0; y <= y1; y++)
		for (x = x0; x <= x1; x++)
			n += ReadPixel(&scr->RastPort, x, y) == p;
	return n;
}

/* A raised button: light top edge, dark bottom edge, a label inside */
static void looks(struct Window *w, UWORD id, char *what)
{
	struct Gadget *g = gadget(w, id);
	WORD b[4];

	if (g == NULL)
		return;
	gbox(w, g, b);
	check(count(PEN(SHINEPEN), b[0] + 2, b[1], b[0] + b[2] - 3, b[1]) >
	    b[2] / 2 && count(PEN(SHADOWPEN), b[0] + 2, b[1] + b[3] - 1,
	    b[0] + b[2] - 3, b[1] + b[3] - 1) > b[2] / 2, what);
	check(count(PEN(TEXTPEN), b[0] + 4, b[1] + 2, b[0] + b[2] - 5,
	    b[1] + b[3] - 3) > 10, "button label");
}

/* Case-insensitive, as the requester sorts */
static int icmp(char *a, char *b)
{
	UBYTE ca, cb;

	for (;; a++, b++) {
		ca = *a >= 'A' && *a <= 'Z' ? *a + 32 : *a;
		cb = *b >= 'A' && *b <= 'Z' ? *b + 32 : *b;
		if (ca != cb || ca == 0)
			return ca - cb;
	}
}

/* The row of a name in a drawer's list: drawers first, then by name */
static WORD rowof(char *dir, char *name)
{
	struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
	BPTR lock = Lock(dir, ACCESS_READ);
	BOOL isdir = FALSE, found = FALSE;
	WORD n = 0, before = 0, dirs = 0;

	if (lock && fib && Examine(lock, fib)) {
		while (ExNext(lock, fib)) {
			if (icmp(fib->fib_FileName, name) == 0) {
				found = TRUE;
				isdir = fib->fib_DirEntryType > 0;
				continue;
			}
			dirs += fib->fib_DirEntryType > 0;
			n++;
		}
		UnLock(lock);
		lock = Lock(dir, ACCESS_READ);
		Examine(lock, fib);
		while (ExNext(lock, fib))
			if ((fib->fib_DirEntryType > 0) == isdir &&
			    icmp(fib->fib_FileName, name) < 0)
				before++;
	}
	if (lock)
		UnLock(lock);
	if (fib)
		FreeDosObject(DOS_FIB, fib);
	check(found, name);
	return isdir ? before : dirs + before;
}

static struct TagItem *tags3(ULONG t0, ULONG d0, ULONG t1, ULONG d1,
    ULONG t2, ULONG d2)
{
	static struct TagItem t[5];

	t[0].ti_Tag = ASLFR_Screen;	t[0].ti_Data = (ULONG)scr;
	t[1].ti_Tag = t0;		t[1].ti_Data = d0;
	t[2].ti_Tag = t1;		t[2].ti_Data = d1;
	t[3].ti_Tag = t2;		t[3].ti_Data = d2;
	t[4].ti_Tag = TAG_DONE;
	return t;
}

/* File requester gadget IDs */
#define LIST	1
#define DRAWER	2
#define FILE	4
#define OK	102
#define VOLUMES	103
#define PARENT	104
#define CANCEL	105

static void filereq(void)
{
	struct FileRequester *fr;
	struct Window *w;
	WORD demos, readme, notes;

	demos = rowof("SYS:", "Demos");
	readme = rowof("SYS:Demos", "ReadMe");
	notes = rowof("SYS:Demos", "Notes");

	fr = AllocAslRequest(ASL_FileRequest, NULL);
	check(fr != NULL, "AllocAslRequest");
	if (fr == NULL)
		return;

	/* A file in a drawer: click the drawer, the file, then OK */
	start(fr, tags3(ASLFR_TitleText, (ULONG)"Load",
	    ASLFR_InitialDrawer, (ULONG)"SYS:", TAG_IGNORE, 0));
	if ((w = waitwin("Load")) != NULL) {
		looks(w, OK, "OK button");
		looks(w, CANCEL, "Cancel button");
		row(w, LIST, demos, 0);
		check(waittext(w, DRAWER, "SYS:Demos"), "drawer entered");
		row(w, LIST, readme, 0);
		check(strcmp(text(w, FILE), "ReadMe") == 0, "file picked");
		press(w, OK);
	}
	check(finish() && jobres == TRUE, "load result");
	check(fr->fr_Drawer && strcmp(fr->fr_Drawer, "SYS:Demos") == 0 &&
	    fr->fr_File && strcmp(fr->fr_File, "ReadMe") == 0, "fr_Drawer/fr_File");

	/* Save mode: a name typed, then Return */
	start(fr, tags3(ASLFR_TitleText, (ULONG)"Save",
	    ASLFR_DoSaveMode, TRUE, ASLFR_InitialFile, (ULONG)""));
	if ((w = waitwin("Save")) != NULL) {
		struct Gadget *g = gadget(w, LIST);
		WORD b[4];

		/* the save mode's list is drawn inverted */
		gbox(w, g, b);
		check(count(PEN(TEXTPEN), b[0] + b[2] - 8, b[1] + b[3] - 7,
		    b[0] + b[2] - 6, b[1] + b[3] - 6) == 6, "save mode list");
		type("new.txt\n");
	}
	check(finish() && jobres == TRUE, "save result");
	check(strcmp(fr->fr_Drawer, "SYS:Demos") == 0 &&
	    strcmp(fr->fr_File, "new.txt") == 0, "saved name");

	/* The parent drawer */
	start(fr, tags3(ASLFR_TitleText, (ULONG)"Parent",
	    ASLFR_DoSaveMode, FALSE, TAG_IGNORE, 0));
	if ((w = waitwin("Parent")) != NULL) {
		press(w, PARENT);
		check(waittext(w, DRAWER, "SYS:"), "parent drawer");
		press(w, OK);
	}
	check(finish() && jobres == TRUE && strcmp(fr->fr_Drawer, "SYS:") == 0,
	    "parent result");

	/* The volumes, a volume entered, then Cancel */
	start(fr, tags3(ASLFR_TitleText, (ULONG)"Cancel",
	    TAG_IGNORE, 0, TAG_IGNORE, 0));
	if ((w = waitwin("Cancel")) != NULL) {
		press(w, VOLUMES);
		row(w, LIST, 0, 0);
		pause();
		check(text(w, DRAWER)[0] && text(w, DRAWER)[strlen(text(w,
		    DRAWER)) - 1] == ':', "volume entered");
		press(w, CANCEL);
	}
	check(finish() && jobres == FALSE, "cancel result");
	check(strcmp(fr->fr_Drawer, "SYS:") == 0, "fr_Drawer kept on cancel");
	FreeAslRequest(fr);

	/* Drawers only */
	fr = AllocAslRequest(ASL_FileRequest, tags3(ASLFR_DrawersOnly, TRUE,
	    ASLFR_InitialDrawer, (ULONG)"SYS:Demos", TAG_IGNORE, 0));
	start(fr, tags3(ASLFR_TitleText, (ULONG)"Drawer", TAG_IGNORE, 0,
	    TAG_IGNORE, 0));
	if ((w = waitwin("Drawer")) != NULL) {
		check(gadget(w, FILE) == NULL, "no file gadget");
		press(w, OK);
	}
	check(finish() && jobres == TRUE && strcmp(fr->fr_Drawer,
	    "SYS:Demos") == 0 && fr->fr_File[0] == 0, "drawers only result");
	FreeAslRequest(fr);

	/* Multi-select: one file, then another with Shift */
	fr = AllocAslRequest(ASL_FileRequest, tags3(ASLFR_DoMultiSelect, TRUE,
	    ASLFR_InitialDrawer, (ULONG)"SYS:Demos", TAG_IGNORE, 0));
	start(fr, tags3(ASLFR_TitleText, (ULONG)"Multi", TAG_IGNORE, 0,
	    TAG_IGNORE, 0));
	if ((w = waitwin("Multi")) != NULL) {
		row(w, LIST, notes, 0);
		event(IECLASS_RAWKEY, RAWKEY_LSHIFT, IEQUALIFIER_LSHIFT, 0, 0);
		row(w, LIST, readme, IEQUALIFIER_LSHIFT);
		event(IECLASS_RAWKEY, RAWKEY_LSHIFT | IECODE_UP_PREFIX, 0, 0, 0);
		press(w, OK);
	}
	check(finish() && jobres == TRUE && fr->fr_NumArgs == 2 &&
	    strcmp(fr->fr_ArgList[0].wa_Name, "Notes") == 0 &&
	    strcmp(fr->fr_ArgList[1].wa_Name, "ReadMe") == 0 &&
	    fr->fr_ArgList[0].wa_Lock != NULL, "multi-select result");
	FreeAslRequest(fr);
}

static void fontreq(void)
{
	struct FontRequester *fo;
	struct Window *w;

	fo = AllocAslRequest(ASL_FontRequest, NULL);
	check(fo != NULL, "font AllocAslRequest");
	if (fo == NULL)
		return;
	start(fo, tags3(ASLFO_TitleText, (ULONG)"Font", TAG_IGNORE, 0,
	    TAG_IGNORE, 0));
	if ((w = waitwin("Font")) != NULL) {
		looks(w, OK, "font OK button");
		row(w, 1, 0, 0);
		press(w, OK);
	}
	check(finish() && jobres == TRUE && strcmp(fo->fo_Attr.ta_Name,
	    "topaz.font") == 0 && fo->fo_Attr.ta_YSize == 8, "font result");
	FreeAslRequest(fo);
}

/* The HR modes, as the display database names them */
static struct DisplayMode modes[3];
static char *modenames[3] = { "HR:640x400 RGBI", "HR:1024x800 Mono",
	"HR:1024x1024 Mono" };
static WORD modesize[3][3] = { { 640, 400, 4 }, { 1024, 800, 1 },
	{ 1024, 1024, 1 } };

static void modereq(void)
{
	struct ScreenModeRequester *sm;
	struct Window *w;
	struct List list;
	WORD i;

	/* no display in the emulator: the database is empty */
	sm = AllocAslRequest(ASL_ScreenModeRequest, NULL);
	check(sm != NULL, "mode AllocAslRequest");
	if (sm == NULL)
		return;
	if (NextDisplayInfo(INVALID_ID) == INVALID_ID) {
		start(sm, tags3(ASLSM_TitleText, (ULONG)"None", TAG_IGNORE, 0,
		    TAG_IGNORE, 0));
		check(finish() && jobres == FALSE, "no modes");
	}

	NewList(&list);
	for (i = 0; i < 3; i++) {
		modes[i].dm_Node.ln_Name = modenames[i];
		modes[i].dm_DimensionInfo.Header.DisplayID = 0xFFFF0000 + i;
		modes[i].dm_DimensionInfo.MaxDepth = modesize[i][2];
		modes[i].dm_DimensionInfo.MinRasterWidth = 16;
		modes[i].dm_DimensionInfo.MinRasterHeight = 1;
		modes[i].dm_DimensionInfo.MaxRasterWidth = modesize[i][0];
		modes[i].dm_DimensionInfo.MaxRasterHeight = modesize[i][1];
		modes[i].dm_DimensionInfo.Nominal.MaxX = modesize[i][0] - 1;
		modes[i].dm_DimensionInfo.Nominal.MaxY = modesize[i][1] - 1;
		modes[i].dm_DimensionInfo.TxtOScan =
		    modes[i].dm_DimensionInfo.Nominal;
		modes[i].dm_PropertyFlags = DIPF_IS_WB;
		AddTail(&list, &modes[i].dm_Node);
	}
	/* sorted by name: 1024x1024, 1024x800, 640x400; pick 1024x800 */
	start(sm, tags3(ASLSM_TitleText, (ULONG)"Mode",
	    ASLSM_CustomSMList, (ULONG)&list, TAG_IGNORE, 0));
	if ((w = waitwin("Mode")) != NULL) {
		looks(w, OK, "mode OK button");
		row(w, 1, 1, 0);
		press(w, OK);
	}
	check(finish() && jobres == TRUE && sm->sm_DisplayID == 0xFFFF0001 &&
	    sm->sm_DisplayWidth == 1024 && sm->sm_DisplayHeight == 800 &&
	    sm->sm_DisplayDepth == 1, "mode result");

	/* the modes are back in the list for the next time */
	start(sm, tags3(ASLSM_TitleText, (ULONG)"Mode 2", TAG_IGNORE, 0,
	    TAG_IGNORE, 0));
	if ((w = waitwin("Mode 2")) != NULL) {
		row(w, 1, 2, 0);
		press(w, OK);
	}
	check(finish() && jobres == TRUE && sm->sm_DisplayID == 0xFFFF0000 &&
	    sm->sm_DisplayWidth == 640 && sm->sm_DisplayHeight == 400,
	    "second mode result");
	FreeAslRequest(sm);
	i = 0;
	while (RemHead(&list))
		i++;
	check(i == 3, "custom mode list kept");
}

static void requesters(char *name)
{
	look = name;
	dri = GetScreenDrawInfo(scr);
	filereq();
	fontreq();
	modereq();
	FreeScreenDrawInfo(scr, dri);
}

static struct TagItem chunkytags[] = {
	{ BMATags_GfxFormat, GDF_CHUNKY4 }, { TAG_DONE, 0 }
};

static struct Screen *openscreen(WORD w, WORD h, WORD d, struct BitMap *bm,
    char *title)
{
	struct TagItem t[7];

	t[0].ti_Tag = SA_Width;		t[0].ti_Data = w;
	t[1].ti_Tag = SA_Height;	t[1].ti_Data = h;
	t[2].ti_Tag = SA_Depth;		t[2].ti_Data = d;
	t[3].ti_Tag = SA_Title;		t[3].ti_Data = (IPTR)title;
	t[4].ti_Tag = SA_Type;		t[4].ti_Data = CUSTOMSCREEN;
	t[5].ti_Tag = bm ? SA_BitMap : TAG_IGNORE; t[5].ti_Data = (IPTR)bm;
	t[6].ti_Tag = TAG_DONE;
	return OpenScreenTagList(NULL, t);
}

static void colourlook(char *name)
{
	struct BitMap *bm;

	bm = AllocBitMap(640, 400, 4, BMF_CLEAR | BMF_RTGTAGS | BMF_RTGCHECK |
	    BMF_FRIENDISTAG, (struct BitMap *)chunkytags);
	look = name;
	check(bm != NULL, "chunky bitmap");
	if (bm == NULL)
		return;
	scr = openscreen(640, 400, 4, bm, name);
	check(scr != NULL, "screen");
	if (scr) {
		requesters(name);
		CloseScreen(scr);
	}
	FreeBitMap(bm);
}

int main(void)
{
	static struct Preferences saved, prefs;
	struct TagItem t[4];

	port = CreateMsgPort();
	io = (struct IOStdReq *)CreateIORequest(port, sizeof(struct IOStdReq));
	if (OpenDevice("input.device", 0, (struct IORequest *)io, 0) != 0) {
		KPutStr("FAIL asl: input.device\n");
		return 1;
	}
	DOSBase = (struct DosLibrary *)OpenLibrary(DOSNAME, 36L);
	GfxBase = (struct GfxBase *)OpenLibrary(GRAPHICSNAME, 39L);
	IntuitionBase = (struct IntuitionBase *)OpenLibrary(INTUITIONNAME, 39L);
	AslBase = OpenLibrary(AslName, 50L);
	if (DOSBase == NULL || GfxBase == NULL || IntuitionBase == NULL ||
	    AslBase == NULL) {
		KPutStr("FAIL asl: OpenLibrary\n");
		return 1;
	}

	maintask = FindTask(NULL);
	t[0].ti_Tag = NP_Entry;		t[0].ti_Data = (ULONG)run;
	t[1].ti_Tag = NP_Name;		t[1].ti_Data = (ULONG)"AslTest";
	t[2].ti_Tag = NP_StackSize;	t[2].ti_Data = 8192;
	t[3].ti_Tag = TAG_DONE;
	proc = (struct Task *)CreateNewProc(t);
	if (proc == NULL) {
		KPutStr("FAIL asl: CreateNewProc\n");
		return 1;
	}

	look = "mono";
	scr = openscreen(1024, 800, 1, NULL, "Mono");
	check(scr != NULL, "screen");
	if (scr) {
		requesters("mono");
		CloseScreen(scr);
	}

	colourlook("colour");

	GetPrefs(&saved, sizeof(saved));
	prefs = saved;
	prefs.color0 = 0x05A;
	prefs.color1 = 0xFFF;
	prefs.color2 = 0x002;
	prefs.color3 = 0xF80;
	prefs.EnableCLI |= OLD_LOOK;
	SetPrefs(&prefs, sizeof(prefs), FALSE);
	colourlook("1.x");
	SetPrefs(&saved, sizeof(saved), FALSE);

	/* the process's stack, then input.device's */
	jobreq = NULL;
	SetSignal(0L, SIGBREAKF_CTRL_F);
	Signal(proc, SIGBREAKF_CTRL_E);
	Wait(SIGBREAKF_CTRL_F);
	xKPrintf("asl: requester process stack %d of 8192 bytes used\n",
	    (int)jobstack);
	if ((proc = FindTask("input.device")) != NULL)
		xKPrintf("asl: input.device stack %d bytes used\n",
		    (int)stackused(proc));

	CloseLibrary(AslBase);
	CloseLibrary((struct Library *)IntuitionBase);
	CloseLibrary((struct Library *)GfxBase);
	CloseLibrary((struct Library *)DOSBase);
	CloseDevice((struct IORequest *)io);
	if (fails != 0)
		return 1;
	KPutStr("PASS asl\n");
	return 0;
}
