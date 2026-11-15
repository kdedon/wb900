/*
 * console.device from DEVS:, on windows of a screen in a RAM bitmap.
 * The root program writes text and control sequences to a standard and
 * a character-mapped unit and reads the character cells back by pixel,
 * reads the reports the console answers with, types keys through
 * input.device and reads them back, and resizes a window.
 */
#include <exec/exec.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <devices/rawkeycodes.h>
#include <devices/console.h>
#include <devices/conunit.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <graphics/rastport.h>
#include <graphics/text.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/alib.h>

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;

static int fails;

static void check(int ok, char *what)
{
	if (!ok) {
		xKPrintf("FAIL console: %s\n", what);
		fails++;
	}
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

/* Give the input handler and the console task a moment */
static void sync(void)
{
	event(IECLASS_NULL, 0, 0, 0, 0);
	TimeDelay(UNIT_VBLANK, 0, 40000);
}

static void key(UWORD code)
{
	event(IECLASS_RAWKEY, code, 0, 0, 0);
	event(IECLASS_RAWKEY, code | IECODE_UP_PREFIX, 0, 0, 0);
}

/* A console unit: a write request and a read request */
struct con {
	struct MsgPort *c_Port;
	struct IOStdReq *c_W, *c_R;
	struct ConUnit *c_Unit;
	struct Window *c_Win;
};

static BOOL conopen(struct con *c, struct Window *w, ULONG unit)
{
	c->c_Win = w;
	c->c_Port = CreateMsgPort();
	c->c_W = (struct IOStdReq *)CreateIORequest(c->c_Port,
	    sizeof(struct IOStdReq));
	c->c_R = (struct IOStdReq *)CreateIORequest(c->c_Port,
	    sizeof(struct IOStdReq));
	if (c->c_W == NULL || c->c_R == NULL)
		return FALSE;
	c->c_W->io_Data = (APTR)w;
	c->c_W->io_Length = sizeof(struct Window);
	if (OpenDevice(CONSOLENAME, unit, (struct IORequest *)c->c_W, 0) != 0)
		return FALSE;
	*c->c_R = *c->c_W;
	c->c_Unit = (struct ConUnit *)c->c_W->io_Unit;
	return TRUE;
}

static void conclose(struct con *c)
{
	CloseDevice((struct IORequest *)c->c_W);
	DeleteIORequest((struct IORequest *)c->c_R);
	DeleteIORequest((struct IORequest *)c->c_W);
	DeleteMsgPort(c->c_Port);
}

static void conwrite(struct con *c, char *s)
{
	c->c_W->io_Command = CMD_WRITE;
	c->c_W->io_Data = (APTR)s;
	c->c_W->io_Length = -1;
	DoIO((struct IORequest *)c->c_W);
}

/* What the console has for its reader within a moment, NUL-terminated */
static LONG conread(struct con *c, char *buf, LONG len)
{
	LONG n;

	c->c_R->io_Command = CMD_READ;
	c->c_R->io_Data = (APTR)buf;
	c->c_R->io_Length = len - 1;
	SendIO((struct IORequest *)c->c_R);
	for (n = 0; n < 20 && !CheckIO((struct IORequest *)c->c_R); n++)
		TimeDelay(UNIT_VBLANK, 0, 20000);
	if (!CheckIO((struct IORequest *)c->c_R)) {
		AbortIO((struct IORequest *)c->c_R);
		WaitIO((struct IORequest *)c->c_R);
		buf[0] = 0;
		return -1;
	}
	WaitIO((struct IORequest *)c->c_R);
	n = c->c_R->io_Actual;
	buf[n] = 0;
	return n;
}

static int streq(char *a, char *b)
{
	while (*a && *a == *b) {
		a++;
		b++;
	}
	return *a == *b;
}

/*
 * The cell at column x, row y against character ch drawn in pens fg on
 * bg (ch 0: a blank cell of pen bg).  Returns the number of pixels that
 * differ.
 */
static int cell(struct con *c, int x, int y, int ch, int fg, int bg)
{
	struct ConUnit *cu = c->c_Unit;
	struct RastPort *rp = c->c_Win->RPort;
	struct TextFont *tf = rp->Font;
	int px = cu->cu_XROrigin + x * cu->cu_XRSize;
	int py = cu->cu_YROrigin + y * cu->cu_YRSize;
	int i, j, bad = 0;
	UBYTE *glyph = NULL;
	ULONG loc = 0;

	if (ch != 0) {
		loc = ((ULONG *)tf->tf_CharLoc)[ch - tf->tf_LoChar] >> 16;
		glyph = (UBYTE *)tf->tf_CharData;
	}
	for (j = 0; j < cu->cu_YRSize; j++)
		for (i = 0; i < cu->cu_XRSize; i++) {
			int on = 0;

			if (glyph) {
				ULONG bit = loc + i;

				on = (glyph[j * tf->tf_Modulo + (bit >> 3)] >>
				    (7 - (bit & 7))) & 1;
			}
			if (ReadPixel(rp, px + i, py + j) != (on ? fg : bg))
				bad++;
		}
	return bad;
}

/* The row's text as the pixels show it, up to n cells */
static int rowis(struct con *c, int y, char *s, int fg, int bg)
{
	int x, bad = 0;

	for (x = 0; s[x]; x++)
		bad += cell(c, x, y, s[x] == ' ' ? 0 : s[x], fg, bg);
	return bad == 0;
}

static struct Screen *scr;
static UWORD textpen, bgpen;

static struct Window *openwin(int sizable)
{
	struct TagItem t[13];

	t[0].ti_Tag = WA_CustomScreen;	t[0].ti_Data = (IPTR)scr;
	t[1].ti_Tag = WA_Left;		t[1].ti_Data = 20;
	t[2].ti_Tag = WA_Top;		t[2].ti_Data = 20;
	t[3].ti_Tag = WA_Width;		t[3].ti_Data = 360;
	t[4].ti_Tag = WA_Height;	t[4].ti_Data = 120;
	t[5].ti_Tag = WA_Title;		t[5].ti_Data = (IPTR)"Console";
	t[6].ti_Tag = WA_DragBar;	t[6].ti_Data = TRUE;
	t[7].ti_Tag = WA_Activate;	t[7].ti_Data = TRUE;
	t[8].ti_Tag = WA_SizeGadget;	t[8].ti_Data = sizable;
	t[9].ti_Tag = WA_SizeBRight;	t[9].ti_Data = sizable;
	t[10].ti_Tag = WA_MaxWidth;	t[10].ti_Data = ~0;
	t[11].ti_Tag = WA_MaxHeight;	t[11].ti_Data = ~0;
	t[12].ti_Tag = TAG_DONE;
	return OpenWindowTagList(NULL, t);
}

/* CONU_STANDARD: text, the cursor, control sequences, reports, keys */
static void standard(void)
{
	struct Window *w;
	struct con c;
	struct ConUnit *cu;
	char buf[64];
	int i, n;

	w = openwin(FALSE);
	check(w != NULL, "OpenWindow");
	if (w == NULL)
		return;
	if (!conopen(&c, w, CONU_STANDARD)) {
		check(FALSE, "OpenDevice CONU_STANDARD");
		CloseWindow(w);
		return;
	}
	cu = c.c_Unit;
	/* the window becomes active in the input handler */
	sync();
	check(w->Flags & WFLG_WINDOWACTIVE, "window active");
	check(cu->cu_Window == w && cu->cu_XRSize == 8 && cu->cu_YRSize == 8,
	    "ConUnit cell size");
	check(cu->cu_XMax == (w->Width - w->BorderLeft - w->BorderRight) / 8 - 1
	    && cu->cu_YMax ==
	    (w->Height - w->BorderTop - w->BorderBottom) / 8 - 1,
	    "ConUnit size");
	check(cu->cu_XROrigin == w->BorderLeft &&
	    cu->cu_YROrigin == w->BorderTop, "ConUnit origin");

	/* text, then the cursor: the cell complemented */
	conwrite(&c, "Hello");
	check(rowis(&c, 0, "Hello", textpen, bgpen), "text");
	check(cu->cu_XCP == 5 && cu->cu_YCP == 0, "position after text");
	check(cell(&c, 5, 0, 0, 0, bgpen ^ 3) == 0, "cursor");

	/* line feed returns; tab */
	conwrite(&c, "\nab\tc");
	check(rowis(&c, 1, "ab      c", textpen, bgpen), "LF and HT");
	check(cell(&c, 5, 0, 0, 0, bgpen) == 0, "cursor gone");

	/* cursor position, then a character there */
	conwrite(&c, "\x9b" "3;10HX");
	check(cell(&c, 9, 2, 'X', textpen, bgpen) == 0, "CSI H");
	conwrite(&c, "\033[5;2H\033[7mI\033[0mJ");
	check(cell(&c, 1, 4, 'I', bgpen, textpen) == 0, "SGR 7");
	check(cell(&c, 2, 4, 'J', textpen, bgpen) == 0, "SGR 0");

	/* reports, as input */
	conwrite(&c, "\x9b" "6n");
	conread(&c, buf, sizeof(buf));
	check(streq(buf, "\x9b" "5;4R"), "device status report");
	conwrite(&c, "\x9b" "0 q");
	conread(&c, buf, sizeof(buf));
	xKPrintf("console: window status %ld cells wide\n",
	    (LONG)cu->cu_XMax + 1);
	check(buf[0] == (char)0x9b && buf[1] == '1' && buf[2] == ';' &&
	    buf[3] == '1' && buf[4] == ';', "window status request");

	/* insert and delete characters, erase the line, with no cursor */
	conwrite(&c, "\x9b" "0 p" "\x9b" "2;1H" "\x9b" "2@");
	check(rowis(&c, 1, "  ab", textpen, bgpen), "CSI @");
	conwrite(&c, "\x9b" "3P");
	check(rowis(&c, 1, "b      c ", textpen, bgpen), "CSI P");
	conwrite(&c, "\x9b" "K");
	check(rowis(&c, 1, "         ", textpen, bgpen), "CSI K");
	conwrite(&c, "\x9b" " p");
	check(cell(&c, 0, 1, 0, 0, bgpen ^ 3) == 0, "cursor shown");

	/* scrolling: one ScrollRaster per line past the bottom */
	conwrite(&c, "\x9b" "1;1H" "top\n");
	n = cu->cu_YMax;
	for (i = 1; i < n; i++)
		conwrite(&c, "\n");
	conwrite(&c, "last\nnext");
	check(rowis(&c, n - 1, "last", textpen, bgpen) &&
	    rowis(&c, n, "next", textpen, bgpen), "scroll up");
	check(cu->cu_YCP == n, "position at the bottom");
	conwrite(&c, "\x9b" "1;1H" "\x9b" "L");
	check(rowis(&c, n, "last", textpen, bgpen) &&
	    rowis(&c, 1, "    ", textpen, bgpen), "CSI L");
	conwrite(&c, "\x9b" "J");
	check(rowis(&c, n, "    ", textpen, bgpen), "CSI J");
	conwrite(&c, "\f");
	check(cu->cu_XCP == 0 && cu->cu_YCP == 0 &&
	    rowis(&c, n - 1, "    ", textpen, bgpen), "FF");

	/* a sequence split across writes */
	conwrite(&c, "\x9b" "4");
	conwrite(&c, ";3HZ");
	check(cell(&c, 2, 3, 'Z', textpen, bgpen) == 0, "split sequence");

	/* keys through input.device and the keymap */
	sync();
	key(0x20);
	key(RAWKEY_UP);
	sync();
	conread(&c, buf, sizeof(buf));
	check(streq(buf, "a" "\x9b" "A"), "keys");

	conclose(&c);
	CloseWindow(w);
}

/* CONU_SNIPMAP: the text is kept, and a bigger window shows it again */
static void charmap(void)
{
	struct Window *w;
	struct con c;
	struct ConUnit *cu;
	WORD xmax, ymax;

	w = openwin(TRUE);
	check(w != NULL, "OpenWindow sizable");
	if (w == NULL)
		return;
	if (!conopen(&c, w, CONU_SNIPMAP)) {
		check(FALSE, "OpenDevice CONU_SNIPMAP");
		CloseWindow(w);
		return;
	}
	cu = c.c_Unit;
	xmax = cu->cu_XMax;
	ymax = cu->cu_YMax;
	conwrite(&c, "Kept\nlines");
	check(rowis(&c, 0, "Kept", textpen, bgpen) &&
	    rowis(&c, 1, "lines", textpen, bgpen), "charmap text");

	SizeWindow(w, 80, 40);
	sync();
	sync();
	check(cu->cu_XMax == xmax + 10 && cu->cu_YMax == ymax + 5,
	    "resized unit");
	check(rowis(&c, 0, "Kept", textpen, bgpen) &&
	    rowis(&c, 1, "lines", textpen, bgpen), "charmap redrawn");
	check(cu->cu_XCP == 5 && cu->cu_YCP == 1, "position kept");

	conclose(&c);
	CloseWindow(w);
}

/* The task's stack: the pool clears a new stack; count above the last zero */
static void stackuse(char *name)
{
	struct Task *t;
	UBYTE *p, *top;

	t = FindTask(name);
	if (t == NULL)
		return;
	p = (UBYTE *)t->tc_SPLower + 2;
	top = (UBYTE *)t->tc_SPUpper;
	while (p < top && *p == 0)
		p++;
	xKPrintf("console: %s stack %ld of %ld bytes used\n", name,
	    (LONG)(top - p), (LONG)(top - (UBYTE *)t->tc_SPLower));
}

static struct TagItem scrtags[] = {
	{ SA_Width, 640 }, { SA_Height, 200 }, { SA_Depth, 2 },
	{ SA_Title, (IPTR)"Console Test" }, { TAG_DONE, 0 }
};

int main(void)
{
	struct DrawInfo *dri;

	port = CreateMsgPort();
	io = (struct IOStdReq *)CreateIORequest(port, sizeof(struct IOStdReq));
	if (OpenDevice("input.device", 0, (struct IORequest *)io, 0) != 0) {
		KPutStr("FAIL console: input.device\n");
		return 1;
	}
	GfxBase = (struct GfxBase *)OpenLibrary(GRAPHICSNAME, 39L);
	IntuitionBase = (struct IntuitionBase *)OpenLibrary(INTUITIONNAME, 39L);
	if (GfxBase == NULL || IntuitionBase == NULL) {
		KPutStr("FAIL console: OpenLibrary\n");
		return 1;
	}
	scr = OpenScreenTagList(NULL, scrtags);
	if (scr == NULL) {
		KPutStr("FAIL console: OpenScreen\n");
		return 1;
	}
	dri = GetScreenDrawInfo(scr);
	textpen = dri->dri_Pens[TEXTPEN];
	bgpen = dri->dri_Pens[BACKGROUNDPEN];
	FreeScreenDrawInfo(scr, dri);

	standard();
	charmap();

	stackuse(CONSOLENAME);
	stackuse("input.device");
	check(CloseScreen(scr), "CloseScreen");
	CloseLibrary((struct Library *)IntuitionBase);
	CloseLibrary((struct Library *)GfxBase);
	CloseDevice((struct IORequest *)io);
	if (fails != 0)
		return 1;
	KPutStr("PASS console\n");
	return 0;
}
