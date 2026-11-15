/* SPDX-License-Identifier: MIT */
/*
 * lc.gfxdriver: the LC card's 8568 as graphics.library's display, 640x200
 * mono in bitmap mode.  The CPU reaches the chip's RAM only through its
 * registers, so the display bitmap is a shadow in RAM, drawn by graphics'
 * planar operations.  Each operation marks the bytes it changed, and
 * gd_Flush pushes them through R31.  Fills of whole bytes and copies
 * that keep the bit alignment run as the chip's block fill and copy.
 *
 *   shadow (16000 B) --dirty spans--> push R18/R19, R31 ---> VRAM 0x0000
 *        \__ fill / aligned copy ---> R30 blocks ---------/  (scratch 0x4000)
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

#define LCNAME		"lc.gfxdriver"
#define LCVERSION	45

#define LC_W		640
#define LC_H		200
#define LC_BPR		80
#define LC_SIZE		((ULONG)LC_BPR * LC_H)
#define V_SCRATCH	0x4000		/* free VRAM for overlapping copies */
#define NOADDR		0xFFFF		/* an update address never used */

/*
 * Estimated cost of each port action in microseconds at 6 MHz, worst
 * case on display lines, and the most a piece with interrupts off may take.
 */
#define US_PUSH		15
#define US_SEEK		25
#define US_REG		10
#define US_FILL		2
#define US_COPY		4
#define US_PIECE	1000
#define BLOCK		255		/* bytes an R30 block takes at most */

/* Dirty spans a row keeps; spans closer than GAP clean bytes merge. */
#define NSPAN		3
#define GAP		2

int lcv_wr(int reg, int val);
int lcv_seek(UWORD addr);
int lcv_push(const UBYTE *src, UWORD n);
int lcv_ready(void);

struct LCBase {
	struct Library	lb_Lib;
	APTR		lb_Seg;
};

APTR FrameBufBase;

static char lcname[] = LCNAME;
static char lcid[] = "lc.gfxdriver 45.1 (15.11.2026)";
static char lcowner[] = "graphics.library";

static struct GfxMode lc_mode = {
	LCMONO_200_ID, LC_W, LC_H, 1, GDF_PLANAR
};
static struct GfxBitMap lc_bm;
static struct GfxDriver *lc_fmt;	/* graphics' planar operations */
static UBYTE lc_pixtab[256];
static BOOL lc_alloc, lc_shown;
static struct SignalSemaphore lc_lock;	/* the chip's port sequences */

static BOOL lc_ok;		/* the chip answers */
static UWORD lc_ua, lc_ba;	/* update and copy source address, or NOADDR */
static UBYTE lc_r24;		/* R24 as written: bit 7 copy, else fill */
static UWORD lc_spent;		/* microseconds of the piece so far */
static UBYTE lc_fg = 0xF, lc_bg = 0x2;	/* RGBI of pixels 1 and 0 */

static UBYTE lc_ns[LC_H];		/* each row's dirty spans */
static UBYTE lc_lo[LC_H][NSPAN], lc_hi[LC_H][NSPAN];	/* bytes, inclusive */
static WORD lc_y0 = LC_H, lc_y1 = -1;	/* rows that may have spans */

#define LCBM	(&lc_bm.gb_BitMap)

/*
 * The registers for bitmap mode, R0-R36: 80x25 cells of 8 lines, as the
 * text console's timing; bitmap at 0x0000, attributes off (R25), 64 KB
 * RAM (R28), cursor off (R10), blank (R26), refresh 5 a line (R36).
 */
static const UBYTE lc_regs[37] = {
	0x7E, 0x50, 0x66, 0x49, 0x20, 0x00, 0x19, 0x1D,
	0x00, 0x07, 0x20, 0x07, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x78, 0x08,
	0x00, 0x87, 0x00, 0x00, 0x10, 0x07, 0x00, 0x00,
	0x00, 0x00, 0x7D, 0x64, 0x05
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
	lc_spent = 0;
}

static void lc_end(void)
{
	if (lc_ok && !lcv_ready())
		lc_ok = FALSE;
	Enable();
}

static void lc_leave(void)
{
	lc_end();
	ReleaseSemaphore(&lc_lock);
}

/* How many of n actions of us microseconds fit in the piece, at least 1 */
static UWORD lc_room(UWORD n, UWORD us)
{
	UWORD k;

	if (lc_spent + us > US_PIECE) {
		lc_end();
		Disable();
		lc_spent = 0;
	}
	k = (US_PIECE - lc_spent) / us;
	if (k > n)
		k = n;
	lc_spent += k * us;
	return k;
}

static void lc_wr(int reg, int val)
{
	lc_room(1, US_REG);
	if (lc_ok && !lcv_wr(reg, val))
		lc_ok = FALSE;
}

static void lc_seek(UWORD a)
{
	if (a == lc_ua)
		return;
	lc_room(1, US_SEEK);
	if (lc_ok && !lcv_seek(a))
		lc_ok = FALSE;
	lc_ua = a;
}

static void lc_mode24(UBYTE copy)
{
	if ((lc_r24 & 0x80) != copy)
		lc_wr(24, lc_r24 = (lc_r24 & 0x7F) | copy);
}

static void lc_push(UWORD a, const UBYTE *src, UWORD n)
{
	UWORD k;

	lc_seek(a);
	for (; n > 0; n -= k, src += k) {
		k = lc_room(n, US_PUSH);
		if (lc_ok && !lcv_push(src, k))
			lc_ok = FALSE;
		lc_ua += k;
	}
}

/* R30 blocks of n bytes in all, of the mode R24 has */
static void lc_blocks(UWORD n, UWORD us)
{
	UWORD k;

	for (; n > 0; n -= k) {
		k = lc_room(n > BLOCK ? BLOCK : n, us);
		lc_wr(30, k);
		lc_ua += k;
		if (us == US_COPY)
			lc_ba += k;
	}
}

/* n bytes of v from a: R31 writes the first, a block fill the rest */
static void lc_fill(UWORD a, UBYTE v, UWORD n)
{
	lc_seek(a);
	lc_mode24(0);
	lc_wr(31, v);
	lc_ua++;
	lc_blocks(n - 1, US_FILL);
}

/* n bytes from s to d; the chip copies upwards */
static void lc_copy(UWORD d, UWORD s, UWORD n)
{
	lc_seek(d);
	if (s != lc_ba) {
		lc_wr(32, s >> 8);
		lc_wr(33, s & 0xFF);
		lc_ba = s;
	}
	lc_mode24(0x80);
	lc_blocks(n, US_COPY);
}

/* The bitmap mode, and the bitmap cleared */
static void lc_init(void)
{
	UWORD i;

	lc_ok = TRUE;
	lc_ua = lc_ba = NOADDR;
	lc_enter();
	lc_wr(28, lc_regs[28]);
	for (i = 0; i < 37; i++)
		if (i != 16 && i != 17 && i != 18 && i != 19 && i != 28 &&
		    i != 30 && i != 31)
			lc_wr(i, lc_regs[i]);
	lc_r24 = lc_regs[24];
	lc_fill(0, 0, LC_SIZE);
	lc_leave();
}

/* Dirty spans */

static void lc_span(WORD y, UWORD b0, UWORD b1)
{
	UBYTE *lo = lc_lo[y], *hi = lc_hi[y];
	UWORD n = lc_ns[y], i, best, d, bd;

	for (;;) {
		for (i = 0; i < n; )
			if (lo[i] <= b1 + GAP + 1 && b0 <= hi[i] + GAP + 1) {
				if (lo[i] < b0)
					b0 = lo[i];
				if (hi[i] > b1)
					b1 = hi[i];
				n--;
				lo[i] = lo[n];
				hi[i] = hi[n];
			} else
				i++;
		if (n < NSPAN)
			break;
		/* full: take in the nearest span, then merge again */
		bd = 0xFFFF;
		best = 0;
		for (i = 0; i < n; i++) {
			d = lo[i] > b1 ? lo[i] - b1 : b0 - hi[i];
			if (d < bd) {
				bd = d;
				best = i;
			}
		}
		if (lo[best] < b0)
			b0 = lo[best];
		if (hi[best] > b1)
			b1 = hi[best];
		n--;
		lo[best] = lo[n];
		hi[best] = hi[n];
	}
	lo[n] = b0;
	hi[n] = b1;
	lc_ns[y] = n + 1;
	if (y < lc_y0)
		lc_y0 = y;
	if (y > lc_y1)
		lc_y1 = y;
}

/* Pixels x0-x1 of rows y0-y1 changed */
static void lc_mark(WORD y0, WORD y1, WORD x0, WORD x1)
{
	for (; y0 <= y1; y0++)
		lc_span(y0, x0 >> 3, x1 >> 3);
}

/* Bytes b0-b1 of row y are on the display; a span split in two may stay */
static void lc_clean(WORD y, UWORD b0, UWORD b1)
{
	UBYTE *lo = lc_lo[y], *hi = lc_hi[y];
	UWORD n = lc_ns[y], i;

	for (i = 0; i < n; ) {
		if (lo[i] > b1 || hi[i] < b0) {
			i++;
		} else if (lo[i] >= b0 && hi[i] <= b1) {
			n--;
			lo[i] = lo[n];
			hi[i] = hi[n];
		} else if (lo[i] >= b0) {
			lo[i++] = b1 + 1;
		} else if (hi[i] <= b1) {
			hi[i++] = b0 - 1;
		} else if (n < NSPAN) {
			lo[n] = b1 + 1;
			hi[n++] = hi[i];
			hi[i++] = b0 - 1;
		} else
			i++;
	}
	lc_ns[y] = n;
}

static void lc_Flush(struct GfxDriver *gd, struct BitMap *bm)
{
	UBYTE *s = LCBM->Planes[0];
	WORD y, y1 = lc_y1;
	UWORD i, a;

	if (y1 < 0)
		return;
	y = lc_y0;
	lc_y0 = LC_H;
	lc_y1 = -1;
	lc_enter();
	for (; y <= y1; y++) {
		for (i = 0; i < lc_ns[y]; i++) {
			a = y * LC_BPR + lc_lo[y][i];
			lc_push(a, s + a, lc_hi[y][i] - lc_lo[y][i] + 1);
		}
		lc_ns[y] = 0;
	}
	lc_leave();
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

/* Byte b of row y to the display from the shadow */
static void lc_pushb(WORD y, UWORD b)
{
	UWORD a = y * LC_BPR + b;

	lc_push(a, LCBM->Planes[0] + a, 1);
}

/*
 * Rows of one byte value fill as blocks, consecutive full rows as one;
 * a partial edge byte goes out beside its row's block, without a seek.
 */
static void lc_Fill(struct BitMap *bm, const struct Rectangle *r,
    const struct GfxPens *p)
{
	UWORD fb0 = (r->MinX + 7) >> 3, fb1 = ((r->MaxX + 1) >> 3) - 1;
	UWORD a, n, ra = 0, rn = 0, left, right;
	UBYTE rv = 0;
	WORD y, v;

	lc_fmt->gd_Fill(bm, r, p);
	if (!(p->gp_Mask & 1))
		return;
	if ((WORD)fb1 < (WORD)fb0 + 3 || (p->gp_DrMd & COMPLEMENT)) {
		lc_mark(r->MinY, r->MaxY, r->MinX, r->MaxX);
		return;
	}
	n = fb1 - fb0 + 1;
	left = (r->MinX & 7) != 0;
	right = (r->MaxX & 7) != 7;
	lc_enter();
	for (y = r->MinY; y <= r->MaxY; y++) {
		v = lc_fillbyte(p, y);
		if (v == -1)
			lc_span(y, r->MinX >> 3, r->MaxX >> 3);
		if (v < 0)
			continue;
		a = y * LC_BPR + fb0;
		lc_clean(y, fb0 - left, fb1 + right);
		if (left || right) {
			if (left)
				lc_pushb(y, fb0 - 1);
			lc_fill(a, v, n);
			if (right)
				lc_pushb(y, fb1 + 1);
		} else if (rn > 0 && ra + rn == a && rv == v) {
			rn += n;
		} else {
			if (rn > 0)
				lc_fill(ra, rv, rn);
			ra = a;
			rn = n;
			rv = v;
		}
	}
	if (rn > 0)
		lc_fill(ra, rv, rn);
	lc_leave();
}

static void lc_Template(struct BitMap *bm, const struct Rectangle *r,
    const UBYTE *src, UWORD srcx, UWORD srcmod, const struct GfxPens *p)
{
	lc_fmt->gd_Template(bm, r, src, srcx, srcmod, p);
	if (p->gp_Mask & 1)
		lc_mark(r->MinY, r->MaxY, r->MinX, r->MaxX);
}

/*
 * The aligned copy of r's whole bytes fb0-fb1 from (sx, sy), on the
 * display as already in the shadow, in the order overlaps need; partial
 * edge bytes go out beside each row's block
 */
static void lc_vcopy(const struct Rectangle *r, WORD sx, WORD sy,
    UWORD fb0, UWORD fb1)
{
	UWORD n = fb1 - fb0 + 1, h = r->MaxY - r->MinY + 1, k, d;
	UWORD left = (r->MinX & 7) != 0, right = (r->MaxX & 7) != 7;
	WORD dy = sy - r->MinY, y, y0, y1, ystep;
	WORD off = dy * LC_BPR + (sx >> 3) - (r->MinX >> 3);

	lc_enter();
	if (n == LC_BPR && dy > 0) {
		lc_copy(r->MinY * LC_BPR, r->MinY * LC_BPR + off, h * LC_BPR);
	} else if (n == LC_BPR) {
		/* bottom up, in blocks of rows that do not overlap their source */
		for (y = r->MaxY + 1; h > 0; h -= k) {
			k = h < -dy ? h : -dy;
			y -= k;
			lc_copy(y * LC_BPR, y * LC_BPR + off, k * LC_BPR);
		}
	} else {
		y0 = dy < 0 ? r->MaxY : r->MinY;
		y1 = dy < 0 ? r->MinY - 1 : r->MaxY + 1;
		ystep = dy < 0 ? -1 : 1;
		for (y = y0; y != y1; y += ystep) {
			d = y * LC_BPR + fb0;
			/* a move right within the row goes through scratch */
			if (off < 0 && -off < n)
				lc_copy(V_SCRATCH, d + off, n);
			if (left)
				lc_pushb(y, fb0 - 1);
			if (off < 0 && -off < n)
				lc_copy(d, V_SCRATCH, n);
			else
				lc_copy(d, d + off, n);
			if (right)
				lc_pushb(y, fb1 + 1);
		}
	}
	lc_leave();
}

static void lc_Copy(struct BitMap *src, WORD sx, WORD sy, struct BitMap *dst,
    const struct Rectangle *r, UBYTE minterm, UBYTE mask)
{
	UWORD fb0 = (r->MinX + 7) >> 3, fb1 = ((r->MaxX + 1) >> 3) - 1;

	if (dst != LCBM || !(mask & 1)) {
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
	if (p->gp_Mask & 1)
		lc_span(y, x >> 3, x >> 3);
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
	if (!(p->gp_Mask & 1))
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
			lc_span(y0, xmin >> 3, xmax >> 3);
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
		lc_span(y0 + k, a >> 3, b >> 3);
	}
}

static void lc_WriteRow(struct BitMap *bm, WORD x, WORD y, UWORD n,
    const UBYTE *pix, UBYTE mask)
{
	lc_fmt->gd_WriteRow(bm, x, y, n, pix, mask);
	if ((mask & 1) && n > 0)
		lc_span(y, x >> 3, (x + n - 1) >> 3);
}

/* The display */

static void lc_colours(void)
{
	lc_enter();
	lc_wr(26, lc_shown ? (lc_fg << 4) | lc_bg : 0);
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

static struct BitMap *lc_AllocDisplay(struct GfxDriver *gd,
    const struct GfxMode *m)
{
	struct BitMap *bm = LCBM;
	UWORD i;

	if (lc_alloc || m != &lc_mode)
		return NULL;
	bm->Planes[0] = AllocMem(LC_SIZE, MEMF_ANY | MEMF_CLEAR);
	if (bm->Planes[0] == NULL)
		return NULL;
	if (!lc_shown && AllocFB(lcowner) != NULL) {
		FreeMem(bm->Planes[0], LC_SIZE);
		return NULL;
	}
	lc_alloc = TRUE;
	lc_init();
	bm->BytesPerRow = LC_BPR;
	bm->Rows = LC_H;
	bm->Depth = 1;
	bm->pad = GFXBM_MAGIC;
	bm->Flags = BMF_DISPLAYABLE | BMF_STANDARD;
	lc_bm.gb_Driver = gd;
	lc_bm.gb_Flags = GBF_DISPLAY;
	lc_bm.gb_PlaneSize = LC_SIZE;
	lc_bm.gb_ModeID = m->gm_ModeID;
	for (i = 0; i < 256; i++)
		lc_pixtab[i] = i & 1;
	lc_bm.gb_PixTab = lc_pixtab;
	lc_y0 = LC_H;
	lc_y1 = -1;
	for (i = 0; i < LC_H; i++)
		lc_ns[i] = 0;
	lc_colours();
	return bm;
}

static void lc_FreeDisplay(struct GfxDriver *gd, struct BitMap *bm)
{
	lc_alloc = FALSE;
	FreeMem(bm->Planes[0], LC_SIZE);
	bm->Planes[0] = NULL;
	if (!lc_shown)
		FreeFB();
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
	&lc_mode, 1,
	lc_SetColor,
	NULL,
	lc_AllocDisplay,
	lc_FreeDisplay,
	lc_Show,
	lc_Flush
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
	lib->lib_Revision = 1;
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
