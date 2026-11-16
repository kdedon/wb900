/* SPDX-License-Identifier: MIT */
/*
 * lc.gfxdriver: the LC card's 8568 as graphics.library's display, 1 bit
 * deep in bitmap mode: 640 or 320 pixels wide (double width), 200 lines
 * at 60 Hz, 256 at 50 Hz, or 400 or 480 interlaced.  The CPU reaches the
 * chip's RAM only through its registers, so the display bitmap is a
 * shadow in RAM, drawn by graphics' planar operations.  Each operation
 * marks the bytes it changed, and gd_Flush pushes them through R31.
 * Fills of whole bytes and copies that keep the bit alignment run as the
 * chip's block fill and copy.
 *
 *   shadow (<= 38 KB) --dirty spans--> push R18/R19, R31 ---> VRAM slot
 *        \__ fill / aligned copy ---> R30 blocks -----------/
 *
 * VRAM holds as many bitmaps of the mode as fit, each in a slot, with a
 * row of copy scratch at the end.  Screens behind keep their slots and
 * are pushed as they are drawn, so bringing one to the front is a write
 * of R12/R13.  An interlaced slot holds the even rows, then the odd ones:
 * the chip loads R12/R13 once a frame and fetches the odd field on from
 * where the even one ended.
 *
 * Every port sequence runs with interrupts off, in pieces of about 1 ms
 * at most, each ended by a status read that finds the chip ready: the
 * 8568 raises the NVI on every ready edge until its status is read.
 */
#include <exec/types.h>
#include <exec/resident.h>
#include <exec/libraries.h>
#include <exec/memory.h>
#include <exec/semaphores.h>
#include <proto/exec.h>
#include <graphics/gfxdriver.h>
#include <graphics/hrmodeid.h>
#include <graphics/rastport.h>
#include <resources/framebuf.h>

#include "lcport.h"

#define LCNAME		"lc.gfxdriver"
#define LCVERSION	45

static struct LCPort lc_lp;
#define LP	(&lc_lp)

struct LCBase {
	struct Library	lb_Lib;
	APTR		lb_Seg;
};

APTR FrameBufBase;

static char lcname[] = LCNAME;
static char lcid[] = "lc.gfxdriver 45.4 (16.11.2026)";
static char lcowner[] = "graphics.library";

/*
 * Mode i is double width when i & 1; i >> 1 picks the lines: 200, 256,
 * 400 or 480.  The ticks are a pixel's size on a 4:3 screen, 22 for 640
 * pixels across.
 */
static const struct GfxMode lc_modes[] = {
	{ LCHIRES_ID, 640, 200, 1, GDF_PLANAR, 22, 53, "LC:640x200" },
	{ LCLORES_ID, 320, 200, 1, GDF_PLANAR, 44, 53, "LC:320x200" },
	{ LCHIRESPAL_ID, 640, 256, 1, GDF_PLANAR, 22, 41, "LC:640x256" },
	{ LCLORESPAL_ID, 320, 256, 1, GDF_PLANAR, 44, 41, "LC:320x256" },
	{ LCHIRESLACE_ID, 640, 400, 1, GDF_PLANAR, 22, 26,
	    "LC:640x400 Interlace" },
	{ LCLORESLACE_ID, 320, 400, 1, GDF_PLANAR, 44, 26,
	    "LC:320x400 Interlace" },
	{ LCHIRESLACE480_ID, 640, 480, 1, GDF_PLANAR, 22, 22,
	    "LC:640x480 Interlace" },
	{ LCLORESLACE480_ID, 320, 480, 1, GDF_PLANAR, 44, 22,
	    "LC:320x480 Interlace" }
};
#define LC_NMODES	8
static struct GfxDriver *lc_fmt;	/* graphics' planar operations */
static BOOL lc_alloc, lc_shown;

/* The bitmaps in VRAM, slot i at i * lc_size; lc_cur is the port's */
#define LC_SLOTS	8
static struct BitMap *lc_slot[LC_SLOTS];
static ULONG lc_used[LC_SLOTS];		/* when last in front */
static UWORD lc_nslots, lc_size, lc_cur, lc_front;
static ULONG lc_age;
static UWORD lc_u;		/* rows from one in VRAM to the next: 2 interlaced */
static struct SignalSemaphore lc_lock;	/* the chip's port sequences */
static UBYTE lc_fg = 0xF, lc_bg = 0x2;	/* RGBI of pixels 1 and 0 */

/*
 * The registers for bitmap mode, R0-R37: 80x25 cells of 8 lines, as the
 * text console's timing; bitmap at 0x0000, attributes off (R25), 64 KB
 * RAM (R28), cursor off (R10), blank (R26), 5 refresh cycles a line
 * (R36), positive syncs (R37).  lc_hregs and lc_vregs replace some for
 * the other modes.
 */
static const UBYTE lc_regs[38] = {
	0x7E, 0x50, 0x66, 0x49, 0x20, 0x00, 0x19, 0x1D,
	0x00, 0x07, 0x20, 0x07, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x78, 0x08,
	0x00, 0x87, 0x00, 0x00, 0x10, 0x07, 0x00, 0x00,
	0x00, 0x00, 0x7D, 0x64, 0x05, 0xC0
};

/*
 * R0, R1, R2, R22, R25, R34, R35: 80 columns of 8 dots in 63.5 us, or
 * 40 of 16 in 64 us, the chip's clock halved (R25 bit 4) and a column
 * shown as 8 doubled pixels (R22 = 0x89)
 */
static const UBYTE lc_hregs[2][7] = {
	{ 0x7E, 0x50, 0x66, 0x78, 0x87, 0x7D, 0x64 },
	{ 0x3F, 0x28, 0x33, 0x89, 0x97, 0x3E, 0x32 }
};
static const UBYTE lc_hnum[7] = { 0, 1, 2, 22, 25, 34, 35 };

/*
 * R4-R7: 25 rows shown of 33 (264 lines, 60 Hz), or 32 of 39 (312
 * lines, 50 Hz); vsync 4 rows before the top.  Interlaced, rows of 16
 * lines count 8 a field: 25 or 30 rows of 32 and 13 half lines (525
 * lines, 60 fields a second); vsync at row 29, or right below 480.
 */
static const UBYTE lc_vregs[4][4] = {
	{ 0x20, 0x00, 0x19, 0x1D },
	{ 0x26, 0x00, 0x20, 0x23 },
	{ 0x1F, 0x0D, 0x19, 0x1D },
	{ 0x1F, 0x0D, 0x1E, 0x1E }
};

/* The colours of RGBI 0-15 (R 8, G 4, B 2, I 1) */
static const UBYTE lc_rgbi[16][3] = {
	{ 0x00, 0x00, 0x00 }, { 0x4E, 0x4E, 0x4E }, { 0x00, 0x00, 0xC4 },
	{ 0x4E, 0x4E, 0xDC }, { 0x00, 0xC4, 0x00 }, { 0x4E, 0xDC, 0x4E },
	{ 0x00, 0xC4, 0xC4 }, { 0x4E, 0xF3, 0xF3 }, { 0xC4, 0x00, 0x00 },
	{ 0xDC, 0x4E, 0x4E }, { 0xC4, 0x00, 0xC4 }, { 0xF3, 0x4E, 0xF3 },
	{ 0xC4, 0x7E, 0x00 }, { 0xF3, 0xF3, 0x4E }, { 0xC4, 0xC4, 0xC4 },
	{ 0xFF, 0xFF, 0xFF }
};

#define MIN(a, b)	((a) < (b) ? (a) : (b))
#define MAX(a, b)	((a) > (b) ? (a) : (b))

/* Port pieces */

static void lc_enter(void)
{
	ObtainSemaphore(&lc_lock);
	Disable();
	lc_lp.lp_Budget = LCP_PIECE;
}

static void lc_leave(void)
{
	lcv_ready(LP);
	Enable();
	ReleaseSemaphore(&lc_lock);
}

/* Mode m, of rows rows, with slot 0 cleared */
static void lc_init(UWORD m, UWORD rows)
{
	UBYTE r[38];
	UWORD i, bpr = lc_lp.lp_BPR;

	for (i = 0; i < 38; i++)
		r[i] = lc_regs[i];
	for (i = 0; i < 7; i++)
		r[lc_hnum[i]] = lc_hregs[m & 1][i];
	for (i = 0; i < 4; i++)
		r[4 + i] = lc_vregs[m >> 1][i];
	lc_lp.lp_Step[0] = lc_lp.lp_Step[1] = bpr;
	if (m >= 4) {
		/* interlaced 16-line rows */
		r[8] = 0x03;
		r[9] = 0x0F;
		r[23] = 0x10;
		lc_lp.lp_Step[0] = rows / 2 * bpr;
		lc_lp.lp_Step[1] = bpr - lc_lp.lp_Step[0];
	}
	lc_u = m >= 4 ? 2 : 1;
	lc_lp.lp_OK = TRUE;
	lc_lp.lp_UA = lc_lp.lp_BA = LCP_NOADDR;
	lc_lp.lp_Y0 = LCP_ROWS;
	lc_lp.lp_Y1 = -1;
	for (i = 0; i < LCP_ROWS; i++) {
		lc_lp.lp_Lo[i] = 0xFF;
		lc_lp.lp_Hi[i] = 0;
		lc_lp.lp_Cx[i] = 0;
	}
	for (i = 0; i < sizeof(lc_lp.lp_Bits); i++)
		lc_lp.lp_Bits[i] = 0;
	lc_enter();
	lcv_wr(LP, 28, r[28]);
	for (i = 0; i < 38; i++)
		if (i != 16 && i != 17 && i != 18 && i != 19 && i != 28 &&
		    i != 30 && i != 31)
			lcv_wr(LP, i, r[i]);
	lcv_fill(LP, 0, 0, 0, rows * bpr, 1, 0);
	lc_leave();
}

/* Pixels x0-x1 of rows y0-y1 changed */
static void lc_mark(WORD y0, WORD y1, WORD x0, WORD x1)
{
	lcv_mark(LP, y0, y1, x0 >> 3, x1 >> 3, TRUE);
}

static void lc_Flush(struct GfxDriver *gd, struct BitMap *bm)
{
	if (lc_lp.lp_Y1 < 0)
		return;
	lc_enter();
	lcv_flush(LP);
	lc_leave();
}

/* Slot i becomes the port's, another's dirty bytes pushed first */
static void lc_port(UWORD i)
{
	if (i != lc_cur)
		lc_Flush(NULL, NULL);
	lc_cur = i;
	lc_lp.lp_Shadow = lc_slot[i]->Planes[0];
	lc_lp.lp_Base = i * lc_size;
}

/* bm's slot becomes the port's; FALSE when bm has none */
static BOOL lc_target(struct BitMap *bm)
{
	UWORD i;

	if (lc_slot[lc_cur] == bm)
		return TRUE;
	for (i = 0; i < lc_nslots && lc_slot[i] != bm; i++)
		;
	if (i == lc_nslots)
		return FALSE;
	lc_port(i);
	return TRUE;
}

/* Whole rows y0-y1 of v: a block for each field's rows */
static void lc_rows(WORD y0, WORD y1, WORD v)
{
	WORD y;

	for (y = y0; y <= y1 && y < y0 + lc_u; y++)
		lcv_fill(LP, y, 0, v, ((y1 - y) / lc_u + 1) * lc_lp.lp_BPR, 1, 0);
}

/* Drawing */

/*
 * The byte a fill leaves in every byte of row y: -1 for none, -2 when it
 * changes nothing
 */
static WORD lc_fillbyte(const struct GfxPens *p, WORD y)
{
	UWORD t = p->gp_Ptrn ? p->gp_Ptrn[y & p->gp_PtrnMask] : 0xFFFF;
	UBYTE f = (p->gp_APen & 1) ? 0xFF : 0, b = (p->gp_BPen & 1) ? 0xFF : 0;

	if (p->gp_DrMd & INVERSVID)
		t = ~t;
	if (p->gp_DrMd & COMPLEMENT)
		return -1;
	if (p->gp_DrMd & JAM2) {
		if ((t >> 8) != (t & 0xFF) && f != b)
			return -1;
		t >>= 8;
		return ((t & f) | (~t & b)) & 0xFF;
	}
	if (t == 0xFFFF)
		return f;
	return t == 0 ? -2 : -1;
}

/*
 * Rows of one byte value fill as blocks, consecutive full rows as one;
 * partial edge bytes go out beside each row's block, without a seek.
 */
static void lc_Fill(struct BitMap *bm, const struct Rectangle *r,
    const struct GfxPens *p)
{
	UWORD fb0 = (r->MinX + 7) >> 3, fb1 = ((r->MaxX + 1) >> 3) - 1;
	UWORD n, edges, bpr = lc_lp.lp_BPR;
	WORD y, y1, v;

	lc_fmt->gd_Fill(bm, r, p);
	if (!(p->gp_Mask & 1) || !lc_target(bm))
		return;
	if ((WORD)fb1 < (WORD)fb0 + 3 || (p->gp_DrMd & COMPLEMENT)) {
		lc_mark(r->MinY, r->MaxY, r->MinX, r->MaxX);
		return;
	}
	n = fb1 - fb0 + 1;
	edges = ((r->MinX & 7) != 0) | ((r->MaxX & 7) != 7) << 1;
	lc_enter();
	for (y = r->MinY; y <= r->MaxY; y = y1) {
		v = lc_fillbyte(p, y);
		y1 = y + 1;
		if (p->gp_Ptrn == NULL)
			y1 = r->MaxY + 1;
		else
			while (y1 <= r->MaxY && lc_fillbyte(p, y1) == v)
				y1++;
		if (v == -1) {
			lc_mark(y, y1 - 1, r->MinX, r->MaxX);
		} else if (v >= 0) {
			lcv_mark(LP, y, y1 - 1, fb0 - (edges & 1),
			    fb1 + (edges >> 1), FALSE);
			if (n == bpr)
				lc_rows(y, y1 - 1, v);
			else
				lcv_fill(LP, y, fb0, v, n, y1 - y, edges);
		}
	}
	lc_leave();
}

static void lc_Template(struct BitMap *bm, const struct Rectangle *r,
    const UBYTE *src, UWORD srcx, UWORD srcmod, const struct GfxPens *p)
{
	lc_fmt->gd_Template(bm, r, src, srcx, srcmod, p);
	if ((p->gp_Mask & 1) && lc_target(bm))
		lc_mark(r->MinY, r->MaxY, r->MinX, r->MaxX);
}

/*
 * The aligned copy of r's whole bytes fb0-fb1 from (sx, sy), on the
 * display as already in the shadow, in the order overlaps need; partial
 * edge bytes go out beside each row's block.  Whole rows moved within
 * their field copy as a block a field; others go a row at a time.
 */
static void lc_vcopy(const struct Rectangle *r, WORD sx, WORD sy,
    UWORD fb0, UWORD fb1)
{
	WORD bpr = lc_lp.lp_BPR, u = lc_u;
	UWORD n = fb1 - fb0 + 1, h, k, edges;
	WORD dy = sy - r->MinY, dx = (sx >> 3) - (r->MinX >> 3), y, f;
	WORD q = -dy / u;

	edges = ((r->MinX & 7) != 0) | ((r->MaxX & 7) != 7) << 1;
	lc_enter();
	if (n == bpr && dy % u == 0) {
		for (f = r->MinY; f <= r->MaxY && f < r->MinY + u; f++) {
			h = (r->MaxY - f) / u + 1;
			if (dy >= 0) {
				lcv_copy(LP, f, 0, f + dy, 0, h * bpr, 1, 1, 0);
				continue;
			}
			/* bottom up, in blocks that do not overlap their source */
			for (y = f + h * u; h > 0; h -= k) {
				k = h < q ? h : q;
				y -= k * u;
				lcv_copy(LP, y, 0, y + dy, 0, k * bpr, 1, 1, 0);
			}
		}
	} else {
		/* a move right within the row goes through scratch */
		if (dy == 0 && dx < 0 && -dx < n)
			edges |= 4;
		y = dy < 0 ? r->MaxY : r->MinY;
		lcv_copy(LP, y, fb0, y + dy, fb0 + dx, n, r->MaxY - r->MinY + 1,
		    dy < 0 ? -1 : 1, edges);
	}
	lc_leave();
}

static void lc_Copy(struct BitMap *src, WORD sx, WORD sy, struct BitMap *dst,
    const struct Rectangle *r, UBYTE minterm, UBYTE mask)
{
	UWORD fb0 = (r->MinX + 7) >> 3, fb1 = ((r->MaxX + 1) >> 3) - 1;

	if (!(mask & 1) || !lc_target(dst)) {
		lc_fmt->gd_Copy(src, sx, sy, dst, r, minterm, mask);
		return;
	}
	if (src == dst && minterm == 0xC0 && ((sx ^ r->MinX) & 7) == 0 &&
	    (WORD)fb1 >= (WORD)fb0 + 3) {
		lc_Flush(NULL, dst);
		lc_fmt->gd_Copy(src, sx, sy, dst, r, minterm, mask);
		lc_vcopy(r, sx, sy, fb0, fb1);
		return;
	}
	lc_fmt->gd_Copy(src, sx, sy, dst, r, minterm, mask);
	lc_mark(r->MinY, r->MaxY, r->MinX, r->MaxX);
}

static void lc_WritePixel(struct BitMap *bm, WORD x, WORD y,
    const struct GfxPens *p)
{
	lc_fmt->gd_WritePixel(bm, x, y, p);
	if ((p->gp_Mask & 1) && lc_target(bm))
		lc_mark(y, y, x, x);
}

/*
 * Row y of a line from (x0, y0) down to (x1, y1) has its pixels between
 * the line's x on the rows above and below, one pixel wider.
 */
static void lc_Line(struct BitMap *bm, const struct Rectangle *clip,
    WORD x0, WORD y0, WORD x1, WORD y1, UWORD ptrn, const struct GfxPens *p)
{
	WORD t, dx, dy, sx, q, rem, e, k, xp, xc, xn, a, b, xmin, xmax;

	lc_fmt->gd_Line(bm, clip, x0, y0, x1, y1, ptrn, p);
	if (!(p->gp_Mask & 1) || !lc_target(bm))
		return;
	if (y1 < y0) {
		t = x0; x0 = x1; x1 = t;
		t = y0; y0 = y1; y1 = t;
	}
	xmin = MAX(MIN(x0, x1), clip->MinX);
	xmax = MIN(MAX(x0, x1), clip->MaxX);
	if (xmin > xmax)
		return;
	dy = y1 - y0;
	if (dy == 0) {
		if (y0 >= clip->MinY && y0 <= clip->MaxY)
			lc_mark(y0, y0, xmin, xmax);
		return;
	}
	dx = x1 - x0;
	sx = dx < 0 ? -1 : 1;
	if (dx < 0)
		dx = -dx;
	q = dx / dy * sx;
	rem = dx % dy;
	e = 0;
	xp = xc = x0;
	xn = x0 + q;
	for (k = 0; k <= dy; k++) {
		/* xn: the line's x on row k + 1 */
		if (k > 0) {
			xp = xc;
			xc = xn;
			xn += q;
		}
		e += rem;
		if (e >= dy) {
			e -= dy;
			xn += sx;
		}
		if (y0 + k < clip->MinY || y0 + k > clip->MaxY)
			continue;
		a = MIN(xp, xn) - 1;
		b = MAX(xp, xn) + 1;
		if (a < xmin)
			a = xmin;
		if (b > xmax)
			b = xmax;
		lc_mark(y0 + k, y0 + k, a, b);
	}
}

static void lc_WriteRow(struct BitMap *bm, WORD x, WORD y, UWORD n,
    const UBYTE *pix, UBYTE mask)
{
	lc_fmt->gd_WriteRow(bm, x, y, n, pix, mask);
	if ((mask & 1) && n > 0 && lc_target(bm))
		lc_mark(y, y, x, x + n - 1);
}

/* The display */

static void lc_colours(void)
{
	lc_enter();
	lcv_wr(LP, 26, lc_shown ? (lc_fg << 4) | lc_bg : 0);
	lc_leave();
}

/* Pens 0 and 1 to the nearest RGBI; the others keep pixel n & 1 */
static void lc_SetColor(struct GfxDriver *gd, UWORD n, UBYTE r, UBYTE g,
    UBYTE b)
{
	UWORD i, best = 0;
	ULONG d, bd = 0xFFFFFFFFL;
	WORD dr, dg, db;

	if (n > 1)
		return;
	for (i = 0; i < 16; i++) {
		dr = (WORD)lc_rgbi[i][0] - r;
		dg = (WORD)lc_rgbi[i][1] - g;
		db = (WORD)lc_rgbi[i][2] - b;
		d = (LONG)dr * dr + (LONG)dg * dg + (LONG)db * db;
		if (d < bd) {
			bd = d;
			best = i;
		}
	}
	if (n == 0)
		lc_bg = best;
	else
		lc_fg = best;
	if (lc_alloc)
		lc_colours();
}

static BOOL lc_AllocDisplay(struct GfxDriver *gd, const struct GfxMode *m,
    struct BitMap *bm)
{
	struct GfxBitMap *gbm = (struct GfxBitMap *)bm;
	UWORD bpr = m->gm_Width >> 3, i;
	ULONG size = (ULONG)bpr * m->gm_Height;

	if (lc_alloc || m < lc_modes || m >= lc_modes + LC_NMODES)
		return FALSE;
	bm->Planes[0] = AllocMem(size, MEMF_ANY | MEMF_CLEAR);
	if (bm->Planes[0] == NULL)
		return FALSE;
	if (!lc_shown && AllocFB(lcowner) != NULL) {
		FreeMem(bm->Planes[0], size);
		return FALSE;
	}
	lc_alloc = TRUE;
	/* slots start on a 256-byte boundary, so R12 alone moves the start */
	lc_size = (size + 255) & ~255L;
	lc_nslots = MIN(LCP_SCRATCH / lc_size, LC_SLOTS);
	for (i = 0; i < LC_SLOTS; i++)
		lc_slot[i] = NULL;
	lc_slot[0] = bm;
	lc_front = lc_cur = 0;
	lc_used[0] = ++lc_age;
	lc_lp.lp_Shadow = bm->Planes[0];
	lc_lp.lp_Base = 0;
	lc_lp.lp_BPR = bpr;
	gbm->gb_PlaneSize = size;
	lc_init(m - lc_modes, m->gm_Height);
	bm->BytesPerRow = bpr;
	bm->Rows = m->gm_Height;
	bm->Depth = 1;
	bm->pad = GFXBM_MAGIC;
	bm->Flags = BMF_DISPLAYABLE | BMF_STANDARD;
	gbm->gb_Driver = gd;
	gbm->gb_Flags = GBF_DISPLAY;
	gbm->gb_ModeID = m->gm_ModeID;
	lc_colours();
	return TRUE;
}

/*
 * The display bitmap, the bitmaps behind going back to RAM; or one
 * behind, its slot freed
 */
static void lc_FreeDisplay(struct GfxDriver *gd, struct BitMap *bm)
{
	UWORD i;

	for (i = 0; i < lc_nslots && lc_slot[i] != bm; i++)
		;
	if (!(GFXBM(bm)->gb_Flags & GBF_DISPLAY)) {
		if (i == lc_cur)
			lc_port(lc_front);
		if (i < lc_nslots)
			lc_slot[i] = NULL;
	} else {
		for (i = 0; i < lc_nslots; i++) {
			if (lc_slot[i] && lc_slot[i] != bm)
				GFXBM(lc_slot[i])->gb_Driver = lc_fmt;
			lc_slot[i] = NULL;
		}
		lc_nslots = 0;
		lc_alloc = FALSE;
		if (!lc_shown)
			FreeFB();
	}
	FreeMem(bm->Planes[0], GFXBM(bm)->gb_PlaneSize);
	bm->Planes[0] = NULL;
}

/*
 * bm to the front.  Without a slot it takes a free one, else the one
 * longest behind, else the front's, and is pushed whole.  The bitmap that
 * was in front keeps its slot and this table.
 */
static void lc_Adopt(struct GfxDriver *gd, struct BitMap *bm)
{
	struct BitMap *old = lc_slot[lc_front];
	UWORD i, k;

	for (i = 0; i < lc_nslots && lc_slot[i] != bm; i++)
		;
	if (i == lc_nslots) {
		for (i = 0; i < lc_nslots && lc_slot[i]; i++)
			;
		if (i == lc_nslots) {
			i = lc_front;
			for (k = 0; k < lc_nslots; k++)
				if (k != lc_front &&
				    (i == lc_front || lc_used[k] < lc_used[i]))
					i = k;
			GFXBM(lc_slot[i])->gb_Driver = lc_fmt;
		}
		lc_Flush(gd, bm);
		lc_slot[i] = bm;
		lc_port(i);
		lc_mark(0, bm->Rows - 1, 0, (bm->BytesPerRow << 3) - 1);
		lc_Flush(gd, bm);
	}
	if (lc_slot[lc_front] == old)
		GFXBM(old)->gb_Driver = gd;
	lc_front = i;
	lc_used[i] = ++lc_age;
	lc_enter();
	lcv_wr(LP, 12, (UWORD)(i * lc_size) >> 8);
	lc_leave();
}

static BOOL lc_Show(struct GfxDriver *gd, struct BitMap *bm)
{
	if (bm) {
		lc_shown = TRUE;
		lc_colours();
	} else if (lc_shown) {
		lc_shown = FALSE;
		if (lc_alloc)
			lc_colours();
		else
			FreeFB();
	}
	return TRUE;
}

static struct GfxDriver lc_driver = {
	{ NULL, NULL, 0, 0, "lc" },
	GDF_PLANAR,
	lc_Fill, lc_Template, lc_Copy, NULL, lc_WritePixel, lc_Line, NULL,
	lc_WriteRow,
	lc_modes, LC_NMODES,
	lc_SetColor,
	NULL,
	lc_AllocDisplay,
	lc_FreeDisplay,
	lc_Show,
	lc_Flush,
	NULL,
	lc_Adopt
};

struct GfxDriver *LC_5(struct LCBase *base, struct GfxDriver **fmts)
{
	if (FrameBufBase == NULL)
		FrameBufBase = OpenResource(FRAMEBUFNAME);
	if (FrameBufBase == NULL || GetFBInfo()->fbi_Card != FBC_LC)
		return NULL;
	lc_fmt = fmts[GDF_PLANAR - 1];
	lc_driver.gd_ReadPixel = lc_fmt->gd_ReadPixel;
	lc_driver.gd_ReadRow = lc_fmt->gd_ReadRow;
	return &lc_driver;
}

static struct Library *lcinit(struct Library *lib, APTR seg, APTR sysbase)
{
	lib->lib_Node.ln_Name = lcname;
	lib->lib_IdString = (APTR)lcid;
	lib->lib_Version = LCVERSION;
	lib->lib_Revision = 4;
	((struct LCBase *)lib)->lb_Seg = seg;
	InitSemaphore(&lc_lock);
	return lib;
}

struct Library *lcopen(struct Library *lib, ULONG ver)
{
	lib->lib_OpenCnt++;
	return lib;
}

APTR lcclose(struct Library *lib)
{
	lib->lib_OpenCnt--;
	return NULL;
}

/* graphics.library keeps it open, and its bitmap may be on the display */
APTR lcexpunge(struct Library *lib)
{
	return NULL;
}

APTR lcnull(void)
{
	return NULL;
}

extern APTR LCFuncs[];

static struct {
	ULONG it_Size;
	APTR it_Funcs, it_Struct, it_Init;
} lcinittab = {
	sizeof(struct LCBase), (APTR)LCFuncs, NULL, (APTR)lcinit
};

struct Resident lctag = {
	RTC_MATCHWORD, &lctag, (APTR)(&lctag + 1), RTF_AUTOINIT, LCVERSION,
	NT_LIBRARY, 0, lcname, lcid, (APTR)&lcinittab
};
