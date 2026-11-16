/*
 * diskfont.library from LIBS: with the fonts of SYS:Fonts: AvailFonts'
 * list; each font loaded, timed and drawn on RAM screens in the mono,
 * colour and 1.x looks; a scaled size; a screen and a window in a disk
 * font; the asl font requester picking one; the unused fonts freed when
 * memory runs out.
 */
#include <exec/exec.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <devices/timer.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <graphics/gfxdriver.h>
#include <graphics/rastport.h>
#include <graphics/text.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <intuition/preferences.h>
#include <libraries/asl.h>
#include <libraries/diskfont.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/asl.h>
#include <proto/diskfont.h>
#include <proto/timer.h>
#include <proto/alib.h>

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct DosLibrary *DOSBase;
struct Library *AslBase;
struct Library *DiskfontBase;
struct Device *TimerBase;

static int fails;
static char *look = "setup";

static void check(int ok, char *what)
{
	if (!ok) {
		xKPrintf("FAIL diskfont: %s: %s\n", look, what);
		fails++;
	}
}

static int strcmp(const char *a, const char *b)
{
	while (*a && *a == *b)
		a++, b++;
	return (UBYTE)*a - (UBYTE)*b;
}

/* The shipped fonts */
#define NFONTS	4
static struct TextAttr attrs[NFONTS] = {
	{ "fixed.font", 8, 0, 0 }, { "fixed.font", 11, 0, 0 },
	{ "fixed.font", 16, 0, 0 }, { "sans.font", 9, 0, 0 }
};
static struct TextFont *fonts[NFONTS];

static struct EClockVal t0;
static ULONG rate;

static void start(void)
{
	rate = ReadEClock(&t0);
}

/* Microseconds since start() */
static ULONG took(void)
{
	struct EClockVal t1;

	ReadEClock(&t1);
	return (t1.ev_lo - t0.ev_lo) * 1000 / (rate / 1000);
}

/* AvailFonts(): the four on disk and topaz in memory; the shortage */
static void avail(void)
{
	static UBYTE buf[600];
	struct AvailFontsHeader *afh = (struct AvailFontsHeader *)buf;
	struct AvailFonts *af = (struct AvailFonts *)(afh + 1);
	struct TAvailFonts *taf = (struct TAvailFonts *)(afh + 1);
	WORD i, j, found = 0;
	LONG need;
	ULONG us;

	start();
	need = AvailFonts((STRPTR)buf, sizeof(buf), AFF_DISK);
	us = took();
	xKPrintf("diskfont: AvailFonts of FONTS: %ld us\n", us);
	check(need == 0 && afh->afh_NumEntries == NFONTS, "AvailFonts disk entries");
	for (i = 0; i < afh->afh_NumEntries; i++)
		for (j = 0; j < NFONTS; j++)
			if (strcmp(af[i].af_Attr.ta_Name, attrs[j].ta_Name) == 0 &&
			    af[i].af_Attr.ta_YSize == attrs[j].ta_YSize &&
			    af[i].af_Type == AFF_DISK &&
			    (af[i].af_Attr.ta_Flags & (FPF_DISKFONT | FPF_DESIGNED)) ==
			    (FPF_DISKFONT | FPF_DESIGNED) &&
			    !(af[i].af_Attr.ta_Flags & FPF_PROPORTIONAL) == (j < 3))
				found |= 1 << j;
	check(found == (1 << NFONTS) - 1, "AvailFonts disk fonts");

	/* too small: the shortage, then enough */
	need = AvailFonts((STRPTR)buf, 20, AFF_DISK);
	check(need > 0 && AvailFonts((STRPTR)buf, 20 + need, AFF_DISK) == 0 &&
	    afh->afh_NumEntries == NFONTS, "AvailFonts shortage");

	check(AvailFonts((STRPTR)buf, sizeof(buf), AFF_MEMORY) == 0 &&
	    afh->afh_NumEntries >= 1 && strcmp(af[0].af_Attr.ta_Name,
	    "topaz.font") == 0 && af[0].af_Type == AFF_MEMORY, "AvailFonts memory");

	check(AvailFonts((STRPTR)buf, sizeof(buf), AFF_DISK | AFF_TAGGED) == 0 &&
	    afh->afh_NumEntries == NFONTS && taf[0].taf_Attr.tta_Tags == NULL &&
	    taf[NFONTS - 1].taf_Attr.tta_YSize != 0, "AvailFonts tagged");
}

/* The rows of 'A' as the sources draw it, bit 7 leftmost */
static UBYTE fixedA[8] = { 0x30, 0x78, 0xCC, 0xCC, 0xFC, 0xCC, 0xCC, 0x00 };
static UBYTE sansA[9] = { 0x20, 0x50, 0x88, 0x88, 0xF8, 0x88, 0x88, 0, 0 };

static UWORD glyph(struct TextFont *tf, UBYTE c)
{
	if (c < tf->tf_LoChar || c > tf->tf_HiChar)
		return tf->tf_HiChar - tf->tf_LoChar + 1;
	return c - tf->tf_LoChar;
}

/* Whether pixel x of row y of c's glyph is set */
static BOOL bit(struct TextFont *tf, UBYTE c, WORD x, WORD y)
{
	ULONG loc = ((ULONG *)tf->tf_CharLoc)[glyph(tf, c)];
	UWORD pos = (UWORD)(loc >> 16);

	if (x < 0 || x >= (WORD)(loc & 0xFFFF))
		return FALSE;
	pos += x;
	return (((UBYTE *)tf->tf_CharData)[(ULONG)y * tf->tf_Modulo +
	    (pos >> 3)] >> (7 - (pos & 7))) & 1;
}

static WORD advance(struct TextFont *tf, UBYTE c)
{
	return tf->tf_CharSpace ? ((WORD *)tf->tf_CharSpace)[glyph(tf, c)] :
	    tf->tf_XSize;
}

/* Loads each font: its size, flags, 'A', the time and memory taken */
static void load(void)
{
	struct TextFont *tf;
	ULONG us, mem;
	WORD i, y;
	BOOL ok;

	for (i = 0; i < NFONTS; i++) {
		mem = AvailMem(MEMF_ANY);
		start();
		tf = fonts[i] = OpenDiskFont(&attrs[i]);
		us = took();
		mem -= AvailMem(MEMF_ANY);
		check(tf != NULL, attrs[i].ta_Name);
		if (tf == NULL)
			continue;
		xKPrintf("diskfont: %s %d: %ld us, %ld bytes\n",
		    attrs[i].ta_Name, attrs[i].ta_YSize, us, mem);
		check(tf->tf_YSize == attrs[i].ta_YSize &&
		    strcmp(tf->tf_Message.mn_Node.ln_Name, attrs[i].ta_Name) == 0 &&
		    (tf->tf_Flags & FPF_DISKFONT) && tf->tf_Accessors == 1 &&
		    tf->tf_LoChar == 0x20 && tf->tf_HiChar == 0x7E, "font fields");
		ok = TRUE;
		for (y = 0; y < 8; y++) {
			UBYTE r = 0, x;

			for (x = 0; x < 8; x++)
				if (bit(tf, 'A', x, y))
					r |= 0x80 >> x;
			if (i == 0 && r != fixedA[y])
				ok = FALSE;
			if (i == 3 && r != sansA[y])
				ok = FALSE;
		}
		check(ok, "glyph A");
		check(i < 3 ? tf->tf_XSize == 8 && tf->tf_CharSpace == NULL :
		    (tf->tf_Flags & FPF_PROPORTIONAL) && tf->tf_CharSpace &&
		    advance(tf, 'i') == 2 && advance(tf, 'M') == 6, "widths");
	}

	/* again: the same font, from memory */
	start();
	tf = OpenDiskFont(&attrs[0]);
	us = took();
	xKPrintf("diskfont: fixed.font 8 again: %ld us\n", us);
	check(tf == fonts[0] && tf->tf_Accessors == 2, "reopen");
	CloseFont(tf);
}

/* Expected pixels of a text, a byte a pixel */
#define EXPW	200
#define EXPH	24
static UBYTE expect[EXPH][EXPW];

/* Draws s in tf with soft style st at x, y (its top) and compares */
static void drawn(struct RastPort *rp, struct TextFont *tf, char *s, UWORD st,
    UBYTE pen, WORD x0, WORD y0, char *what)
{
	WORD x, y, w, n, bad = 0, smear = st & FSF_BOLD ? tf->tf_BoldSmear : 0;
	char *p;

	for (y = 0; y < EXPH; y++)
		for (x = 0; x < EXPW; x++)
			expect[y][x] = 0;
	for (w = 0, p = s; *p; w += advance(tf, *p), p++)
		for (y = 0; y < tf->tf_YSize; y++)
			for (x = 0; x < advance(tf, *p); x++)
				if (bit(tf, *p, x, y)) {
					expect[y][w + x] = 1;
					expect[y][w + x + smear] = 1;
				}

	SetAPen(rp, 0);
	RectFill(rp, x0, y0, x0 + EXPW - 1, y0 + EXPH - 1);
	SetFont(rp, tf);
	SetSoftStyle(rp, st, FSF_BOLD);
	SetAPen(rp, pen);
	SetDrMd(rp, JAM1);
	for (n = 0, p = s; *p; p++)
		n++;
	check(TextLength(rp, s, n) == w, "TextLength");
	Move(rp, x0, y0 + tf->tf_Baseline);
	Text(rp, s, n);
	check(rp->cp_x == x0 + w, "Text advance");
	for (y = 0; y < EXPH; y++)
		for (x = 0; x < EXPW; x++)
			bad += (ReadPixel(rp, x0 + x, y0 + y) == pen) != expect[y][x];
	SetSoftStyle(rp, 0, FSF_BOLD);
	check(bad == 0, what);
}

static char sample[] = "AgW!jy{}";

/* Each font, and one bold, on a screen */
static void drawall(struct Screen *scr, UBYTE pen)
{
	struct RastPort *rp = &scr->RastPort;
	WORD i;

	for (i = 0; i < NFONTS; i++)
		if (fonts[i])
			drawn(rp, fonts[i], sample, 0, pen, 20, 40 + 30 * i,
			    "text pixels");
	if (fonts[3])
		drawn(rp, fonts[3], sample, FSF_BOLD, pen, 260, 40, "bold text");
	if (fonts[1])
		drawn(rp, fonts[1], sample, FSF_BOLD, pen, 260, 70, "bold fixed");
}

/* fixed 24 from 16, its glyphs the source's at its pixels' centres */
static void scaled(struct Screen *scr, UBYTE pen)
{
	static struct TextAttr big = { "fixed.font", 24, 0, 0 };
	static struct TextAttr designed = { "fixed.font", 24, 0, FPF_DESIGNED };
	static struct TextAttr sans18 = { "sans.font", 18, 0, 0 };
	struct DiskFontHeader *dfh;
	struct TextFont *tf, *src = fonts[2];
	WORD x, y, bad = 0;
	ULONG us;

	if (src == NULL)
		return;
	start();
	tf = OpenDiskFont(&big);
	us = took();
	check(tf != NULL, "scaled font");
	if (tf == NULL)
		return;
	xKPrintf("diskfont: fixed.font 24 scaled: %ld us\n", us);
	check(tf->tf_YSize == 24 && tf->tf_XSize == 12 &&
	    !(tf->tf_Flags & (FPF_DESIGNED | FPF_DISKFONT)), "scaled fields");
	for (y = 0; y < 24; y++)
		for (x = 0; x < 12; x++)
			bad += bit(tf, 'A', x, y) != bit(src, 'A', (2 * x + 1) * 8 / 24,
			    (2 * y + 1) * 16 / 48);
	check(bad == 0, "scaled glyph");
	drawn(&scr->RastPort, tf, "Ag", 0, pen, 260, 100, "scaled text");
	CloseFont(tf);

	tf = OpenDiskFont(&designed);
	check(tf == src, "designed size not scaled");
	CloseFont(tf);

	/* NewScaledDiskFont() itself, the font added and removed */
	start();
	dfh = fonts[3] ? NewScaledDiskFont(fonts[3], &sans18) : NULL;
	xKPrintf("diskfont: sans.font 18 made: %ld us\n", took());
	check(dfh != NULL && dfh->dfh_TF.tf_YSize == 18 &&
	    dfh->dfh_TF.tf_CharSpace != NULL &&
	    ((WORD *)dfh->dfh_TF.tf_CharSpace)['M' - 0x20] == 12, "NewScaledDiskFont");
	if (dfh) {
		AddFont(&dfh->dfh_TF);
		tf = OpenFont(&sans18);
		check(tf == &dfh->dfh_TF, "AddFont");
		CloseFont(tf);
		RemFont(&dfh->dfh_TF);
		tf = OpenFont(&sans18);
		check(tf != &dfh->dfh_TF, "RemFont");
		CloseFont(tf);
		UnLoadSeg(dfh->dfh_Segment);
	}
}

/* A screen in sans 9 and a window on it */
static void fontscreen(void)
{
	struct TagItem t[6];
	struct Screen *scr;
	struct Window *w;

	t[0].ti_Tag = SA_Width;		t[0].ti_Data = 640;
	t[1].ti_Tag = SA_Height;	t[1].ti_Data = 400;
	t[2].ti_Tag = SA_Depth;		t[2].ti_Data = 1;
	t[3].ti_Tag = SA_Title;		t[3].ti_Data = (IPTR)"Sans";
	t[4].ti_Tag = SA_Font;		t[4].ti_Data = (IPTR)&attrs[3];
	t[5].ti_Tag = TAG_DONE;
	scr = OpenScreenTagList(NULL, t);
	check(scr != NULL, "sans screen");
	if (scr == NULL)
		return;
	check(scr->RastPort.Font == fonts[3] && scr->Font->ta_YSize == 9 &&
	    strcmp(scr->Font->ta_Name, "sans.font") == 0 &&
	    scr->BarHeight >= 9, "screen font");
	t[0].ti_Tag = WA_CustomScreen;	t[0].ti_Data = (IPTR)scr;
	t[1].ti_Tag = WA_Title;		t[1].ti_Data = (IPTR)"Window";
	t[2].ti_Tag = WA_Width;		t[2].ti_Data = 200;
	t[3].ti_Tag = WA_Height;	t[3].ti_Data = 100;
	t[4].ti_Tag = TAG_DONE;
	w = OpenWindowTagList(NULL, t);
	check(w != NULL, "window");
	if (w) {
		check(w->RPort->Font == fonts[3] && w->IFont == fonts[3] &&
		    w->BorderTop >= 9, "window font");
		CloseWindow(w);
	}
	CloseScreen(scr);
}

/* Events through input.device, which runs intuition's handler */
static struct MsgPort *port;
static struct IOStdReq *io;
static struct InputEvent ev;
static struct Task *proc;

static void event(UBYTE class, UWORD code, WORD x, WORD y)
{
	ev.ie_NextEvent = NULL;
	ev.ie_Class = class;
	ev.ie_SubClass = 0;
	ev.ie_Code = code;
	ev.ie_Qualifier = 0;
	ev.ie_X = x;
	ev.ie_Y = y;
	io->io_Command = IND_WRITEEVENT;
	io->io_Data = &ev;
	io->io_Length = sizeof(ev);
	DoIO((struct IORequest *)io);
}

/* Until the requester's process waits for input, 2 s at most */
static void pause(void)
{
	int i, n = 0;

	for (i = 0; i < 100 && n < 2; i++) {
		TimeDelay(UNIT_VBLANK, 0, 20000);
		n = proc->tc_State == TS_WAIT ? n + 1 : 0;
	}
}

static void click(WORD x, WORD y)
{
	event(IECLASS_RAWMOUSE, IECODE_NOBUTTON, x, y);
	event(IECLASS_RAWMOUSE, IECODE_LBUTTON, x, y);
	event(IECLASS_RAWMOUSE, IECODE_LBUTTON | IECODE_UP_PREFIX, x, y);
	pause();
}

/* The requester's process: runs AslRequest(), then signals the test's */
static struct Task *maintask, *roottask;
static APTR jobreq;
static struct TagItem jobtags[3];
static BOOL jobres;

static LONG run(char *args, LONG len)
{
	for (;;) {
		Wait(SIGBREAKF_CTRL_E);
		if (jobreq == NULL)
			break;
		jobres = AslRequest(jobreq, jobtags);
		Signal(maintask, SIGBREAKF_CTRL_F);
	}
	Signal(maintask, SIGBREAKF_CTRL_F);
	return 0;
}

static struct Gadget *gadget(struct Window *w, UWORD id)
{
	struct Gadget *g;

	for (g = w->FirstGadget; g; g = g->NextGadget)
		if (g->GadgetID == id && !(g->GadgetType & GTYP_SYSGADGET))
			return g;
	return NULL;
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

/* The font requester's gadgets */
#define NAMES	1
#define SIZES	2
#define OK	102

/* Row n of a listview, rows of the screen font plus 2 under a 4 pixel frame */
static void row(struct Screen *scr, struct Window *w, UWORD id, WORD n)
{
	struct Gadget *g = gadget(w, id);
	WORD b[4], h = scr->Font->ta_YSize + 2;

	check(g != NULL, "listview");
	if (g == NULL)
		return;
	gbox(w, g, b);
	click(b[0] + 20, b[1] + 4 + n * h + h / 2);
}

/* The asl font requester: fixed (before sans and topaz) at its second size */
static void fontreq(struct Screen *scr)
{
	struct FontRequester *fo;
	struct Window *w = NULL, *f;
	struct Gadget *g;
	WORD b[4];
	int i;

	fo = AllocAslRequest(ASL_FontRequest, NULL);
	check(fo != NULL, "font AllocAslRequest");
	if (fo == NULL)
		return;
	jobreq = fo;
	jobtags[0].ti_Tag = ASLFO_Screen;	jobtags[0].ti_Data = (IPTR)scr;
	jobtags[1].ti_Tag = ASLFO_TitleText;	jobtags[1].ti_Data = (IPTR)"Font";
	jobtags[2].ti_Tag = TAG_DONE;
	jobres = 99;
	SetSignal(0L, SIGBREAKF_CTRL_F);
	Signal(proc, SIGBREAKF_CTRL_E);

	for (i = 0; i < 100 && w == NULL; i++) {
		TimeDelay(UNIT_VBLANK, 0, 20000);
		Forbid();
		for (f = scr->FirstWindow; f; f = f->NextWindow)
			if (f->Title && strcmp((char *)f->Title, "Font") == 0 &&
			    gadget(f, OK))
				w = f;
		Permit();
	}
	check(w != NULL, "font requester window");
	if (w) {
		pause();
		row(scr, w, NAMES, 0);
		row(scr, w, SIZES, 1);
		if ((g = gadget(w, OK)) != NULL) {
			gbox(w, g, b);
			click(b[0] + b[2] / 2, b[1] + b[3] / 2);
		}
	}
	for (i = 0; i < 500; i++) {
		if (SetSignal(0L, SIGBREAKF_CTRL_F) & SIGBREAKF_CTRL_F)
			break;
		TimeDelay(UNIT_VBLANK, 0, 20000);
	}
	if (i == 500) {
		AbortAslRequest(fo);
		Wait(SIGBREAKF_CTRL_F);
	}
	check(jobres == TRUE && strcmp(fo->fo_Attr.ta_Name, "fixed.font") == 0 &&
	    fo->fo_Attr.ta_YSize == 11, "font requester picks fixed 11");
	FreeAslRequest(fo);
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

static void colourlook(char *name, UBYTE pen)
{
	struct BitMap *bm;
	struct Screen *scr;

	bm = AllocBitMap(640, 400, 4, BMF_CLEAR | BMF_RTGTAGS | BMF_RTGCHECK |
	    BMF_FRIENDISTAG, (struct BitMap *)chunkytags);
	look = name;
	check(bm != NULL, "chunky bitmap");
	if (bm == NULL)
		return;
	scr = openscreen(640, 400, 4, bm, name);
	check(scr != NULL, "screen");
	if (scr) {
		drawall(scr, pen);
		CloseScreen(scr);
	}
	FreeBitMap(bm);
}

/* No font of diskfont's in the list */
static BOOL nodiskfonts(void)
{
	struct TextFont *tf;
	BOOL none = TRUE;

	Forbid();
	for (tf = (struct TextFont *)GfxBase->TextFonts.lh_Head;
	    tf->tf_Message.mn_Node.ln_Succ;
	    tf = (struct TextFont *)tf->tf_Message.mn_Node.ln_Succ)
		if (tf->tf_Message.mn_Node.ln_Name[0] != 't')
			none = FALSE;
	Permit();
	return none;
}

/* The test, in a process for dos.library's calls */
static LONG test(char *args, LONG len)
{
	static struct Preferences saved, prefs;
	struct Screen *scr;
	struct TagItem t[4];
	ULONG mem, big;
	APTR p;
	WORD i;

	check(AssignLate("FONTS", "SYS:Fonts"), "FONTS: assign");
	port = CreateMsgPort();
	io = (struct IOStdReq *)CreateIORequest(port, sizeof(struct IOStdReq));
	check(io != NULL && OpenDevice("input.device", 0,
	    (struct IORequest *)io, 0) == 0, "input.device");

	avail();
	load();

	maintask = FindTask(NULL);
	t[0].ti_Tag = NP_Entry;		t[0].ti_Data = (ULONG)run;
	t[1].ti_Tag = NP_Name;		t[1].ti_Data = (ULONG)"FontReq";
	t[2].ti_Tag = NP_StackSize;	t[2].ti_Data = 8192;
	t[3].ti_Tag = TAG_DONE;
	proc = (struct Task *)CreateNewProc(t);
	check(proc != NULL, "CreateNewProc");

	look = "mono";
	scr = openscreen(1024, 800, 1, NULL, "Mono");
	check(scr != NULL, "screen");
	if (scr) {
		drawall(scr, 1);
		scaled(scr, 1);
		if (proc)
			fontreq(scr);
		CloseScreen(scr);
	}
	colourlook("colour", 3);
	GetPrefs(&saved, sizeof(saved));
	prefs = saved;
	prefs.EnableCLI |= OLD_LOOK;
	SetPrefs(&prefs, sizeof(prefs), FALSE);
	colourlook("1.x", 2);
	SetPrefs(&saved, sizeof(saved), FALSE);

	look = "screen";
	fontscreen();

	/* unused, the fonts go when memory runs out */
	look = "flush";
	for (i = 0; i < NFONTS; i++)
		CloseFont(fonts[i]);
	mem = AvailMem(MEMF_ANY);
	big = AvailMem(MEMF_ANY | MEMF_LARGEST) + 8;
	if ((p = AllocMem(big, MEMF_ANY)) != NULL)
		FreeMem(p, big);
	xKPrintf("diskfont: flushed fonts gave %ld bytes\n", AvailMem(MEMF_ANY) - mem);
	check(nodiskfonts(), "fonts freed");
	fonts[0] = OpenDiskFont(&attrs[0]);
	check(fonts[0] != NULL && fonts[0]->tf_YSize == 8, "reload");
	CloseFont(fonts[0]);

	if (proc) {
		jobreq = NULL;
		SetSignal(0L, SIGBREAKF_CTRL_F);
		Signal(proc, SIGBREAKF_CTRL_E);
		Wait(SIGBREAKF_CTRL_F);
	}
	CloseDevice((struct IORequest *)io);
	Signal(roottask, SIGBREAKF_CTRL_F);
	return 0;
}

int main(void)
{
	struct timerequest *tr;
	struct TagItem t[4];

	tr = (struct timerequest *)CreateIORequest(CreateMsgPort(), sizeof(*tr));
	if (tr == NULL ||
	    OpenDevice(TIMERNAME, UNIT_ECLOCK, (struct IORequest *)tr, 0) != 0) {
		KPutStr("FAIL diskfont: devices\n");
		return 1;
	}
	TimerBase = tr->tr_node.io_Device;
	DOSBase = (struct DosLibrary *)OpenLibrary(DOSNAME, 36L);
	GfxBase = (struct GfxBase *)OpenLibrary(GRAPHICSNAME, 39L);
	IntuitionBase = (struct IntuitionBase *)OpenLibrary(INTUITIONNAME, 39L);
	DiskfontBase = OpenLibrary("diskfont.library", 50L);
	AslBase = OpenLibrary(AslName, 50L);
	if (DOSBase == NULL || GfxBase == NULL || IntuitionBase == NULL ||
	    DiskfontBase == NULL || AslBase == NULL) {
		KPutStr("FAIL diskfont: OpenLibrary\n");
		return 1;
	}

	roottask = FindTask(NULL);
	t[0].ti_Tag = NP_Entry;		t[0].ti_Data = (ULONG)test;
	t[1].ti_Tag = NP_Name;		t[1].ti_Data = (ULONG)"DiskfontTest";
	t[2].ti_Tag = NP_StackSize;	t[2].ti_Data = 8192;
	t[3].ti_Tag = TAG_DONE;
	if (CreateNewProc(t) == NULL) {
		KPutStr("FAIL diskfont: CreateNewProc\n");
		return 1;
	}
	Wait(SIGBREAKF_CTRL_F);

	CloseLibrary(AslBase);
	CloseLibrary(DiskfontBase);
	CloseLibrary((struct Library *)IntuitionBase);
	CloseLibrary((struct Library *)GfxBase);
	CloseLibrary((struct Library *)DOSBase);
	CloseDevice((struct IORequest *)tr);
	if (fails != 0)
		return 1;
	KPutStr("PASS diskfont\n");
	return 0;
}
