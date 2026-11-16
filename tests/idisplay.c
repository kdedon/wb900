/* SPDX-License-Identifier: MIT */
/*
 * intuition.library on hr.gfxdriver, behind a framebuf.resource of an HR
 * card with its VRAM in RAM, in the mono and RGBI modes: QueryOverscan(),
 * double buffering, where the buffer shown trades memory with VRAM, two
 * screens brought to the front in turn, each shown with exactly its
 * pixels and drawn into while behind, and an alert over the screen,
 * which leaves VRAM as it was.
 */
/* EMUFLAGS: --ram=08-1F */
#include <exec/exec.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <devices/timer.h>
#include <graphics/displayinfo.h>
#include <graphics/gfxbase.h>
#include <graphics/gfxdriver.h>
#include <graphics/hrmodeid.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <resources/framebuf.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

struct GfxBase *GfxBase;
struct IntuitionBase *IntuitionBase;
struct Device *TimerBase;

static int fails;

static void check(int ok, char *what)
{
	if (!ok) {
		xKPrintf("FAIL idisplay: %s\n", what);
		fails++;
	}
}

/* framebuf.resource of an HR card in mono, VRAM in two RAM pages */
static struct FBMode fbmode = {
	1024, 800, 1, 128, FBF_MONO, 0xFFFF, 0, { NULL, NULL }, 0x20000L
};
static struct FBInfo fbinfo = { FBC_HR, 1, &fbmode, 0 };
static char *owner;

static struct FBInfo *fb_getinfo(APTR base)
{
	return &fbinfo;
}

static char *fb_alloc(APTR base, char *name)
{
	if (owner)
		return owner;
	owner = name;
	return NULL;
}

static void fb_free(APTR base)
{
	owner = NULL;
}

static APTR fbfuncs[] = {
	(APTR)fb_getinfo, (APTR)fb_alloc, (APTR)fb_free, (APTR)-1
};

static BOOL fbsetup(void)
{
	struct Library *fb;

	fbmode.fbm_Half[0] = AllocMem(0x10000L, MEMF_CLEAR);
	fbmode.fbm_Half[1] = AllocMem(0x10000L, MEMF_CLEAR);
	if (fbmode.fbm_Half[0] == NULL || fbmode.fbm_Half[1] == NULL ||
	    ((ULONG)fbmode.fbm_Half[0] & 0xFFFF) != 0 ||
	    ((ULONG)fbmode.fbm_Half[1] & 0xFFFF) != 0)
		return FALSE;
	fb = MakeLibrary(fbfuncs, NULL, NULL, sizeof(struct Library), NULL);
	if (fb == NULL)
		return FALSE;
	fb->lib_Node.ln_Name = FRAMEBUFNAME;
	fb->lib_Node.ln_Type = NT_RESOURCE;
	AddResource(fb);
	return TRUE;
}

/* Every overscan rectangle of a mode is the whole display */
static void overscan(ULONG id, WORD w, WORD h)
{
	struct Rectangle r;
	WORD type;

	for (type = OSCAN_TEXT; type <= OSCAN_VIDEO; type++) {
		r.MinX = r.MinY = r.MaxX = r.MaxY = -1;
		check(QueryOverscan(id, &r, type) && r.MinX == 0 && r.MinY == 0 &&
		    r.MaxX == w - 1 && r.MaxY == h - 1, "QueryOverscan");
	}
	check(!QueryOverscan(id, &r, 99), "QueryOverscan of no type");
}

/* What VRAM shows, as a bitmap of the mode's format */
static struct RastPort vram;

static struct BitMap *vramview(WORD w, WORD h, UWORD depth)
{
	struct TagItem t[3];
	struct BitMap *bm;

	t[0].ti_Tag = BMATags_NoMemory;	t[0].ti_Data = TRUE;
	t[1].ti_Tag = BMATags_GfxFormat;
	t[1].ti_Data = depth == 1 ? GDF_PLANAR : GDF_CHUNKY4;
	t[2].ti_Tag = TAG_DONE;
	bm = AllocBitMap(w, h, depth, BMF_CHECKVALUE, (struct BitMap *)t);
	if (bm) {
		bm->Planes[0] = fbmode.fbm_Half[0];
		GFXBM(bm)->gb_Half1 = fbmode.fbm_Half[1];
	}
	InitRastPort(&vram);
	vram.BitMap = bm;
	return bm;
}

static ULONG shown(WORD x, WORD y)
{
	return ReadPixel(&vram, x, y);
}

static BOOL isdisplay(struct Screen *s)
{
	return (GFXBM(s->RastPort.BitMap)->gb_Flags & GBF_DISPLAY) != 0;
}

static BOOL infront(struct Screen *s)
{
	return IntuitionBase->FirstScreen == s && isdisplay(s);
}

/* Fletcher sums of n bytes (even) of bm from offset o, in words */
static void run(struct BitMap *bm, ULONG o, UWORD n, UWORD *s)
{
	UWORD k, s1 = s[0], s2 = s[1], *w;

	if (o < 0x10000L && o + n > 0x10000L) {
		k = 0x10000L - o;
		run(bm, o, k, s);
		run(bm, 0x10000L, n - k, s);
		return;
	}
	w = (UWORD *)GD_ADDR(bm, o);
	for (k = n >> 1; k; k--) {
		s1 += *w++;
		s2 += s1;
	}
	s[0] = s1;
	s[1] = s2;
}

/*
 * A sum of bm's pixels but those near the pointer, which is drawn only
 * into the front screen, and in *box (if not NULL) a sum of those
 */
static ULONG sum(struct BitMap *bm, ULONG *box)
{
	UWORD bpr = bm->BytesPerRow, s[2], b[2];
	WORD ppb = bm->Depth == 1 ? 8 : 2;
	WORD mx = IntuitionBase->MouseX, my = IntuitionBase->MouseY;
	WORD y, lo = (mx - 32) / ppb, hi = (mx + 64) / ppb + 2;
	ULONG o;

	lo = lo < 0 ? 0 : lo & ~1;
	hi = hi > bpr ? bpr : hi & ~1;
	s[0] = s[1] = b[0] = b[1] = 0;
	for (y = 0; y < bm->Rows; y++) {
		o = (ULONG)y * bpr;
		if (y < my - 32 || y > my + 64) {
			run(bm, o, bpr, s);
		} else {
			run(bm, o, lo, s);
			run(bm, o + lo, hi - lo, b);
			run(bm, o + hi, bpr - hi, s);
		}
	}
	if (box)
		*box = (ULONG)b[1] << 16 | b[0];
	return (ULONG)s[1] << 16 | s[0];
}

static struct timerequest *tio;
static struct IOStdReq *iio;

static void opendevs(void)
{
	struct MsgPort *port = CreateMsgPort();

	tio = (struct timerequest *)CreateIORequest(port,
	    sizeof(struct timerequest));
	if (tio != NULL && OpenDevice(TIMERNAME, UNIT_VBLANK,
	    (struct IORequest *)tio, 0) == 0)
		TimerBase = tio->tr_node.io_Device;
	iio = (struct IOStdReq *)CreateIORequest(port, sizeof(struct IOStdReq));
	if (iio != NULL && OpenDevice("input.device", 0,
	    (struct IORequest *)iio, 0) != 0)
		iio = NULL;
}

/*
 * ScreenToFront() and the like leave the work to the input handler: an
 * event, and up to a second until cond(s) holds
 */
static void settle(BOOL (*cond)(struct Screen *), struct Screen *s)
{
	static struct InputEvent ev;
	WORD n;

	ev.ie_Class = IECLASS_NULL;
	iio->io_Command = IND_WRITEEVENT;
	iio->io_Data = (APTR)&ev;
	iio->io_Length = sizeof(ev);
	DoIO((struct IORequest *)iio);
	for (n = 0; n < 50 && !cond(s) && TimerBase; n++) {
		tio->tr_node.io_Command = TR_ADDREQUEST;
		tio->tr_time.tv_secs = 0;
		tio->tr_time.tv_micro = 20000;
		DoIO((struct IORequest *)tio);
	}
}

static LONG ms(struct timeval *t0)
{
	struct timeval t;

	if (TimerBase == NULL)
		return 0;
	GetSysTime(&t);
	return (t.tv_secs - t0->tv_secs) * 1000 +
	    ((LONG)t.tv_micro - (LONG)t0->tv_micro) / 1000;
}

/* A rectangle of pen p in a bitmap */
static void box(struct BitMap *bm, WORD x, WORD y, UBYTE p)
{
	struct RastPort rp;

	InitRastPort(&rp);
	rp.BitMap = bm;
	SetAPen(&rp, p);
	RectFill(&rp, x, y, x + 99, y + 99);
}

/* The rows the alert covers, as a sum */
static ULONG top(WORD w)
{
	ULONG sum = 0;
	WORD x, y;

	for (y = 0; y < 40; y += 3)
		for (x = 0; x < w; x += 7)
			sum = sum * 3 + shown(x, y);
	return sum;
}

static void display(ULONG id, WORD w, WORD h, UWORD depth, UBYTE p)
{
	struct TagItem t[4];
	struct Screen *s;
	struct ScreenBuffer *sb0, *sb1;
	struct BitMap *own;
	struct MsgPort *mp;
	struct timeval t0;
	ULONG before;

	overscan(id, w, h);
	t[0].ti_Tag = SA_DisplayID;	t[0].ti_Data = id;
	t[1].ti_Tag = SA_Depth;		t[1].ti_Data = depth;
	t[2].ti_Tag = SA_Title;		t[2].ti_Data = (IPTR)"Display";
	t[3].ti_Tag = TAG_DONE;
	s = OpenScreenTagList(NULL, t);
	check(s != NULL, "screen on the display");
	if (s == NULL)
		return;
	own = s->RastPort.BitMap;
	check(IS_GFXBM(own) && (GFXBM(own)->gb_Flags & GBF_DISPLAY) &&
	    s->Width == w && s->Height == h, "screen in VRAM");
	GFXBM(vram.BitMap)->gb_PixTab = GFXBM(own)->gb_PixTab;

	/* The buffer shown has VRAM, the others are in RAM */
	mp = CreateMsgPort();
	sb0 = AllocScreenBuffer(s, NULL, SB_SCREEN_BITMAP);
	sb1 = AllocScreenBuffer(s, NULL, SB_COPY_BITMAP);
	check(mp && sb0 && sb1 && sb0->sb_BitMap == own && sb1->sb_BitMap != own,
	    "screen buffers");
	if (!mp || !sb0 || !sb1) {
		CloseScreen(s);
		return;
	}
	box(sb1->sb_BitMap, 100, 300, p);
	check(shown(150, 350) != p, "buffer drawn off screen");
	sb1->sb_DBufInfo->dbi_SafeMessage.mn_ReplyPort = mp;
	sb1->sb_DBufInfo->dbi_DispMessage.mn_ReplyPort = mp;
	check(ChangeScreenBuffer(s, sb1) && shown(150, 350) == p &&
	    shown(400, 1) == ReadPixel(&s->RastPort, 400, 1) &&
	    (GFXBM(sb1->sb_BitMap)->gb_Flags & GBF_DISPLAY) &&
	    !(GFXBM(own)->gb_Flags & GBF_DISPLAY), "ChangeScreenBuffer");
	check(GetMsg(mp) == &sb1->sb_DBufInfo->dbi_DispMessage &&
	    GetMsg(mp) == &sb1->sb_DBufInfo->dbi_SafeMessage, "DBufInfo replies");
	box(own, 500, 300, p);
	check(shown(550, 350) != p, "screen's bitmap drawn off screen");
	if (TimerBase)
		GetSysTime(&t0);
	check(ChangeScreenBuffer(s, sb0) && shown(150, 350) != p &&
	    shown(550, 350) == p, "ChangeScreenBuffer back");
	xKPrintf("idisplay: %ld-plane buffer shown in %ld ms\n", (LONG)depth,
	    ms(&t0));
	FreeScreenBuffer(s, sb1);
	FreeScreenBuffer(s, sb0);
	check(s->RastPort.BitMap == own && s->ViewPort.RasInfo->BitMap == own &&
	    shown(550, 350) == p, "FreeScreenBuffer");
	DeleteMsgPort(mp);

	before = top(w);
	check(!TimedDisplayAlert(RECOVERY_ALERT, "\000(\024Display\000", 40, 5) &&
	    top(w) == before, "alert on the display");
	CloseScreen(s);
}

static struct Screen *screen(ULONG id, UWORD depth, char *title)
{
	struct TagItem t[4];

	t[0].ti_Tag = SA_DisplayID;	t[0].ti_Data = id;
	t[1].ti_Tag = SA_Depth;		t[1].ti_Data = depth;
	t[2].ti_Tag = SA_Title;		t[2].ti_Data = (IPTR)title;
	t[3].ti_Tag = TAG_DONE;
	return OpenScreenTagList(NULL, t);
}

static struct Window *window(struct Screen *s, WORD x)
{
	struct TagItem t[7];

	t[0].ti_Tag = WA_CustomScreen;	t[0].ti_Data = (IPTR)s;
	t[1].ti_Tag = WA_Left;		t[1].ti_Data = x;
	t[2].ti_Tag = WA_Top;		t[2].ti_Data = 150;
	t[3].ti_Tag = WA_Width;		t[3].ti_Data = 300;
	t[4].ti_Tag = WA_Height;	t[4].ti_Data = 200;
	t[5].ti_Tag = WA_Title;		t[5].ti_Data = (IPTR)s->Title;
	t[6].ti_Tag = TAG_DONE;
	return OpenWindowTagList(NULL, t);
}

/* Pen p in a 20 by 20 box of a window at (x, y) of its screen */
static void winbox(struct Window *w, WORD x, WORD y, UBYTE p)
{
	SetAPen(w->RPort, p);
	RectFill(w->RPort, x - w->LeftEdge, y - w->TopEdge,
	    x - w->LeftEdge + 19, y - w->TopEdge + 19);
}

static LONG switches, switchms;

/*
 * s, behind, to the front (as how says): VRAM shows exactly what s's
 * bitmap held, and near the pointer what it showed when last in front,
 * unless s was drawn into since (*box 0)
 */
static void tofront(struct Screen *s, struct Screen *back, ULONG *box,
    int how)
{
	ULONG want = sum(s->RastPort.BitMap, NULL), f;
	struct timeval t0;

	check(!isdisplay(s) && isdisplay(back), "the front screen has VRAM");
	if (TimerBase)
		GetSysTime(&t0);
	if (how == 0)
		ScreenToFront(s);
	else if (how == 1)
		ScreenToBack(back);
	else
		ScreenDepth(s, SDEPTH_TOFRONT, NULL);
	settle(infront, s);
	switchms += ms(&t0);
	switches++;
	check(IntuitionBase->FirstScreen == s && isdisplay(s) && !isdisplay(back),
	    "VRAM goes to the front screen");
	check(sum(vram.BitMap, &f) == want, "the front screen's pixels");
	check(!*box || f == *box, "the front screen as last shown");
	*box = f;
}

/*
 * Whether VRAM's top left holds exactly bm's pixels, but for the rows of
 * the pointer at the top, which a copy leaves out
 */
static BOOL copied(struct BitMap *bm)
{
	UWORD y, i;

	for (y = IntuitionBase->MouseY + 64; y < bm->Rows; y++)
		for (i = 0; i < bm->BytesPerRow; i++)
			if (*GD_ADDR(vram.BitMap, (ULONG)y * vram.BitMap->BytesPerRow + i) !=
			    bm->Planes[0][(ULONG)y * bm->BytesPerRow + i])
				return FALSE;
	return TRUE;
}

/*
 * A screen on a smaller bitmap of its own in front of s: it shows by a
 * copy into VRAM, s's pixels moved to RAM first, and s gets VRAM back
 */
static void custom(ULONG id, UWORD depth, struct Screen *s, UBYTE p)
{
	struct TagItem t[7];
	struct BitMap *bm;
	struct Screen *c;
	ULONG v;

	bm = AllocBitMap(320, 200, depth, BMF_CLEAR, s->RastPort.BitMap);
	if (bm == NULL) {
		check(FALSE, "a custom bitmap");
		return;
	}
	t[0].ti_Tag = SA_DisplayID;	t[0].ti_Data = id;
	t[1].ti_Tag = SA_Depth;		t[1].ti_Data = depth;
	t[2].ti_Tag = SA_Width;		t[2].ti_Data = 320;
	t[3].ti_Tag = SA_Height;	t[3].ti_Data = 200;
	t[4].ti_Tag = SA_BitMap;	t[4].ti_Data = (IPTR)bm;
	t[5].ti_Tag = SA_Quiet;		t[5].ti_Data = TRUE;
	t[6].ti_Tag = TAG_DONE;
	v = sum(s->RastPort.BitMap, NULL);
	c = OpenScreenTagList(NULL, t);
	settle(infront, c);
	check(c && IntuitionBase->FirstScreen == c && !isdisplay(c) &&
	    !isdisplay(s) && sum(s->RastPort.BitMap, NULL) == v,
	    "a custom screen moves the one behind to RAM");
	if (c) {
		SetAPen(&c->RastPort, p);
		RectFill(&c->RastPort, 10, 70, 49, 109);
		ScrollVPort(&c->ViewPort);
		check(copied(bm) && *GD_ADDR(vram.BitMap,
		    300L * vram.BitMap->BytesPerRow + 100) == 0,
		    "a custom screen by a copy");
		v = sum(s->RastPort.BitMap, NULL);
		CloseScreen(c);
		settle(infront, s);
		check(infront(s) && sum(vram.BitMap, NULL) == v,
		    "VRAM back after a custom screen");
	}
	FreeBitMap(bm);
}

/*
 * Two screens of a mode brought to the front in turn, each drawn into
 * while behind, in a window and on the screen
 */
static void flips(ULONG id, UWORD depth, UBYTE pa, UBYTE pb)
{
	struct Screen *a, *b;
	struct Window *wa, *wb;
	ULONG fa, fb, v, vb, f;
	WORD k, x;

	a = screen(id, depth, "A");
	wa = a ? window(a, 100) : NULL;
	check(a && wa && isdisplay(a), "first screen in VRAM");
	if (!a || !wa)
		return;
	GFXBM(vram.BitMap)->gb_PixTab = GFXBM(a->RastPort.BitMap)->gb_PixTab;
	winbox(wa, 150, 200, pa);
	sum(vram.BitMap, &fa);
	b = screen(id, depth, "B");
	wb = b ? window(b, 300) : NULL;
	check(b && wb && isdisplay(b) && !isdisplay(a) &&
	    IntuitionBase->FirstScreen == b, "a new screen in front has VRAM");
	if (!b || !wb)
		return;
	winbox(wb, 350, 200, pb);
	check(shown(360, 210) == pb && shown(160, 210) != pa &&
	    ReadPixel(&a->RastPort, 160, 210) == pa, "screen behind kept in RAM");
	sum(vram.BitMap, &fb);
	for (k = 0; k < 6; k++) {
		/* the screen behind drawn into on odd turns */
		x = 120 + 30 * k;
		if (k & 1) {
			v = sum(vram.BitMap, &vb);
			winbox(wa, x, 250, pa);
			Move(&a->RastPort, x, 330);
			Draw(&a->RastPort, x + 25, 340);
			check(sum(vram.BitMap, &f) == v && f == vb,
			    "drawing behind leaves VRAM");
			fa = 0;
		}
		tofront(a, b, &fa, k % 3);
		if (k & 1)
			check(shown(x + 5, 255) == pa && shown(x + 25, 340) ==
			    ReadPixel(&a->RastPort, x + 25, 340), "drawn while behind");
		if (k & 1) {
			winbox(wb, x + 300, 260, pb);
			fb = 0;
		}
		tofront(b, a, &fb, (k + 1) % 3);
		if (k & 1)
			check(shown(x + 305, 265) == pb, "drawn while behind");
	}
	xKPrintf("idisplay: %ld-plane screen to the front in %ld ms\n",
	    (LONG)depth, switchms / switches);
	switches = switchms = 0;
	custom(id, depth, b, pb);
	/* Closing the front screen gives VRAM back to the one behind */
	v = sum(a->RastPort.BitMap, NULL);
	CloseWindow(wb);
	CloseScreen(b);
	settle(infront, a);
	check(infront(a) && sum(vram.BitMap, NULL) == v, "the screen behind after a close");
	CloseWindow(wa);
	CloseScreen(a);
}

int main(void)
{
	if (!fbsetup()) {
		KPutStr("FAIL idisplay: setup\n");
		return 1;
	}
	GfxBase = (struct GfxBase *)OpenLibrary(GRAPHICSNAME, 39L);
	IntuitionBase = (struct IntuitionBase *)OpenLibrary(INTUITIONNAME, 39L);
	if (GfxBase == NULL || IntuitionBase == NULL) {
		KPutStr("FAIL idisplay: OpenLibrary\n");
		return 1;
	}
	opendevs();
	if (iio == NULL) {
		KPutStr("FAIL idisplay: input.device\n");
		return 1;
	}
	if (vramview(1024, 800, 1)) {
		display(HRMONO_800_ID, 1024, 800, 1, 1);
		flips(HRMONO_800_ID, 1, 1, 1);
		FreeBitMap(vram.BitMap);
	}
	if (vramview(640, 400, 4)) {
		display(HRCOLOR_ID, 640, 400, 4, 12);
		flips(HRCOLOR_ID, 4, 12, 5);
		FreeBitMap(vram.BitMap);
	}
	CloseLibrary((struct Library *)IntuitionBase);
	CloseLibrary((struct Library *)GfxBase);
	if (fails != 0)
		return 1;
	KPutStr("PASS idisplay\n");
	return 0;
}
