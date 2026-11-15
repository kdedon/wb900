/*
 * graphics.library from LIBS:, drawing into RAM bitmaps: planar and
 * chunky 4-bit bitmaps, pens and draw modes, lines, rectangles, pixels,
 * blits with minterms, templates and patterns, text in the ROM font,
 * scrolling, regions, pens, ViewPorts and Views, and the display
 * database, empty without a display.  Bitmaps over 64 KB check the rows
 * that cross into their second segment.  Fills, templates and blits in
 * both formats are checked pixel by pixel against what they should do.  It reports the stack the
 * deepest calls take.
 */
#include <exec/exec.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <graphics/gfxmacros.h>
#include <graphics/gfxdriver.h>
#include <graphics/rastport.h>
#include <graphics/regions.h>
#include <graphics/text.h>
#include <graphics/view.h>
#include <graphics/gfxnodes.h>
#include <graphics/sprite.h>
#include <graphics/videocontrol.h>
#include <graphics/displayinfo.h>
#include <graphics/modeid.h>
#include <graphics/hrmodeid.h>
#include <proto/graphics.h>

struct GfxBase *GfxBase;

static int fails;

static void check(int ok, char *what)
{
	if (!ok) {
		xKPrintf("FAIL graphics: %s\n", what);
		fails++;
	}
}

/* Byte o of plane 0 of a bitmap that may be over 64 KB */
static UBYTE byteat(struct BitMap *bm, ULONG o)
{
	return *(UBYTE *)PtrStep(bm->Planes[0], o);
}

/* A checksum of the rows of plane p */
static ULONG planesum(struct BitMap *bm, int p)
{
	ULONG sum = 0, o, n = (ULONG)bm->BytesPerRow * bm->Rows;

	for (o = 0; o < n; o++)
		sum = sum * 31 + ((UBYTE *)PtrStep(bm->Planes[p], o))[0];
	return sum;
}

static struct TagItem chunkytags[] = {
	{ BMATags_GfxFormat, GDF_CHUNKY4 },
	{ TAG_DONE, 0 }
};

#define EXTTAGS	(BMF_RTGTAGS | BMF_RTGCHECK | BMF_FRIENDISTAG)

static struct BitMap *chunky(ULONG w, ULONG h)
{
	return AllocBitMap(w, h, 4, BMF_CLEAR | EXTTAGS,
	    (struct BitMap *)chunkytags);
}

/* The lowest stack word the deepest of the calls below wrote */
static UWORD *stklow;

static void paint(void)
{
	UWORD here, *p;

	stklow = &here - 1200;
	for (p = stklow; p < &here - 40; p++)
		*p = 0xA5A5;
}

static UWORD used(void)
{
	UWORD here, *p;

	for (p = stklow; *p == 0xA5A5; p++)
		;
	return (UWORD)((char *)&here - (char *)p);
}

static UWORD maxstack;

static void stack(UWORD n)
{
	if (n > maxstack)
		maxstack = n;
}

static void planar(void)
{
	struct RastPort rp;
	struct BitMap *bm;
	UBYTE *p;
	int x, y, ok;

	bm = AllocBitMap(64, 32, 1, BMF_CLEAR, NULL);
	check(bm != NULL, "AllocBitMap 64x32x1");
	if (bm == NULL)
		return;
	check(bm->BytesPerRow == 8 && bm->Rows == 32 && bm->Depth == 1,
	    "planar BitMap fields");
	check(GetBitMapAttr(bm, BMA_WIDTH) == 64 &&
	    GetBitMapAttr(bm, BMA_HEIGHT) == 32 &&
	    GetBitMapAttr(bm, BMA_DEPTH) == 1, "GetBitMapAttr");
	InitRastPort(&rp);
	rp.BitMap = bm;
	check(rp.Font == GfxBase->DefaultFont && rp.DrawMode == JAM2 &&
	    rp.Mask == 0xFF, "InitRastPort");
	p = bm->Planes[0];

	SetAPen(&rp, 1);
	RectFill(&rp, 3, 2, 12, 3);
	check(p[16] == 0x1F && p[17] == 0xF8 && p[24] == 0x1F &&
	    p[25] == 0xF8 && p[8] == 0 && p[32] == 0, "RectFill");
	check(ReadPixel(&rp, 3, 2) == 1 && ReadPixel(&rp, 2, 2) == 0 &&
	    ReadPixel(&rp, 12, 3) == 1 && ReadPixel(&rp, 13, 3) == 0,
	    "ReadPixel");
	check(ReadPixel(&rp, 64, 0) == -1, "ReadPixel outside");

	SetDrMd(&rp, COMPLEMENT);
	RectFill(&rp, 0, 2, 7, 2);
	check(p[16] == 0xE0 && p[17] == 0xF8, "RectFill COMPLEMENT");
	SetDrMd(&rp, JAM2);
	SetAPen(&rp, 0);
	RectFill(&rp, -5, -5, 100, 100);
	check(planesum(bm, 0) == 0, "RectFill clipped to the bitmap");

	SetAPen(&rp, 1);
	WritePixel(&rp, 63, 31);
	check(p[255] == 0x01, "WritePixel");
	SetDrMd(&rp, COMPLEMENT);
	WritePixel(&rp, 63, 31);
	check(p[255] == 0x00, "WritePixel COMPLEMENT");
	SetDrMd(&rp, JAM1);

	/* lines: horizontal, vertical, diagonal and patterned */
	Move(&rp, 0, 0);
	Draw(&rp, 15, 0);
	check(p[0] == 0xFF && p[1] == 0xFF && p[2] == 0, "Draw horizontal");
	Draw(&rp, 15, 7);
	for (ok = 1, y = 0; y < 8; y++)
		ok &= (p[y * 8 + 1] & 1) != 0;
	check(ok && (p[64 + 1] & 1) == 0, "Draw vertical");
	Move(&rp, 20, 10);
	Draw(&rp, 27, 17);
	for (ok = 1, x = 0; x < 8; x++)
		ok &= ReadPixel(&rp, 20 + x, 10 + x) == 1 &&
		    ReadPixel(&rp, 21 + x, 10 + x) == 0;
	check(ok, "Draw diagonal");
	SetDrPt(&rp, 0xAAAA);
	Move(&rp, 32, 20);
	Draw(&rp, 47, 20);
	check(p[20 * 8 + 4] == 0xAA && p[20 * 8 + 5] == 0xAA,
	    "Draw patterned");
	SetDrPt(&rp, 0xFFFF);

	/* SetRast clears the whole bitmap */
	SetRast(&rp, 0);
	check(planesum(bm, 0) == 0, "SetRast 0");
	SetRast(&rp, 1);
	check(p[0] == 0xFF && p[255] == 0xFF, "SetRast 1");
	SetRast(&rp, 0);

	FreeBitMap(bm);
}

static void text(void)
{
	struct RastPort rp;
	struct BitMap *bm;
	struct TextFont *tf = GfxBase->DefaultFont;
	struct TextAttr ta;
	struct TextFont *f;
	struct TextExtent te;
	struct TextFont f2;
	static WORD space[256], kern[256];
	UBYTE *p, *glyph, str[5];
	ULONG loc;
	int x, y, k, ok;

	ta.ta_Name = (STRPTR)"topaz.font";
	ta.ta_YSize = 8;
	ta.ta_Style = 0;
	ta.ta_Flags = 0;
	f = OpenFont(&ta);
	check(f == tf && tf->tf_YSize == 8 && tf->tf_XSize == 8, "OpenFont");

	bm = AllocBitMap(64, 16, 1, BMF_CLEAR, NULL);
	if (bm == NULL)
		return;
	InitRastPort(&rp);
	rp.BitMap = bm;
	SetFont(&rp, f);
	check(TextLength(&rp, "Hello", 5) == 40, "TextLength");
	TextExtent(&rp, "Hi", 2, &te);
	check(te.te_Width == 16 && te.te_Height == 8 &&
	    te.te_Extent.MinY == -6 && te.te_Extent.MaxX == 15, "TextExtent");

	SetABPenDrMd(&rp, 1, 0, JAM2);
	Move(&rp, 8, 6);
	paint();
	Text(&rp, "H", 1);
	stack(used());
	check(rp.cp_x == 16 && rp.cp_y == 6, "Text moves the pen");
	loc = ((ULONG *)tf->tf_CharLoc)['H' - tf->tf_LoChar];
	glyph = (UBYTE *)tf->tf_CharData + (loc >> 16) / 8;
	p = bm->Planes[0];
	for (ok = 1, y = 0; y < 8; y++)
		ok &= p[y * 8 + 1] == glyph[y * tf->tf_Modulo] &&
		    p[y * 8] == 0 && p[y * 8 + 2] == 0;
	check(ok, "Text glyph");

	/* the same glyph, not byte aligned, in JAM1 over a set background */
	SetRast(&rp, 0);
	SetAPen(&rp, 1);
	RectFill(&rp, 0, 8, 63, 15);
	SetABPenDrMd(&rp, 0, 1, JAM1);
	Move(&rp, 3, 14);
	Text(&rp, "H", 1);
	for (ok = 1, y = 0; y < 8; y++) {
		UWORD row = (p[(8 + y) * 8] << 8) | p[(8 + y) * 8 + 1];

		ok &= row == (UWORD)(0xFFFF & ~(glyph[y * tf->tf_Modulo] << 5));
	}
	check(ok, "Text JAM1 shifted");

	/* glyphs from both ends of the font and from outside it */
	str[0] = tf->tf_LoChar;
	str[1] = tf->tf_HiChar;
	str[2] = tf->tf_LoChar - 1;
	str[3] = 'q';
	str[4] = tf->tf_LoChar + 1;
	SetRast(&rp, 0);
	SetABPenDrMd(&rp, 1, 0, JAM2);
	Move(&rp, 0, 6);
	Text(&rp, (STRPTR)str, 5);
	for (ok = 1, x = 0; x < 5; x++) {
		k = str[x] < tf->tf_LoChar || str[x] > tf->tf_HiChar ?
		    tf->tf_HiChar - tf->tf_LoChar + 1 : str[x] - tf->tf_LoChar;
		loc = ((ULONG *)tf->tf_CharLoc)[k];
		glyph = (UBYTE *)tf->tf_CharData + (loc >> 16) / 8;
		for (y = 0; y < 8; y++)
			ok &= p[y * 8 + x] == glyph[y * tf->tf_Modulo];
	}
	check(ok, "Text glyphs");

	/* a glyph spaced or kerned otherwise goes the long way */
	f2 = *tf;
	for (x = 0; x < 256; x++) {
		space[x] = 8;
		kern[x] = 0;
	}
	space['q' - tf->tf_LoChar] = 9;
	f2.tf_CharSpace = (APTR)space;
	SetFont(&rp, &f2);
	Move(&rp, 0, 6);
	Text(&rp, "qq", 2);
	check(rp.cp_x == 18 && TextLength(&rp, "qq", 2) == 18, "Text spaced");
	space['q' - tf->tf_LoChar] = 8;
	kern['q' - tf->tf_LoChar] = 1;
	f2.tf_CharKern = (APTR)kern;
	Move(&rp, 0, 6);
	Text(&rp, "qq", 2);
	check(rp.cp_x == 18 && TextLength(&rp, "qq", 2) == 18, "Text kerned");
	SetFont(&rp, f);
	CloseFont(f);
	FreeBitMap(bm);
}

static void blits(void)
{
	struct RastPort rp, rp2;
	struct BitMap *a, *b;
	static UWORD tmpl[] = { 0xF00F, 0x0FF0 };
	static UWORD ptrn[] = { 0xAAAA, 0x5555 };
	UBYTE *p;
	int x, y, ok;

	a = AllocBitMap(64, 16, 1, BMF_CLEAR, NULL);
	b = AllocBitMap(64, 16, 1, BMF_CLEAR, NULL);
	if (a == NULL || b == NULL)
		return;
	InitRastPort(&rp);
	rp.BitMap = a;
	InitRastPort(&rp2);
	rp2.BitMap = b;

	/* a diagonal pattern to copy */
	SetAPen(&rp, 1);
	for (y = 0; y < 16; y++)
		WritePixel(&rp, (y * 3) & 63, y);
	RectFill(&rp, 40, 4, 50, 9);

	check(BltBitMap(a, 0, 0, b, 3, 1, 60, 15, 0xC0, 0xFF, NULL) == 1,
	    "BltBitMap result");
	for (ok = 1, y = 0; y < 15; y++)
		for (x = 0; x < 60; x++)
			ok &= ReadPixel(&rp, x, y) == ReadPixel(&rp2, x + 3, y + 1);
	check(ok, "BltBitMap shifted copy");

	/* inverted copy back, then an exclusive or of the two: all set */
	BltBitMap(b, 3, 1, a, 0, 0, 60, 15, 0x30, 0xFF, NULL);
	BltBitMap(b, 3, 1, a, 0, 0, 60, 15, 0x60, 0xFF, NULL);
	for (ok = 1, y = 0; y < 15; y++)
		for (x = 0; x < 60; x++)
			ok &= ReadPixel(&rp, x, y) == 1;
	check(ok, "BltBitMap minterms 0x30, 0x60");

	/* overlapping copy within one bitmap, to the right and down */
	SetRast(&rp, 0);
	for (y = 0; y < 8; y++)
		WritePixel(&rp, y * 2, y);
	BltBitMap(a, 0, 0, a, 5, 3, 20, 8, 0xC0, 0xFF, NULL);
	for (ok = 1, y = 0; y < 8; y++)
		ok &= ReadPixel(&rp, y * 2 + 5, y + 3) == 1;
	check(ok, "BltBitMap overlapping");

	/* ClipBlit and BltBitMapRastPort */
	SetRast(&rp2, 0);
	paint();
	ClipBlit(&rp, 0, 0, &rp2, 9, 2, 30, 12, 0xC0);
	stack(used());
	for (ok = 1, y = 0; y < 12; y++)
		for (x = 0; x < 30; x++)
			ok &= ReadPixel(&rp, x, y) == ReadPixel(&rp2, x + 9, y + 2);
	check(ok, "ClipBlit");
	BltBitMapRastPort(a, 0, 0, &rp2, 9, 2, 30, 12, 0x50);
	for (ok = 1, y = 0; y < 12; y++)
		for (x = 0; x < 30; x++)
			ok &= ReadPixel(&rp, x, y) != ReadPixel(&rp2, x + 9, y + 2);
	check(ok, "BltBitMapRastPort 0x50");

	/* BltTemplate in JAM2 */
	SetRast(&rp2, 0);
	SetABPenDrMd(&rp2, 1, 0, JAM2);
	paint();
	BltTemplate((PLANEPTR)tmpl, 4, 2, &rp2, 2, 0, 12, 2);
	stack(used());
	p = b->Planes[0];
	check(p[0] == 0x00 && p[1] == 0x3C && p[8] == 0x3F && p[9] == 0xC0,
	    "BltTemplate");

	/* BltPattern, and RectFill with an area pattern */
	SetRast(&rp2, 0);
	SetAfPt(&rp2, ptrn, 1);
	SetDrMd(&rp2, JAM1);
	RectFill(&rp2, 0, 0, 15, 3);
	check(p[0] == 0xAA && p[8] == 0x55 && p[16] == 0xAA && p[24] == 0x55 &&
	    p[2] == 0, "RectFill with AreaPtrn");
	SetRast(&rp2, 0);
	BltPattern(&rp2, (PLANEPTR)tmpl, 0, 0, 15, 1, 2);
	check(p[0] == 0xA0 && p[1] == 0x0A && p[8] == 0x05 && p[9] == 0x50,
	    "BltPattern with a mask");
	SetAfPt(&rp2, NULL, 0);

	FreeBitMap(a);
	FreeBitMap(b);
}

static void scroll(void)
{
	struct RastPort rp;
	struct BitMap *bm;
	int ok, y;

	bm = AllocBitMap(64, 16, 1, BMF_CLEAR, NULL);
	if (bm == NULL)
		return;
	InitRastPort(&rp);
	rp.BitMap = bm;
	SetAPen(&rp, 1);
	for (y = 0; y < 16; y++)
		WritePixel(&rp, 20 + y, y);
	SetBPen(&rp, 0);
	ScrollRaster(&rp, 4, 2, 0, 0, 63, 15);
	for (ok = 1, y = 0; y < 14; y++)
		ok &= ReadPixel(&rp, 18 + y, y) == 1 && ReadPixel(&rp, 22 + y, y) == 0;
	check(ok, "ScrollRaster moved");
	check(ReadPixel(&rp, 34, 14) == 0 && ReadPixel(&rp, 35, 15) == 0,
	    "ScrollRaster filled");
	check(rp.FgPen == 1, "ScrollRaster pens");
	FreeBitMap(bm);
}

static void chunky4(void)
{
	struct RastPort rp;
	struct BitMap *c, *img;
	struct BitMap plan;
	static UWORD plane0[] = { 0xF000, 0xF000 };
	static UWORD plane1[] = { 0xCC00, 0xCC00 };
	static UWORD halftone[] = { 0x3333, 0xCCCC };
	static UBYTE pixtab[256];
	UBYTE *p;
	int x, y, ok;
	ULONG v;

	c = chunky(40, 10);
	check(c != NULL, "AllocBitMap chunky");
	if (c == NULL)
		return;
	check(c->BytesPerRow == 24 && c->Depth == 4 &&
	    GetBitMapAttr(c, BMA_WIDTH) == 48, "chunky BitMap fields");
	InitRastPort(&rp);
	rp.BitMap = c;
	p = c->Planes[0];
	SetAPen(&rp, 9);
	RectFill(&rp, 1, 0, 4, 0);
	check(p[0] == 0x09 && p[1] == 0x99 && p[2] == 0x90 && p[3] == 0,
	    "chunky RectFill");
	check(ReadPixel(&rp, 1, 0) == 9 && ReadPixel(&rp, 5, 0) == 0,
	    "chunky ReadPixel");
	SetWriteMask(&rp, 0x3);
	SetAPen(&rp, 6);
	RectFill(&rp, 0, 0, 3, 0);
	check(p[0] == 0x2A && p[1] == 0xAA, "chunky RectFill under a mask");
	SetWriteMask(&rp, 0xFF);

	/* a pattern in two pens from an odd pixel, then under a mask */
	SetRast(&rp, 0);
	SetAfPt(&rp, halftone, 1);
	SetABPenDrMd(&rp, 11, 4, JAM2);
	RectFill(&rp, 1, 0, 34, 1);
	SetWriteMask(&rp, 0x3);
	RectFill(&rp, 3, 2, 36, 3);
	SetWriteMask(&rp, 0xFF);
	SetAfPt(&rp, NULL, 0);
	for (ok = 1, y = 0; y < 4; y++)
		for (x = 0; x < 40; x++) {
			v = ((halftone[y & 1] >> (15 - (x & 15))) & 1) ? 11 : 4;
			if (y < 2 ? x < 1 || x > 34 : x < 3 || x > 36)
				v = 0;
			ok &= ReadPixel(&rp, x, y) == (y < 2 ? v : v & 3);
		}
	check(ok, "chunky RectFill with a pattern");
	SetDrMd(&rp, JAM1);
	SetRast(&rp, 0);

	/* pens through a pixel table, as on a fixed palette */
	for (x = 0; x < 256; x++)
		pixtab[x] = (x & 15) ^ 7;
	GFXBM(c)->gb_PixTab = pixtab;
	SetAPen(&rp, 2);
	RectFill(&rp, 0, 0, 9, 0);
	BltBitMap(c, 0, 0, c, 4, 0, 4, 1, 0x00, 0xFF, NULL);
	check(ReadPixel(&rp, 3, 0) == 2 && ReadPixel(&rp, 4, 0) == 0 &&
	    p[0] == 0x55 && p[2] == 0x77 && p[4] == 0x55,
	    "BltBitMap clear through a pixel table");
	img = AllocBitMap(8, 1, 4, BMF_CLEAR, c);
	check(img && GFXBM(img)->gb_PixTab == pixtab, "a friend's pixel table");
	if (img)
		FreeBitMap(img);
	GFXBM(c)->gb_PixTab = NULL;
	SetRast(&rp, 0);

	/* text and a planar image of depth 2 */
	SetABPenDrMd(&rp, 12, 1, JAM2);
	Move(&rp, 0, 9);
	Text(&rp, "|", 1);
	check(ReadPixel(&rp, 3, 4) == 12 && ReadPixel(&rp, 0, 4) == 1,
	    "chunky Text");
	InitBitMap(&plan, 2, 16, 2);
	plan.Planes[0] = (PLANEPTR)plane0;
	plan.Planes[1] = (PLANEPTR)plane1;
	paint();
	BltBitMapRastPort(&plan, 0, 0, &rp, 20, 1, 6, 2, 0xC0);
	stack(used());
	for (ok = 1, x = 0; x < 6; x++)
		ok &= ReadPixel(&rp, 20 + x, 1) ==
		    ((x < 4) | ((x < 2 || x == 4 || x == 5) ? 2 : 0));
	check(ok, "planar image to chunky");

	/* chunky to chunky, by a nibble */
	BltBitMap(c, 20, 1, c, 31, 5, 6, 1, 0xC0, 0xFF, NULL);
	for (ok = 1, x = 0; x < 6; x++)
		ok &= ReadPixel(&rp, 31 + x, 5) == ReadPixel(&rp, 20 + x, 1);
	check(ok, "chunky BltBitMap odd");

	/* chunky back to planar */
	img = AllocBitMap(16, 2, 2, BMF_CLEAR, NULL);
	if (img) {
		BltBitMap(c, 20, 1, img, 0, 0, 6, 2, 0xC0, 0xFF, NULL);
		check(img->Planes[0][0] == 0xF0 && img->Planes[1][0] == 0xCC,
		    "chunky to planar");
		FreeBitMap(img);
	}
	FreeBitMap(c);
}

/*
 * The row code against the pixels: each operation on an area of FW x FH
 * pixels, its expected pixels worked out from the area before it.
 */
#define FW	72
#define FH	12

static UBYTE was[FH][FW], want[FH][FW];
static UBYTE tmpl[FH * 10];
static ULONG seed = 12345;

static UWORD rnd(void)
{
	seed = seed * 1103515245L + 12345;
	return (UWORD)(seed >> 16);
}

static void snap(struct RastPort *rp, WORD ox, WORD oy, UBYTE (*a)[FW])
{
	WORD x, y;

	for (y = 0; y < FH; y++)
		for (x = 0; x < FW; x++)
			a[y][x] = ReadPixel(rp, ox + x, oy + y);
}

/* The area after the operation against want */
static void compare(struct RastPort *rp, WORD ox, WORD oy, char *name,
    char *what)
{
	WORD x, y;

	for (y = 0; y < FH; y++)
		for (x = 0; x < FW; x++)
			if (ReadPixel(rp, ox + x, oy + y) != want[y][x]) {
				xKPrintf("FAIL graphics: %s %s at %d,%d\n", name,
				    what, ox + x, oy + y);
				fails++;
				return;
			}
}

/* What a draw mode puts at a pixel of pattern or template bit t */
static UBYTE pixel(UBYTE old, WORD t, UBYTE mode, UBYTE a, UBYTE b,
    UBYTE all)
{
	if (mode & INVERSVID)
		t = !t;
	if (mode & COMPLEMENT)
		return t ? old ^ all : old;
	if (mode & JAM2)
		return t ? a : b;
	return t ? a : old;
}

static UBYTE minterm(UBYTE mt, UBYTE b, UBYTE c, UBYTE all)
{
	UBYTE r = 0;

	if (mt & 0x80)
		r |= b & c;
	if (mt & 0x40)
		r |= b & ~c;
	if (mt & 0x20)
		r |= ~b & c;
	if (mt & 0x10)
		r |= ~b & ~c;
	return r & all;
}

static UWORD halftone[] = { 0x3333, 0xCCCC };

/* RectFill of (x0, y0)-(x1, y1) in the area, pattern rows np (0: none) */
static void fillcase(struct RastPort *rp, WORD ox, WORD oy, char *name,
    WORD x0, WORD y0, WORD x1, WORD y1, UBYTE mode, UBYTE a, UBYTE b,
    WORD np, UBYTE all)
{
	WORD x, y, t;

	snap(rp, ox, oy, was);
	SetABPenDrMd(rp, a, b, mode);
	SetAfPt(rp, np ? halftone : NULL, np == 2 ? 1 : 0);
	RectFill(rp, ox + x0, oy + y0, ox + x1, oy + y1);
	SetAfPt(rp, NULL, 0);
	for (y = 0; y < FH; y++)
		for (x = 0; x < FW; x++) {
			want[y][x] = was[y][x];
			if (x < x0 || x > x1 || y < y0 || y > y1)
				continue;
			t = np ? (halftone[(oy + y) & (np - 1)] >>
			    (15 - ((ox + x) & 15))) & 1 : 1;
			want[y][x] = pixel(was[y][x], t, mode, a, b, all);
		}
	compare(rp, ox, oy, name, "RectFill");
}

/* BltTemplate of tmpl (rows of 10 bytes) from bit sx, w x h at (x0, y0) */
static void tmplcase(struct RastPort *rp, WORD ox, WORD oy, char *name,
    WORD sx, WORD x0, WORD y0, WORD w, WORD h, UBYTE mode, UBYTE a, UBYTE b,
    UBYTE all)
{
	WORD x, y, t, bit;

	snap(rp, ox, oy, was);
	SetABPenDrMd(rp, a, b, mode);
	BltTemplate(tmpl, sx, 10, rp, ox + x0, oy + y0, w, h);
	for (y = 0; y < FH; y++)
		for (x = 0; x < FW; x++) {
			want[y][x] = was[y][x];
			if (x < x0 || x >= x0 + w || y < y0 || y >= y0 + h)
				continue;
			bit = sx + x - x0;
			t = (tmpl[(y - y0) * 10 + (bit >> 3)] >> (7 - (bit & 7))) & 1;
			want[y][x] = pixel(was[y][x], t, mode, a, b, all);
		}
	compare(rp, ox, oy, name, "BltTemplate");
}

/* BltBitMap in the bitmap from (sx, sy) to (dx, dy), w x h */
static void blitcase(struct RastPort *rp, WORD ox, WORD oy, char *name,
    WORD sx, WORD sy, WORD dx, WORD dy, WORD w, WORD h, UBYTE mt, UBYTE all)
{
	WORD x, y;

	snap(rp, ox, oy, was);
	BltBitMap(rp->BitMap, ox + sx, oy + sy, rp->BitMap, ox + dx, oy + dy,
	    w, h, mt, 0xFF, NULL);
	for (y = 0; y < FH; y++)
		for (x = 0; x < FW; x++) {
			want[y][x] = was[y][x];
			if (x >= dx && x < dx + w && y >= dy && y < dy + h)
				want[y][x] = minterm(mt, was[y - dy + sy][x - dx + sx],
				    was[y][x], all);
		}
	compare(rp, ox, oy, name, "BltBitMap");
}

/*
 * A line by the blitter's line mode, which steps diagonally on a tie,
 * put in want where it is in the area; pattern bit 15 goes with the
 * first pixel.
 */
static void refline(WORD ox, WORD oy, WORD x0, WORD y0, WORD x1, WORD y1,
    UWORD ptrn, UBYTE mode, UBYTE a, UBYTE b, UBYTE all)
{
	WORD dx = x1 - x0, dy = y1 - y0, sx = dx > 0 ? 1 : -1;
	WORD sy = dy > 0 ? 1 : -1, x = x0, y = y0, dM, dm, d, i, steep;

	if (dx < 0)
		dx = -dx;
	if (dy < 0)
		dy = -dy;
	steep = dx < dy;
	dM = steep ? dy : dx;
	dm = steep ? dx : dy;
	d = 2 * dm - dM;
	for (i = 0; i <= dM; i++) {
		if (x >= ox && x < ox + FW && y >= oy && y < oy + FH)
			want[y - oy][x - ox] = pixel(want[y - oy][x - ox],
			    (ptrn >> (15 - (i & 15))) & 1, mode, a, b, all);
		if (d >= 0) {
			if (steep)
				x += sx;
			else
				y += sy;
			d += 2 * (dm - dM);
		} else
			d += 2 * dm;
		if (steep)
			y += sy;
		else
			x += sx;
	}
}

/* Lines in all directions, some running out of the bitmap */
static void linecase(struct RastPort *rp, WORD ox, WORD oy, char *name,
    UBYTE mode, UBYTE a, UBYTE b, UWORD ptrn, UBYTE all)
{
	static WORD ends[][4] = {
		{ 0, 0, 71, 11 }, { 70, 1, 2, 10 }, { 5, 11, 40, 0 },
		{ 64, 0, 66, 11 }, { 1, 5, 70, 6 }, { 66, 11, 60, 0 },
		{ 71, 11, 0, 0 }, { 30, 0, 20, 11 }, { 2, 11, 5, 0 },
		{ -40, -30, 100, 25 }, { 80, -5, -20, 30 }, { 10, 40, 30, -50 },
		{ -10, 6, 200, 7 }, { 50, -9, 47, 20 }, { 0, 3, 71, 3 }
	};
	WORD x, y, k;

	snap(rp, ox, oy, was);
	for (y = 0; y < FH; y++)
		for (x = 0; x < FW; x++)
			want[y][x] = was[y][x];
	SetABPenDrMd(rp, a, b, mode);
	SetDrPt(rp, ptrn);
	for (k = 0; k < sizeof(ends) / sizeof(ends[0]); k++) {
		Move(rp, ox + ends[k][0], oy + ends[k][1]);
		Draw(rp, ox + ends[k][2], oy + ends[k][3]);
		refline(ox, oy, ox + ends[k][0], oy + ends[k][1],
		    ox + ends[k][2], oy + ends[k][3], ptrn, mode, a, b, all);
	}
	SetDrPt(rp, 0xFFFF);
	compare(rp, ox, oy, name, "Draw");
}

static void rowcases(struct BitMap *bm, WORD ox, WORD oy, char *name)
{
	struct RastPort rp;
	WORD x, y, k;
	UBYTE all;

	InitRastPort(&rp);
	rp.BitMap = bm;
	all = (1 << GetBitMapAttr(bm, BMA_DEPTH)) - 1;
	for (y = 0; y < FH; y++)
		for (x = 0; x < FW; x++) {
			SetAPen(&rp, rnd() & all);
			WritePixel(&rp, ox + x, oy + y);
		}
	for (k = 0; k < FH * 10; k++)
		tmpl[k] = rnd();
	fillcase(&rp, ox, oy, name, 3, 1, 60, 9, JAM2, 5 & all, 0, 0, all);
	fillcase(&rp, ox, oy, name, 5, 0, 66, 10, JAM1, 6 & all, 0, 2, all);
	fillcase(&rp, ox, oy, name, 1, 2, 17, 11, COMPLEMENT, 1, 0, 0, all);
	fillcase(&rp, ox, oy, name, 9, 3, 70, 8, JAM2 | INVERSVID, 3 & all,
	    4 & all, 2, all);
	fillcase(&rp, ox, oy, name, 30, 0, 30, 11, JAM2, 7 & all, 0, 0, all);
	tmplcase(&rp, ox, oy, name, 3, 7, 2, 50, 8, JAM2, 2 & all, 5 & all,
	    all);
	tmplcase(&rp, ox, oy, name, 0, 13, 1, 59, 10, JAM1, 7 & all, 0, all);
	tmplcase(&rp, ox, oy, name, 11, 2, 0, 33, 12, COMPLEMENT | INVERSVID,
	    1, 0, all);
	tmplcase(&rp, ox, oy, name, 5, 40, 4, 1, 5, JAM2 | INVERSVID, 6 & all,
	    1, all);
	tmplcase(&rp, ox, oy, name, 2, 9, 0, 17, 12, JAM1 | INVERSVID, 3 & all,
	    0, all);
	tmplcase(&rp, ox, oy, name, 6, 1, 1, 70, 6, COMPLEMENT, 1, 0, all);
	blitcase(&rp, ox, oy, name, 2, 1, 7, 1, 40, 8, 0xC0, all);
	blitcase(&rp, ox, oy, name, 9, 2, 2, 2, 50, 8, 0xC0, all);
	blitcase(&rp, ox, oy, name, 16, 1, 32, 3, 30, 7, 0xC0, all);
	blitcase(&rp, ox, oy, name, 20, 2, 4, 2, 48, 9, 0xC0, all);
	blitcase(&rp, ox, oy, name, 5, 4, 3, 1, 60, 7, 0x60, all);
	blitcase(&rp, ox, oy, name, 1, 0, 6, 3, 61, 9, 0x30, all);
	blitcase(&rp, ox, oy, name, 0, 0, 0, 1, 72, 11, 0xC0, all);
	blitcase(&rp, ox, oy, name, 3, 0, 14, 2, 40, 8, 0xC0, all);
	blitcase(&rp, ox, oy, name, 30, 3, 19, 3, 37, 6, 0xC0, all);
	blitcase(&rp, ox, oy, name, 2, 1, 18, 1, 40, 8, 0x30, all);
	blitcase(&rp, ox, oy, name, 1, 2, 6, 2, 50, 8, 0x30, all);
	blitcase(&rp, ox, oy, name, 16, 1, 0, 1, 40, 8, 0x30, all);
	blitcase(&rp, ox, oy, name, 3, 0, 14, 2, 40, 8, 0x30, all);
	linecase(&rp, ox, oy, name, JAM1, 5 & all | 1, 0, 0xFFFF, all);
	linecase(&rp, ox, oy, name, JAM1, 6 & all | 1, 0, 0xF0C3, all);
	linecase(&rp, ox, oy, name, JAM2, 3 & all, 6 & all, 0xE4D1, all);
	linecase(&rp, ox, oy, name, COMPLEMENT, 1, 0, 0xFF3C, all);
}

/*
 * BltBitMap from a source x left of the destination's in their words:
 * from x 0-3 of one bitmap to x 5-20 of another
 */
static void blitedge(struct BitMap *src, struct BitMap *dst, char *name)
{
	struct RastPort s, d;
	WORD x, y, sx, dx, ok = TRUE;
	UBYTE all = (1 << GetBitMapAttr(dst, BMA_DEPTH)) - 1;

	InitRastPort(&s);
	s.BitMap = src;
	InitRastPort(&d);
	d.BitMap = dst;
	for (y = 0; y < 6; y++)
		for (x = 0; x < 40; x++) {
			SetAPen(&s, rnd() & all);
			WritePixel(&s, x, y);
		}
	for (sx = 0; sx < 4; sx++)
		for (dx = 5; dx <= 20; dx += 5) {
			BltBitMap(src, sx, 0, dst, dx, 2, 30 - sx, 5, 0xC0, 0xFF,
			    NULL);
			for (y = 0; y < 5; y++)
				for (x = 0; x < 30 - sx; x++)
					if (ReadPixel(&d, dx + x, 2 + y) !=
					    ReadPixel(&s, sx + x, y))
						ok = FALSE;
		}
	if (!ok)
		xKPrintf("FAIL graphics: %s BltBitMap from the left edge\n", name);
	fails += !ok;
}

/* Pixel x of row y of a planar bitmap, from its planes */
static UBYTE rawpix(struct BitMap *bm, WORD x, WORD y)
{
	UBYTE v = 0;
	WORD i;

	for (i = 0; i < bm->Depth; i++)
		if ((bm->Planes[i][y * bm->BytesPerRow + (x >> 3)] >>
		    (7 - (x & 7))) & 1)
			v |= 1 << i;
	return v;
}

/* Pixel x of row y of a chunky bitmap */
static UBYTE rawnib(struct BitMap *bm, WORD x, WORD y)
{
	UBYTE b = bm->Planes[0][y * bm->BytesPerRow + (x >> 1)];

	return (x & 1) ? b & 15 : b >> 4;
}

static UBYTE flip1[256], flip2[256], flip7[256];

/*
 * BltBitMap of pens from a planar image to a planar bitmap whose pixels
 * are its pens inverted, as on the mono display, and from such a bitmap
 * to an image, each pixel checked against the minterm on the pens.
 */
static void flipcase(struct BitMap *src, struct BitMap *dst, WORD sx,
    WORD dx, UBYTE mt, UBYTE mask, char *name)
{
	UBYTE *sp = IS_GFXBM(src) ? GFXBM(src)->gb_PixTab : NULL;
	UBYTE *dp = IS_GFXBM(dst) ? GFXBM(dst)->gb_PixTab : NULL;
	UBYTE old[6][48], all = (1 << dst->Depth) - 1, s, d, w;
	WORD x, y, ok = TRUE;

	/* the planes above the source's depth stay */
	if (src->Depth < dst->Depth)
		mask &= (1 << src->Depth) - 1;
	for (y = 0; y < 6; y++)
		for (x = 0; x < 48; x++)
			old[y][x] = rawpix(dst, x, y);
	BltBitMap(src, sx, 1, dst, dx, 2, 21, 3, mt, mask, NULL);
	for (y = 0; y < 6; y++)
		for (x = 0; x < 48; x++) {
			w = old[y][x];
			if (x >= dx && x < dx + 21 && y >= 2 && y < 5) {
				s = rawpix(src, x - dx + sx, y - 1);
				if (sp)
					s ^= sp[0];
				d = dp ? w ^ dp[0] : w;
				d = minterm(mt, s, d, all);
				d = dp ? d ^ dp[0] : d;
				w = (w & ~mask) | (d & mask);
			}
			ok &= rawpix(dst, x, y) == w;
		}
	if (!ok)
		xKPrintf("FAIL graphics: %s BltBitMap %02x from %d to %d\n", name,
		    mt, sx, dx);
	fails += !ok;
}

static void flips(void)
{
	static UBYTE mts[] = { 0xC0, 0x30, 0x60, 0x80, 0xE0, 0x50 };
	struct BitMap *a, *b, *c, *pl;
	WORD i, k, x, y, ok;

	for (i = 0; i < 256; i++) {
		flip1[i] = (i & 1) ^ 1;
		flip2[i] = (i & 3) ^ 3;
		flip7[i] = (i & 15) ^ 7;
	}
	a = AllocBitMap(48, 6, 1, BMF_CLEAR, NULL);
	b = AllocBitMap(48, 6, 2, BMF_CLEAR, NULL);
	pl = AllocBitMap(48, 6, 2, BMF_CLEAR, NULL);
	c = chunky(48, 6);
	if (a == NULL || b == NULL || pl == NULL || c == NULL)
		return;
	for (y = 0; y < 6; y++)
		for (x = 0; x < 6; x++) {
			a->Planes[0][y * a->BytesPerRow + x] = rnd();
			b->Planes[0][y * b->BytesPerRow + x] = rnd();
			b->Planes[1][y * b->BytesPerRow + x] = rnd();
			pl->Planes[0][y * pl->BytesPerRow + x] = rnd();
			pl->Planes[1][y * pl->BytesPerRow + x] = rnd();
		}
	GFXBM(a)->gb_PixTab = flip1;
	GFXBM(b)->gb_PixTab = flip2;
	for (i = 0; i < sizeof(mts); i++)
		for (k = 0; k < 4; k++) {
			flipcase(pl, a, k * 5, 3 + k * 7, mts[i], 0xFF, "to flipped 1");
			flipcase(pl, b, k * 3, 16 - k * 5, mts[i], 0xFF, "to flipped 2");
			flipcase(pl, b, k, 8 + k, mts[i], 0x01, "under a mask to flipped 2");
			flipcase(a, pl, k * 4, 9 - k, mts[i], 0xFF, "from flipped 1");
			flipcase(b, a, 16 + k, k * 8, mts[i], 0xFF, "between flipped");
		}
	GFXBM(a)->gb_PixTab = NULL;
	GFXBM(b)->gb_PixTab = NULL;

	/* planar pens to chunky pixels through a table, and back */
	GFXBM(c)->gb_PixTab = flip7;
	for (k = 0; k < 4; k++) {
		for (y = 0; y < 6; y++)
			for (x = 0; x < c->BytesPerRow; x++)
				c->Planes[0][y * c->BytesPerRow + x] = 0;
		BltBitMap(pl, k * 3, 0, c, 5 + k * 3, 1, 19, 4, 0xC0, 0xFF, NULL);
		for (ok = 1, y = 0; y < 4; y++)
			for (x = 0; x < 19; x++)
				ok &= rawnib(c, 5 + k * 3 + x, 1 + y) ==
				    (flip7[rawpix(pl, k * 3 + x, y)] & 3);
		check(ok, "planar to chunky through a pixel table");
		BltBitMap(c, 5 + k * 3, 1, b, 9 - k, 0, 19, 4, 0xC0, 0xFF, NULL);
		for (ok = 1, y = 0; y < 4; y++)
			for (x = 0; x < 19; x++)
				ok &= rawpix(b, 9 - k + x, y) == rawpix(pl, k * 3 + x, y);
		check(ok, "chunky through a pixel table to planar");
	}
	GFXBM(c)->gb_PixTab = NULL;
	FreeBitMap(a);
	FreeBitMap(b);
	FreeBitMap(pl);
	FreeBitMap(c);
}

static void rows(void)
{
	struct BitMap *bm, *src;

	bm = AllocBitMap(200, 40, 3, BMF_CLEAR, NULL);
	src = AllocBitMap(40, 6, 3, BMF_CLEAR, NULL);
	if (bm && src)
		blitedge(src, bm, "planar 3");
	if (src)
		FreeBitMap(src);
	if (bm) {
		rowcases(bm, 5, 3, "planar 3");
		FreeBitMap(bm);
	}
	bm = chunky(200, 40);
	src = chunky(40, 6);
	if (bm && src)
		blitedge(src, bm, "chunky");
	if (src)
		FreeBitMap(src);
	if (bm) {
		rowcases(bm, 7, 20, "chunky");
		FreeBitMap(bm);
	}
	/* row 504 crosses into the second segment at x = 128 */
	bm = AllocBitMap(1040, 520, 1, BMF_CLEAR, NULL);
	check(bm != NULL, "AllocBitMap 1040x520x1");
	if (bm) {
		rowcases(bm, 93, 498, "planar split");
		FreeBitMap(bm);
	}
	/* row 204 crosses at x = 512 */
	bm = chunky(640, 400);
	if (bm) {
		rowcases(bm, 477, 199, "chunky split");
		FreeBitMap(bm);
	}
}

/* 640x400 chunky: row 204 crosses into the second segment at x = 512 */
static void bigchunky(void)
{
	static UWORD halftone[] = { 0x3333, 0xCCCC };
	struct RastPort rp;
	struct BitMap *c;
	int x, y, ok;

	c = chunky(640, 400);
	check(c != NULL, "AllocBitMap 640x400 chunky");
	if (c == NULL)
		return;
	check(GFXBM(c)->gb_Half1 == PtrStep(c->Planes[0], 0x10000L),
	    "second half");
	InitRastPort(&rp);
	rp.BitMap = c;
	SetAPen(&rp, 5);
	RectFill(&rp, 508, 203, 515, 205);
	check(byteat(c, 204L * 320 + 253) == 0x00 &&
	    byteat(c, 204L * 320 + 254) == 0x55 &&
	    byteat(c, 204L * 320 + 256) == 0x55 &&
	    byteat(c, 204L * 320 + 258) == 0x00 &&
	    byteat(c, 205L * 320 + 257) == 0x55, "RectFill over the split");
	check(ReadPixel(&rp, 511, 204) == 5 && ReadPixel(&rp, 512, 204) == 5 &&
	    ReadPixel(&rp, 516, 204) == 0, "ReadPixel over the split");
	/* a two-pen pattern over the split, as the Workbench's backfill */
	SetAfPt(&rp, halftone, 1);
	SetABPenDrMd(&rp, 3, 7, JAM2);
	RectFill(&rp, 0, 203, 639, 205);
	SetAfPt(&rp, NULL, 0);
	SetDrMd(&rp, JAM1);
	for (ok = 1, y = 203; y < 206; y++)
		for (x = 0; x < 640; x++)
			ok &= ReadPixel(&rp, x, y) ==
			    (((halftone[y & 1] >> (15 - (x & 15))) & 1) ? 3 : 7);
	check(ok, "pattern over the split");
	SetRast(&rp, 0);
	SetAPen(&rp, 5);
	RectFill(&rp, 508, 203, 515, 205);
	SetAPen(&rp, 3);
	Move(&rp, 500, 204);
	Draw(&rp, 530, 204);
	check(ReadPixel(&rp, 530, 204) == 3 && ReadPixel(&rp, 512, 204) == 3,
	    "Draw over the split");
	/* a copy from the split row to one in the second half, and back */
	BltBitMap(c, 500, 204, c, 101, 300, 40, 1, 0xC0, 0xFF, NULL);
	for (ok = 1, x = 0; x < 40; x++)
		ok &= ReadPixel(&rp, 101 + x, 300) == ReadPixel(&rp, 500 + x, 204);
	check(ok, "BltBitMap from the split row");
	SetRast(&rp, 0);
	BltBitMap(c, 101, 300, c, 490, 204, 40, 1, 0xC0, 0xFF, NULL);
	check(ReadPixel(&rp, 491, 204) == 0, "BltBitMap to the split row");
	FreeBitMap(c);
}

/* 1024x800 mono: rows 512 on are in the second segment */
static void bigplanar(void)
{
	struct RastPort rp;
	struct BitMap *bm;

	bm = AllocBitMap(1024, 800, 1, BMF_CLEAR, NULL);
	check(bm != NULL, "AllocBitMap 1024x800x1");
	if (bm == NULL)
		return;
	InitRastPort(&rp);
	rp.BitMap = bm;
	SetAPen(&rp, 1);
	RectFill(&rp, 8, 510, 23, 513);
	check(byteat(bm, 511L * 128 + 1) == 0xFF &&
	    byteat(bm, 512L * 128 + 2) == 0xFF &&
	    byteat(bm, 514L * 128 + 1) == 0, "RectFill over 64 KB");
	BltBitMap(bm, 8, 510, bm, 100, 790, 16, 4, 0xC0, 0xFF, NULL);
	check(ReadPixel(&rp, 100, 790) == 1 && ReadPixel(&rp, 115, 793) == 1 &&
	    ReadPixel(&rp, 116, 793) == 0, "BltBitMap within 100 KB");
	paint();
	ScrollRaster(&rp, 0, -8, 0, 0, 1023, 799);
	stack(used());
	check(ReadPixel(&rp, 8, 518) == 1 && ReadPixel(&rp, 8, 510) == 0,
	    "ScrollRaster 1024x800");
	FreeBitMap(bm);
}

static void regions(void)
{
	struct Region *r, *s, *t;
	struct Rectangle a, b;
	struct RegionRectangle *rr;
	int n;

	r = NewRegion();
	s = NewRegion();
	check(r != NULL && s != NULL && r->RegionRectangle == NULL, "NewRegion");
	a.MinX = 0; a.MinY = 0; a.MaxX = 9; a.MaxY = 9;
	b.MinX = 5; b.MinY = 5; b.MaxX = 14; b.MaxY = 14;
	check(OrRectRegion(r, &a) && OrRectRegion(r, &b), "OrRectRegion");
	check(r->bounds.MinX == 0 && r->bounds.MaxX == 14 &&
	    r->bounds.MaxY == 14, "region bounds");
	for (n = 0, rr = r->RegionRectangle; rr; rr = rr->Next)
		n++;
	check(n == 3, "region bands");
	check(IsPointInRegion(r, 12, 12) && !IsPointInRegion(r, 12, 2) &&
	    IsPointInRegion(r, 2, 2), "IsPointInRegion");
	check(XorRectRegion(r, &b), "XorRectRegion");
	check(IsPointInRegion(r, 2, 2) && !IsPointInRegion(r, 6, 6) &&
	    !IsPointInRegion(r, 12, 12), "XorRectRegion result");
	check(OrRectRegion(s, &b) && OrRegionRegion(r, s) &&
	    IsPointInRegion(s, 12, 12), "OrRegionRegion");
	/* r is a less b; s is a or b */
	check(AndRegionRegion(r, s) && !IsPointInRegion(s, 6, 6) &&
	    IsPointInRegion(s, 2, 2), "AndRegionRegion");
	t = AndRegionRegionND(r, s);
	check(t != NULL && IsPointInRegion(t, 2, 2) && !IsPointInRegion(t, 12, 12),
	    "AndRegionRegionND");
	if (t)
		DisposeRegion(t);
	check(ClearRectRegion(r, &b) && !IsPointInRegion(r, 6, 6) &&
	    IsPointInRegion(r, 2, 2), "ClearRectRegion");
	AndRectRegion(r, &b);
	check(r->RegionRectangle == NULL, "AndRectRegion");
	check(SetRegion(s, r) && IsPointInRegion(r, 2, 2), "SetRegion");
	check(ClearRegionRegion(s, r) && r->RegionRectangle == NULL,
	    "ClearRegionRegion");
	ClearRegion(s);
	check(s->RegionRectangle == NULL, "ClearRegion");
	DisposeRegion(r);
	DisposeRegion(s);
}

/* Pens 0-3 of a 2-deep ViewPort to share */
static void pens(struct ColorMap *cm, struct ViewPort *vp)
{
	struct PaletteExtra *pe;
	struct TagItem gui[2];
	ULONG rgb[3];
	LONG a, b, c, d;

	check(AttachPalExtra(cm, vp) == 0 && cm->PalExtra != NULL &&
	    AttachPalExtra(cm, vp) == 0, "AttachPalExtra");
	pe = cm->PalExtra;
	if (pe == NULL)
		return;
	check(pe->pe_NFree == 4 && pe->pe_SharableColors == 3 &&
	    pe->pe_FirstFree == 3, "PalExtra");
	a = ObtainPen(cm, -1, 0x12000000, 0x34000000, 0x56000000, 0);
	GetRGB32(cm, a, 1, rgb);
	check(a == 3 && rgb[0] == 0x12121212 && rgb[2] == 0x56565656,
	    "ObtainPen shared");
	b = ObtainPen(cm, -1, 0x12000000, 0x34000000, 0x56000000, 0);
	check(b == a && pe->pe_NShared == 1, "ObtainPen shares a colour");
	gui[0].ti_Tag = OBP_Precision;
	gui[0].ti_Data = PRECISION_GUI;
	gui[1].ti_Tag = TAG_DONE;
	check(ObtainBestPenA(cm, 0x13000000, 0x33000000, 0x57000000, gui) == a &&
	    pe->pe_NFree == 3, "ObtainBestPenA near a shared pen");
	ReleasePen(cm, a);
	check(FindColor(cm, 0xF0000000, 0x80000000, 0x10000000, 15) == 5 &&
	    FindColor(cm, 0x13000000, 0x33000000, 0x57000000, -1) == 3 &&
	    FindColor(cm, 0xF0000000, 0x80000000, 0x10000000, -1) != 5,
	    "FindColor");
	c = ObtainPen(cm, -1, 0xFF000000, 0, 0, PENF_EXCLUSIVE);
	check(c == 2 && pe->pe_NFree == 2, "ObtainPen exclusive");
	d = ObtainPen(cm, 0, 0, 0xFF000000, 0, PENF_EXCLUSIVE);
	check(d == 0, "ObtainPen exclusive, given pen");
	check(ObtainPen(cm, 0, 0, 0, 0, PENF_EXCLUSIVE) == -1 &&
	    ObtainPen(cm, 2, 0, 0, 0, 0) == -1 &&
	    ObtainPen(cm, 4, 0, 0, 0, 0) == -1, "ObtainPen taken pens");
	check(ObtainPen(cm, 1, 0, 0, 0, PENF_EXCLUSIVE | PENF_NO_SETCOLOR) == 1 &&
	    pe->pe_NFree == 0 &&
	    ObtainPen(cm, -1, 0, 0, 0, PENF_EXCLUSIVE) == -1, "ObtainPen, none free");
	ReleasePen(cm, a);
	check(pe->pe_NShared == 1 && pe->pe_NFree == 0, "ReleasePen shared");
	ReleasePen(cm, b);
	ReleasePen(cm, c);
	ReleasePen(cm, d);
	ReleasePen(cm, 1);
	ReleasePen(cm, 1);
	check(pe->pe_NShared == 0 && pe->pe_NFree == 4, "ReleasePen");
	check(ObtainPen(cm, 3, 0, 0, 0, PENF_EXCLUSIVE) == 3, "ObtainPen after ReleasePen");
	ReleasePen(cm, 3);
}

static void tag(struct TagItem *t, Tag id, IPTR data)
{
	t->ti_Tag = id;
	t->ti_Data = data;
}

static void videocontrol(struct ColorMap *cm, struct ViewPort *vp)
{
	struct ViewPortExtra *vpe;
	struct TagItem t[6];
	ULONG q;

	vpe = (struct ViewPortExtra *)GfxNew(VIEWPORT_EXTRA_TYPE);
	if (vpe == NULL)
		return;
	tag(&t[0], VTAG_VIEWPORTEXTRA_SET, (IPTR)vpe);
	tag(&t[1], VTAG_ATTACH_CM_SET, (IPTR)vp);
	tag(&t[2], VTAG_VPMODEID_SET, 0x12345678);
	tag(&t[3], VTAG_SPEVEN_BASE_SET, 3);
	tag(&t[4], VC_NoColorPaletteLoad, TRUE);
	tag(&t[5], TAG_DONE, 0);
	check(VideoControl(cm, t) == 0 && cm->cm_vpe == vpe &&
	    cm->cm_vp == vp && cm->VPModeID == 0x12345678 &&
	    cm->SpriteBase_Even == 3 && (cm->AuxFlags & CMAF_NO_COLOR_LOAD),
	    "VideoControl set");
	check(GfxLookUp(vp) == (struct ExtendedNode *)vpe,
	    "VideoControl associates the ViewPortExtra");
	tag(&t[0], VTAG_VIEWPORTEXTRA_GET, 0);
	tag(&t[1], VTAG_ATTACH_CM_GET, 0);
	tag(&t[2], VTAG_VPMODEID_GET, 0);
	tag(&t[3], VTAG_SPEVEN_BASE_GET, 0);
	tag(&t[4], VTAG_BORDERSPRITE_GET, TRUE);
	tag(&t[5], TAG_DONE, 0);
	check(VideoControl(cm, t) == 0 &&
	    t[0].ti_Tag == VTAG_VIEWPORTEXTRA_SET && t[0].ti_Data == (IPTR)vpe &&
	    t[1].ti_Tag == VTAG_ATTACH_CM_SET && t[1].ti_Data == (IPTR)vp &&
	    t[2].ti_Tag == VTAG_VPMODEID_SET && t[2].ti_Data == 0x12345678 &&
	    t[3].ti_Tag == VTAG_SPEVEN_BASE_SET && t[3].ti_Data == 3 &&
	    t[4].ti_Tag == VTAG_BORDERSPRITE_CLR && t[4].ti_Data == FALSE,
	    "VideoControl get");
	tag(&t[0], VTAG_VPMODEID_CLR, 0);
	tag(&t[1], VTAG_VPMODEID_GET, 0);
	tag(&t[2], VC_NoColorPaletteLoad_Query, (IPTR)&q);
	tag(&t[3], TAG_DONE, 0);
	check(VideoControl(cm, t) == 0 && t[1].ti_Tag == VTAG_VPMODEID_CLR &&
	    t[1].ti_Data == INVALID_ID && q == TRUE, "VideoControl clear");
	tag(&t[0], VTAG_DEFSPRITERESN_GET, 0);
	tag(&t[1], TAG_DONE, 0);
	check(VideoControl(cm, t) == 1, "VideoControl unknown tag");
}

/* RP_Extra is freed by its own RastPort only */
static void deinitrp(void)
{
	struct RastPort rp, copy;
	ULONG avail;

	InitRastPort(&rp);
	avail = AvailMem(MEMF_ANY);
	rp.RP_Extra = AllocVec(16, MEMF_CLEAR);
	if (rp.RP_Extra == NULL)
		return;
	*(struct RastPort **)rp.RP_Extra = &rp;
	copy = rp;
	DeinitRastPort(&copy);
	check(copy.RP_Extra == NULL && AvailMem(MEMF_ANY) < avail,
	    "DeinitRastPort on a copy");
	DeinitRastPort(&rp);
	check(rp.RP_Extra == NULL && AvailMem(MEMF_ANY) == avail,
	    "DeinitRastPort");
}

static void views(void)
{
	struct View v;
	struct ViewPort vp, vp2;
	struct RasInfo ri;
	struct ColorMap *cm;
	struct ViewPortExtra *vpe;
	struct BitMap *bm;
	ULONG rgb[6];
	static ULONG table[] = {
		2L << 16 | 4, 0x11111111, 0x22222222, 0x33333333,
		0xFFFFFFFF, 0x80000000, 0x00000000, 0
	};

	InitView(&v);
	InitVPort(&vp);
	check(v.ViewPort == NULL && vp.Next == NULL && vp.ColorMap == NULL,
	    "InitView, InitVPort");
	cm = GetColorMap(16);
	check(cm != NULL && cm->Count == 16 && cm->Type == COLORMAP_TYPE_V39,
	    "GetColorMap");
	if (cm == NULL)
		return;
	bm = AllocBitMap(32, 8, 2, 0, NULL);
	ri.Next = NULL;
	ri.BitMap = bm;
	ri.RxOffset = ri.RyOffset = 0;
	vp.RasInfo = &ri;
	vp.ColorMap = cm;
	SetRGB32(&vp, 1, 0xAB000000, 0xCD000000, 0xEF000000);
	GetRGB32(cm, 1, 1, rgb);
	check(rgb[0] == 0xABABABAB && rgb[1] == 0xCDCDCDCD &&
	    rgb[2] == 0xEFEFEFEF, "SetRGB32, GetRGB32");
	SetRGB4(&vp, 2, 1, 2, 3);
	check(GetRGB4(cm, 2) == 0x123, "SetRGB4, GetRGB4");
	LoadRGB32(&vp, table);
	GetRGB32(cm, 4, 2, rgb);
	check(rgb[0] == 0x11111111 && rgb[2] == 0x33333333 &&
	    rgb[3] == 0xFFFFFFFF && (rgb[4] >> 24) == 0x80 && rgb[5] == 0,
	    "LoadRGB32");

	vpe = (struct ViewPortExtra *)GfxNew(VIEWPORT_EXTRA_TYPE);
	check(vpe != NULL, "GfxNew");
	if (vpe) {
		GfxAssociate(&vp, (struct ExtendedNode *)vpe);
		check(GfxLookUp(&vp) == (struct ExtendedNode *)vpe &&
		    vpe->ViewPort == &vp, "GfxAssociate, GfxLookUp");
		GfxFree((struct ExtendedNode *)vpe);
		check(GfxLookUp(&vp) == NULL, "GfxFree");
	}
	pens(cm, &vp);
	videocontrol(cm, &vp);
	FreeColorMap(cm);
	check(GfxLookUp(&vp) == NULL, "FreeColorMap frees the ViewPortExtra");

	InitVPort(&vp2);
	check(MakeVPort(&v, &vp2) == MVP_NO_DISPLAY, "MakeVPort, no RasInfo");
	vp2.RasInfo = &ri;
	v.ViewPort = &vp2;
	check(MakeVPort(&v, &vp2) == MVP_OK, "MakeVPort");
	vpe = (struct ViewPortExtra *)GfxLookUp(&vp2);
	check(vpe != NULL && (vpe->Flags & VPXF_FREE_ME), "MakeVPort's ViewPortExtra");
	check(MakeVPort(&v, &vp2) == MVP_OK &&
	    GfxLookUp(&vp2) == (struct ExtendedNode *)vpe, "MakeVPort again");
	check(MrgCop(&v) == MCOP_OK, "MrgCop");
	ScrollVPort(&vp2);
	LoadView(&v);
	check(GfxBase->ActiView == &v, "LoadView");
	LoadView(NULL);
	check(GfxBase->ActiView == NULL, "LoadView(NULL)");
	FreeVPortCopLists(&vp2);
	check(GfxLookUp(&vp2) == NULL, "FreeVPortCopLists");
	FreeBitMap(bm);
}

/*
 * The display database without a display: the C emulator has no HR card,
 * so it is empty and graphics draws in RAM only.
 */
static void modes(void)
{
	struct DisplayInfo di;
	struct TagItem t[3];
	struct ViewPort vp;
	struct RasInfo ri;
	struct BitMap *bm, *bm2;
	struct ColorMap *cm;
	struct View v;

	check(FindDisplayInfo(HRMONO_800_ID) == NULL &&
	    FindDisplayInfo(INVALID_ID) == NULL, "FindDisplayInfo, no display");
	check(NextDisplayInfo(INVALID_ID) == INVALID_ID, "NextDisplayInfo, no display");
	check(GetDisplayInfoData(NULL, (UBYTE *)&di, sizeof(di), DTAG_DISP,
	    HRCOLOR_ID) == 0, "GetDisplayInfoData, no display");
	check(ModeNotAvailable(HRMONO_800_ID) == DI_AVAIL_NOMONITOR,
	    "ModeNotAvailable, no display");
	tag(&t[0], BIDTAG_Depth, 1);
	tag(&t[1], TAG_DONE, 0);
	check(BestModeIDA(t) == INVALID_ID && BestModeIDA(NULL) == INVALID_ID,
	    "BestModeIDA, no display");
	tag(&t[0], BMATags_DisplayID, HRMONO_800_ID);
	check(AllocBitMap(1024, 800, 1, BMF_CLEAR | EXTTAGS,
	    (struct BitMap *)t) == NULL, "AllocBitMap of a mode not there");

	/* GetVPModeID: the ColorMap's, else the bitmap's */
	bm = AllocBitMap(64, 16, 1, BMF_CLEAR, NULL);
	bm2 = AllocBitMap(64, 16, 1, BMF_CLEAR, NULL);
	cm = GetColorMap(2);
	if (bm == NULL || bm2 == NULL || cm == NULL)
		return;
	InitVPort(&vp);
	ri.Next = NULL;
	ri.BitMap = bm;
	ri.RxOffset = ri.RyOffset = 0;
	vp.RasInfo = &ri;
	check(GetVPModeID(&vp) == INVALID_ID, "GetVPModeID of a RAM bitmap");
	vp.ColorMap = cm;
	cm->VPModeID = HRCOLOR_ID;
	check(GetVPModeID(&vp) == HRCOLOR_ID, "GetVPModeID of the ColorMap");
	vp.ColorMap = NULL;

	/* A View of RAM bitmaps: nothing shown, nothing copied */
	InitView(&v);
	v.ViewPort = &vp;
	check(MakeVPort(&v, &vp) == MVP_OK && MrgCop(&v) == MCOP_OK, "MakeVPort, MrgCop");
	LoadView(&v);
	ChangeVPBitMap(&vp, bm2, NULL);
	check(ri.BitMap == bm2 && GfxBase->ActiView == &v, "ChangeVPBitMap");
	ri.RxOffset = 8;
	ScrollVPort(&vp);
	WaitTOF();
	WaitBOVP(&vp);
	LoadView(NULL);
	FreeVPortCopLists(&vp);
	FreeCprList(NULL);
	FreeColorMap(cm);
	FreeBitMap(bm2);
	FreeBitMap(bm);
}

/* A pixel of plane 0 as it is in memory, pointer and all */
static int raw(struct BitMap *bm, int x, int y)
{
	return (bm->Planes[0][y * bm->BytesPerRow + (x >> 3)] >> (7 - (x & 7))) & 1;
}

/*
 * The pointer on a mono bitmap: pens 17-19 are 1, 0 and 1 there.
 * ReadPixel() sees what is under it.
 */
static void pointer(void)
{
	struct ViewPort vp;
	struct RasInfo ri;
	struct RastPort rp;
	struct BitMap *bm;
	struct ExtSprite *es;
	static struct TagItem old[] = {
		{ SPRITEA_OldDataFormat, TRUE }, { TAG_DONE, 0 }
	};
	/* rows: colour 2, colour 1, colour 3 over two pixels */
	static UWORD data[] = { 0, 0, 0x0000, 0xC000, 0xC000, 0x0000,
	    0xC000, 0xC000, 0, 0 };
	static UBYTE pixtab[256];
	int x;

	bm = AllocBitMap(64, 32, 1, BMF_CLEAR, NULL);
	es = AllocSpriteDataA((struct BitMap *)data, old);
	check(es != NULL && es->es_SimpleSprite.height == 3,
	    "AllocSpriteDataA");
	if (bm == NULL || es == NULL)
		return;
	InitVPort(&vp);
	ri.Next = NULL;
	ri.BitMap = bm;
	ri.RxOffset = ri.RyOffset = 0;
	vp.RasInfo = &ri;
	InitRastPort(&rp);
	rp.BitMap = bm;
	SetAPen(&rp, 1);
	RectFill(&rp, 8, 5, 9, 5);

	check(ChangeExtSpriteA(&vp, NULL, es, NULL), "ChangeExtSpriteA");
	MoveSprite(&vp, &es->es_SimpleSprite, 8, 4);
	check(raw(bm, 8, 4) == 0 && raw(bm, 9, 6) == 1 &&
	    raw(bm, 8, 5) == 1, "pointer drawn");
	/* drawing under the pointer leaves it on top */
	RectFill(&rp, 0, 4, 20, 4);
	SetAPen(&rp, 0);
	RectFill(&rp, 0, 5, 20, 5);
	check(raw(bm, 8, 4) == 0 && raw(bm, 12, 4) == 1 &&
	    raw(bm, 9, 5) == 1, "pointer over drawing");
	check(ReadPixel(&rp, 8, 4) == 1 && ReadPixel(&rp, 9, 5) == 0,
	    "ReadPixel under the pointer");
	paint();
	MoveSprite(&vp, &es->es_SimpleSprite, 40, 20);
	stack(used());
	check(raw(bm, 8, 4) == 1 && raw(bm, 9, 5) == 0 &&
	    raw(bm, 9, 6) == 0 && raw(bm, 40, 21) == 1,
	    "MoveSprite restores");
	/* through a pixel table that gives pens 17 and 19 one pixel */
	for (x = 0; x < 256; x++)
		pixtab[x] = x == 18 || (x & 1);
	pixtab[17] = pixtab[19] = 0;
	GFXBM(bm)->gb_PixTab = pixtab;
	SetAPen(&rp, 1);
	RectFill(&rp, 30, 11, 50, 16);
	MoveSprite(&vp, &es->es_SimpleSprite, 40, 10);
	check(raw(bm, 40, 10) == 1 && raw(bm, 42, 10) == 0 &&
	    raw(bm, 40, 11) == 0 && raw(bm, 41, 12) == 0 &&
	    raw(bm, 42, 12) == 1 && raw(bm, 40, 13) == 1,
	    "pointer through a pixel table");
	MoveSprite(&vp, &es->es_SimpleSprite, 40, 20);
	GFXBM(bm)->gb_PixTab = NULL;
	check(ChangeExtSpriteA(&vp, es, NULL, NULL) &&
	    raw(bm, 40, 21) == 0, "pointer taken away");
	FreeSpriteData(es);
	FreeBitMap(bm);
}

int main(void)
{
	GfxBase = (struct GfxBase *)OpenLibrary(GRAPHICSNAME, 45L);
	if (GfxBase == NULL) {
		KPutStr("FAIL graphics: OpenLibrary\n");
		return 1;
	}
	check(GfxBase->LibNode.lib_Version == 45, "version");
	check(GfxBase->DefaultFont != NULL, "DefaultFont");
	planar();
	text();
	blits();
	scroll();
	chunky4();
	bigchunky();
	bigplanar();
	rows();
	flips();
	regions();
	views();
	modes();
	deinitrp();
	pointer();
	xKPrintf("graphics: deepest call %d bytes of stack\n", maxstack);
	CloseLibrary((struct Library *)GfxBase);
	if (fails != 0)
		return 1;
	KPutStr("PASS graphics\n");
	return 0;
}
