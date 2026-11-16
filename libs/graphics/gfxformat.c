/* SPDX-License-Identifier: MIT */
/*
 * The row code of both RAM bitmap formats: fills, templates, copies and
 * lines on rows of 1 or 4 bits a pixel, a word at a time, the long runs in
 * assembler.  A row that crosses from the first 64 KB of a plane
 * into the second is split there.
 */
#include <exec/types.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include "gfxformat.h"

/* Words a pass of a template or a shifted copy works on */
#define CHUNK	32

/* Byte o of a plane */
#define PL_AT(pl, o) \
	((o) < 0x10000L ? (pl)->pl_Lo + (UWORD)(o) : (pl)->pl_Hi + (UWORD)(o))

/* Words from offset o before the second half, or n if the row is in one */
#define PL_SPLIT(o, n) \
	((o) < 0x10000L && (o) + 2 * (ULONG)(n) > 0x10000L ? \
	    (UWORD)((0x10000L - (o)) >> 1) : (n))

/* 4 template bits, bit 3 leftmost, as a word of chunky pixel masks */
static const UWORD nib4[16] = {
    0x0000, 0x000F, 0x00F0, 0x00FF, 0x0F00, 0x0F0F, 0x0FF0, 0x0FFF,
    0xF000, 0xF00F, 0xF0F0, 0xF0FF, 0xFF00, 0xFF0F, 0xFFF0, 0xFFFF
};

/* 8 template bits, bit 7 leftmost, as two words of chunky pixel masks */
static ULONG nib8[256];

void gfx_InitFormats(void)
{
    UWORD k;

    for (k = 0; k < 256; k++)
        nib8[k] = ((ULONG)nib4[k >> 4] << 16) | nib4[k & 15];
}

/* Plane n of those of bm that p draws into, in *pl, and the bits a pixel; FALSE past the last */
static BOOL gfx_Plane(struct BitMap *bm, const struct GfxPens *p, WORD n,
    struct GfxPlane *pl, UWORD *bpp)
{
    register PLANEPTR *pp = bm->Planes;
    register UWORD i;
    UBYTE *hi = NULL;
    UBYTE m = p->gp_Mask, a = p->gp_APen, b = p->gp_BPen;
    UWORD depth = bm->Depth;
    BOOL chunky = FALSE;

    if (IS_GFXBM(bm)) {
        hi = GFXBM(bm)->gb_Half1;
        chunky = GFXBM(bm)->gb_Driver->gd_Format == GDF_CHUNKY4;
    }
    if (chunky) {
        *bpp = 4;
        if (n > 0 || (m & 15) == 0)
            return FALSE;
        pl->pl_Lo = *pp;
        pl->pl_Hi = hi;
        pl->pl_F = (a & 15) * 0x1111;
        pl->pl_B = (b & 15) * 0x1111;
        pl->pl_M = (m & 15) * 0x1111;
        return TRUE;
    }
    *bpp = 1;
    if (depth > 8)
        depth = 8;
    for (i = 0; i < depth; i++, pp++, a >>= 1, b >>= 1, m >>= 1) {
        if (!(m & 1) || *pp == NULL || *pp == (PLANEPTR)-1 || n-- > 0)
            continue;
        pl->pl_Lo = *pp;
        pl->pl_Hi = i == 0 ? hi : NULL;
        pl->pl_F = (a & 1) ? 0xFFFF : 0;
        pl->pl_B = (b & 1) ? 0xFFFF : 0;
        pl->pl_M = 0xFFFF;
        return TRUE;
    }
    return FALSE;
}

WORD gfx_Planes(struct BitMap *bm, const struct GfxPens *p,
    struct GfxPlane *pl, UWORD *bpp)
{
    WORD np = 0;

    while (gfx_Plane(bm, p, np, &pl[np], bpp))
        np++;
    return np;
}

/*
 * Rows from offset o, step bytes apart, that stay in one half of the
 * plane; 0 when the first row's [o, o + len) crosses.
 */
static UWORD runrows(ULONG o, UWORD len, LONG step, UWORD rows)
{
    ULONG k, last;

    if (o < 0x10000L && o + len > 0x10000L)
        return 0;
    /* the rows from o to the last are on one side of 0x10000 */
    if (step < 0)
        last = o - (ULONG)(rows - 1) * (UWORD)-step;
    else
        last = o + (ULONG)(rows - 1) * (UWORD)step;
    if (o >= 0x10000L ? last >= 0x10000L : last + len <= 0x10000L)
        return rows;
    if (step > 0)
        k = (0x10000L - len - o) / (ULONG)step + 1;
    else
        k = (o - 0x10000L) / (ULONG)-step + 1;
    return k < rows ? (UWORD)k : rows;
}

/*
 * The FillRows entries of the words from word w under cm: for each
 * pattern row, a[0-3] then x[0-3].  FALSE when they change nothing.
 */
static BOOL filltab(UWORD *tab, const struct GfxPens *p,
    const struct GfxPlane *pl, UWORD bpp, UWORD w, UWORD cm, UWORD ne,
    UWORD row)
{
    register UWORD *a, t, m = pl->pl_M & cm, j;
    UWORD inv = (p->gp_DrMd & INVERSVID) ? 0xFFFF : 0;
    UBYTE mode = p->gp_DrMd & (JAM2 | COMPLEMENT);
    UWORD f = pl->pl_F, b = pl->pl_B, e, pw, live = 0;

    for (e = 0; e < ne; e++, tab += 8) {
        pw = p->gp_Ptrn[(row + e) & p->gp_PtrnMask];
        for (j = 0, a = tab; j < 4; j++, a++) {
            /* a planar word is the same pattern word 4 times */
            if (bpp == 1 && j > 0) {
                a[0] = a[-1];
                a[4] = a[3];
                continue;
            }
            t = bpp == 1 ? pw :
                nib4[(pw >> (12 - (((w + j) & 3) << 2))) & 15];
            t ^= inv;
            if (mode & COMPLEMENT) {
                a[0] = 0xFFFF;
                a[4] = t & m;
            } else if (mode & JAM2) {
                a[0] = ~m;
                a[4] = ((t & f) | (~t & b)) & m;
            } else {
                a[0] = ~(t & m);
                a[4] = f & t & m;
            }
            live |= ~a[0] | a[4];
        }
    }
    return live != 0;
}

/*
 * Words [c, c + cnt) of the rows from row offset o under the pattern, in
 * either half; a pattern of over 8 rows a row at a time.
 */
static void fillcols(const struct GfxPens *p, const struct GfxPlane *pl,
    UWORD bpp, UWORD bpr, ULONG o, UWORD w0, UWORD c, UWORD cnt, UWORD cm,
    WORD y, UWORD rows)
{
    UWORD tab[8 * 8], one[8];
    UWORD pm, tpm, g, k, n1;
    BOOL tall;

    pm = p->gp_PtrnMask;
    tall = pm > 7;
    tpm = tall ? 0 : pm;
    if (!tall && !filltab(tab, p, pl, bpp, w0 + c, cm, pm + 1, 0))
        return;
    o += c << 1;
    while (rows > 0) {
        g = rows;
        if (tall) {
            g = 1;
            if (!filltab(tab, p, pl, bpp, w0 + c, cm, 1, y)) {
                o += bpr;
                y++;
                rows--;
                continue;
            }
        }
        k = runrows(o, cnt << 1, bpr, g);
        if (k > 0) {
            gfx_FillRows((UWORD *)PL_AT(pl, o), cnt, k, bpr, tab, tpm,
                tall ? 0 : y);
            o += (ULONG)k * bpr;
            y += k;
            rows -= k;
            continue;
        }
        /* a row that crosses into the second half */
        n1 = (UWORD)((0x10000L - o) >> 1);
        gfx_FillRows((UWORD *)PL_AT(pl, o), n1, 1, bpr, tab, tpm,
            tall ? 0 : y);
        filltab(one, p, pl, bpp, w0 + c + n1, cm, 1, y);
        gfx_FillRows((UWORD *)pl->pl_Hi, cnt - n1, 1, bpr, one, 0, 0);
        o += bpr;
        y++;
        rows--;
    }
}

/* gfx_FillRect with lm and rm as one mask for a single word */
static void fillrect(UWORD *d, UWORD n, UWORD rows, UWORD bpr, UWORD a,
    UWORD x, UWORD lm, UWORD rm)
{
    if (n == 1)
        lm &= rm;
    gfx_FillRect(d, n, rows, bpr, a, x, lm, rm);
}

/* rows rows of n words from offset o, d = (d & a) ^ x, in either half */
static void fillspan(const struct GfxPlane *pl, ULONG o, UWORD n, UWORD rows,
    UWORD bpr, UWORD a, UWORD x, UWORD lm, UWORD rm)
{
    UWORD k, n1;

    while (rows > 0) {
        k = runrows(o, n << 1, bpr, rows);
        if (k > 0) {
            fillrect((UWORD *)PL_AT(pl, o), n, k, bpr, a, x, lm, rm);
            o += (ULONG)k * bpr;
            rows -= k;
            continue;
        }
        /* a row that crosses into the second half */
        n1 = (UWORD)((0x10000L - o) >> 1);
        fillrect((UWORD *)PL_AT(pl, o), n1, 1, bpr, a, x, lm, 0xFFFF);
        fillrect((UWORD *)pl->pl_Hi, n - n1, 1, bpr, a, x, 0xFFFF, rm);
        o += bpr;
        rows--;
    }
}

void gfx_Fill(struct BitMap *bm, const struct Rectangle *r,
    const struct GfxPens *p)
{
    struct GfxPlane pl[8];
    register struct GfxPlane *q;
    UWORD bpp, bpr = bm->BytesPerRow, w0, n, lm, rm, k, e, rows, t, m, a, x;
    UBYTE dm = p->gp_DrMd;
    WORD np, i, bx0, bx1;
    ULONG o;

    np = gfx_Planes(bm, p, pl, &bpp);
    bx0 = bpp == 1 ? r->MinX : r->MinX << 2;
    bx1 = bpp == 1 ? r->MaxX : (r->MaxX << 2) + 3;
    w0 = bx0 >> 4;
    n = (bx1 >> 4) - w0 + 1;
    lm = 0xFFFF >> (bx0 & 15);
    rm = 0xFFFF << (15 - (bx1 & 15));
    rows = r->MaxY - r->MinY + 1;
    o = (ULONG)r->MinY * bpr + (w0 << 1);
    if (p->gp_Ptrn == NULL) {
        t = (dm & INVERSVID) ? 0 : 0xFFFF;
        for (q = pl; np > 0; np--, q++) {
            m = q->pl_M;
            if (dm & COMPLEMENT) {
                a = 0xFFFF;
                x = t & m;
            } else if (dm & JAM2) {
                a = ~m;
                x = ((t & q->pl_F) | (~t & q->pl_B)) & m;
            } else {
                a = ~(t & m);
                x = q->pl_F & t & m;
            }
            if (a == 0xFFFF && x == 0)
                continue;
            if (q->pl_Hi == NULL ||
                    o + (ULONG)(rows - 1) * bpr + (n << 1) <= 0x10000L)
                fillrect((UWORD *)(q->pl_Lo + (UWORD)o), n, rows, bpr,
                    a, x, lm, rm);
            else
                fillspan(q, o, n, rows, bpr, a, x, lm, rm);
        }
        return;
    }
    if (n == 1) {
        lm &= rm;
        rm = 0xFFFF;
    }
    k = lm != 0xFFFF;
    e = n - (rm != 0xFFFF);
    for (i = 0; i < np; i++) {
        if (k)
            fillcols(p, &pl[i], bpp, bpr, o, w0, 0, 1, lm, r->MinY, rows);
        if (e > k)
            fillcols(p, &pl[i], bpp, bpr, o, w0, k, e - k, 0xFFFF, r->MinY,
                rows);
        if (e < n)
            fillcols(p, &pl[i], bpp, bpr, o, w0, e, 1, rm, r->MinY, rows);
    }
}

/* 16 bits of s from bit b (-15 to 7 + 8q), no byte past s[qend] */
static UWORD bits16(const UBYTE *s, WORD b, UWORD qend)
{
    UWORD q, sh, v;

    if (b < 0)
        return (((UWORD)s[0] << 8) | (qend > 0 ? s[1] : 0)) >> -b;
    q = (UWORD)b >> 3;
    sh = b & 7;
    v = (UWORD)s[q] << 8;
    if (q + 1 <= qend)
        v |= s[q + 1];
    v <<= sh;
    if (sh != 0 && q + 2 <= qend)
        v |= s[q + 2] >> (8 - sh);
    return v;
}

/* n words of the bits of s from bit b (b >= -15), none past s[qend] */
static void getbits(UWORD *out, const UBYTE *s, WORD b, UWORD n, UWORD qend)
{
    UWORD k = 0, fast;

    if (b < 0) {
        *out++ = bits16(s, b, qend);
        b += 16;
        n--;
    }
    if (n == 0)
        return;
    s += (UWORD)b >> 3;
    qend -= (UWORD)b >> 3;
    b &= 7;
    /* the words whose bytes are all there go in one run */
    fast = b == 0 ? (qend + 1) >> 1 : qend >> 1;
    if (fast > n)
        fast = n;
    if (fast > 0)
        gfx_Bits(out, s, b, fast);
    for (k = fast; k < n; k++)
        out[k] = bits16(s, b + (WORD)(k << 4), qend);
}

/* gfx_TmplRows' and gfx_TmplRows1's job; its modes */
struct tmpljob {
    UWORD	*tj_D;
    WORD	 tj_DStep;
    UWORD	 tj_Rows;
    const UBYTE	*tj_Src;
    UWORD	 tj_SrcMod;
    WORD	 tj_TB;
    UWORD	 tj_N, tj_F, tj_B, tj_M, tj_LM, tj_RM, tj_Mode;
    const ULONG	*tj_Nib;
};

#define T_JAM2		0
#define T_JAM1		1
#define T_COMP		4
#define T_NOT		2

/* Destination words a pass of gfx_TmplRows takes */
#define TPASS	32

/*
 * rows rows of cnt words of the destination from d, template bit tb of
 * src for its first pixel.  TmplRows reads a byte past each row's bits,
 * so the last row of the template, qlen bytes, goes from a copy.
 */
static void tmplrows(struct tmpljob *tj, UWORD *d, UWORD rows,
    const UBYTE *src, WORD tb, UWORD cnt, UWORD lm, UWORD rm, BOOL last,
    UWORD qlen)
{
    UBYTE lb[2 * TPASS + 6];
    UWORD nw, nb, k;
    const UBYTE *s;

    tj->tj_N = cnt;
    tj->tj_LM = lm;
    tj->tj_RM = rm;
    if (rows > last) {
        tj->tj_D = d;
        tj->tj_Rows = rows - last;
        tj->tj_Src = src;
        tj->tj_TB = tb;
        if (tj->tj_Nib)
            gfx_TmplRows(tj);
        else
            gfx_TmplRows1(tj);
        d = (UWORD *)((UBYTE *)d + (ULONG)tj->tj_DStep * (rows - 1));
        src += tj->tj_SrcMod * (rows - 1);
    }
    if (last) {
        /* the bytes it reads, and 0s after the template's */
        nw = tj->tj_Nib ? (cnt + 3) >> 2 : cnt;
        k = tb < 0 ? 0 : (UWORD)tb >> 3;
        s = src + k;
        tb -= k << 3;
        nb = qlen > k ? qlen - k : 0;
        for (k = 0; k < 2 * nw + 3; k++)
            lb[k] = k < nb ? s[k] : 0;
        tj->tj_D = d;
        tj->tj_Rows = 1;
        tj->tj_Src = lb;
        tj->tj_TB = tb;
        if (tj->tj_Nib)
            gfx_TmplRows(tj);
        else
            gfx_TmplRows1(tj);
    }
}

void gfx_Template(struct BitMap *bm, const struct Rectangle *r,
    const UBYTE *src, UWORD srcx, UWORD srcmod, const struct GfxPens *p)
{
    struct GfxPlane pl;
    struct tmpljob tj;
    UWORD bpp, ppw, bpr = bm->BytesPerRow;
    UWORD qlen = ((srcx + (r->MaxX - r->MinX)) >> 3) + 1;
    UWORD w0, n, n1, lm, rm, c, cnt, rows, k, mode;
    UBYTE dm = p->gp_DrMd;
    WORD i, y, bx0, bx1, tx, tb;
    const UBYTE *s;
    ULONG o, oc;

    /* One plane at a time, as the blits below need the stack */
    if (!gfx_Plane(bm, p, 0, &pl, &bpp))
        return;
    ppw = 16 / bpp;
    bx0 = r->MinX * bpp;
    bx1 = r->MaxX * bpp + bpp - 1;
    w0 = bx0 >> 4;
    n = (bx1 >> 4) - w0 + 1;
    lm = 0xFFFF >> (bx0 & 15);
    rm = 0xFFFF << (15 - (bx1 & 15));
    if (dm & COMPLEMENT)
        mode = T_COMP;
    else if (dm & JAM2)
        mode = T_JAM2;
    else
        mode = T_JAM1;
    if ((dm & INVERSVID) && mode != T_JAM2)
        mode |= T_NOT;
    tj.tj_DStep = bpr;
    tj.tj_SrcMod = srcmod;
    /* template bit of the first pixel of word w0 */
    tx = srcx - (r->MinX & (ppw - 1));
    for (i = 0; i == 0 || gfx_Plane(bm, p, i, &pl, &bpp); i++) {
        tj.tj_F = pl.pl_F;
        tj.tj_B = pl.pl_B;
        if (mode == T_JAM2) {
            /* ((f ^ b) & t) ^ b, or with t inverted ^ f */
            tj.tj_F ^= pl.pl_B;
            if (dm & INVERSVID)
                tj.tj_B = pl.pl_F;
        }
        tj.tj_M = pl.pl_M;
        tj.tj_Mode = mode;
        tj.tj_Nib = bpp == 1 ? NULL : nib8;
        for (c = 0; c < n; c += cnt) {
            cnt = n - c < TPASS ? n - c : TPASS;
            tb = tx + (WORD)(c * ppw);
            o = (ULONG)r->MinY * bpr + ((w0 + c) << 1);
            s = src;
            y = r->MinY;
            for (rows = r->MaxY - r->MinY + 1; rows > 0; rows -= k) {
                k = runrows(o, cnt << 1, bpr, rows);
                if (k > 0) {
                    tmplrows(&tj, (UWORD *)PL_AT(&pl, o), k, s, tb, cnt,
                        c == 0 ? lm : 0xFFFF, c + cnt == n ? rm : 0xFFFF,
                        y + k - 1 == r->MaxY, qlen);
                } else {
                    /* a row that crosses into the second half */
                    k = 1;
                    n1 = (UWORD)((0x10000L - o) >> 1);
                    tmplrows(&tj, (UWORD *)PL_AT(&pl, o), 1, s, tb, n1,
                        c == 0 ? lm : 0xFFFF, 0xFFFF, y == r->MaxY, qlen);
                    tmplrows(&tj, (UWORD *)pl.pl_Hi, 1, s,
                        tb + (WORD)(n1 * ppw), cnt - n1, 0xFFFF,
                        c + cnt == n ? rm : 0xFFFF, y == r->MaxY, qlen);
                }
                o += (ULONG)k * bpr;
                s += srcmod * k;
                y += k;
            }
        }
    }
}

/*
 * cnt words of a row of the destination plane d from the source bits
 * sb: the minterm under m, the first word also under lm and the last
 * under rm.
 */
static void copyrun(UWORD *d, const UWORD *sb, UWORD cnt, UWORD lm, UWORD rm,
    UWORD m, UBYTE mt)
{
    register UWORD b, c, r, mm;
    register UWORD k;
    UWORD tbc, tbn, tnc, tnn;

    if (cnt == 1) {
        lm &= rm;
        rm = 0xFFFF;
    }
    if (mt == 0xC0 && m == 0xFFFF) {
        if (lm != 0xFFFF) {
            *d = (*d & ~lm) | (*sb++ & lm);
            d++;
            cnt--;
        }
        if (rm != 0xFFFF) {
            cnt--;
            d[cnt] = (d[cnt] & ~rm) | (sb[cnt] & rm);
        }
        if (cnt > 0)
            gfx_WCopy(d, sb, cnt);
        return;
    }
    /* the minterm's four terms as masks */
    tbc = (mt & 0x80) ? 0xFFFF : 0;
    tbn = (mt & 0x40) ? 0xFFFF : 0;
    tnc = (mt & 0x20) ? 0xFFFF : 0;
    tnn = (mt & 0x10) ? 0xFFFF : 0;
    for (k = cnt; k > 0; k--, d++, sb++) {
        mm = m;
        if (k == cnt)
            mm &= lm;
        if (k == 1)
            mm &= rm;
        b = *sb;
        c = *d;
        r = (b & c & tbc) | (b & ~c & tbn) | (~b & c & tnc) | (~b & ~c & tnn);
        *d = c ^ ((c ^ r) & mm);
    }
}

/* What a copy of one plane needs, for copyrow */
struct copyjob {
    const struct GfxPlane *cj_Dst;
    UBYTE	*cj_Lo, *cj_Hi;	/* the source plane, or NULL: cj_Const */
    UWORD	 cj_Const;
    UWORD	 cj_N, cj_LM, cj_RM, cj_SQ;
    WORD	 cj_TB;
    UBYTE	 cj_MT;
    BOOL	 cj_Left;
};

/*
 * One row of a plane in passes of up to CHUNK words, each in one half
 * of the destination; the source bits through a buffer, so the row may
 * cross the halves of either plane and overlap itself.
 */
static void copyrow(const struct copyjob *cj, ULONG so, ULONG dof)
{
    UWORD tb[CHUNK];
    UBYTE lb[2 * CHUNK + 4];
    UWORD n = cj->cj_N, dn1, c, cnt, k, q, nb;
    WORD ct, st;
    ULONG sqo;

    dn1 = PL_SPLIT(dof, n);
    for (c = 0; c < n; c += cnt) {
        cnt = n - c;
        if (cnt > CHUNK)
            cnt = CHUNK;
        if (cj->cj_Left) {
            ct = n - c - cnt;
            if (ct < dn1 && ct + cnt > dn1) {
                cnt = ct + cnt - dn1;
                ct = dn1;
            }
        } else {
            ct = c;
            if (ct < dn1 && ct + cnt > dn1)
                cnt = dn1 - ct;
        }
        st = cj->cj_TB + (ct << 4);
        if (cj->cj_Lo == NULL) {
            for (k = 0; k < cnt; k++)
                tb[k] = cj->cj_Const;
        } else {
            /* the source bytes q to q + nb - 1 of this pass */
            q = st < 0 ? 0 : (UWORD)st >> 3;
            nb = ((UWORD)(st + (WORD)(cnt << 4) - 1) >> 3) - q + 1;
            if (q + nb > cj->cj_SQ + 1)
                nb = cj->cj_SQ + 1 - q;
            sqo = so + q;
            if (sqo < 0x10000L && sqo + nb > 0x10000L) {
                for (k = 0; k < nb; k++, sqo++)
                    lb[k] = *(sqo < 0x10000L ? cj->cj_Lo + (UWORD)sqo :
                        cj->cj_Hi + (UWORD)sqo);
                getbits(tb, lb, st - (q << 3), cnt, nb - 1);
            } else
                getbits(tb, sqo < 0x10000L ? cj->cj_Lo + (UWORD)sqo :
                    cj->cj_Hi + (UWORD)sqo, st - (q << 3), cnt, nb - 1);
        }
        copyrun((UWORD *)PL_AT(cj->cj_Dst, dof + (ct << 1)), tb, cnt,
            ct == 0 ? cj->cj_LM : 0xFFFF, ct + cnt == n ? cj->cj_RM : 0xFFFF,
            cj->cj_Dst->pl_M, cj->cj_MT);
    }
}

void gfx_Copy(struct BitMap *src, WORD sx, WORD sy, struct BitMap *dst,
    const struct Rectangle *r, UBYTE minterm, UBYTE mask)
{
    struct GfxPlane dp[8];
    struct GfxPens pens;
    struct copyjob cj;
    UBYTE *shi;
    UWORD bpp, sbpr = src->BytesPerRow, dbpr = dst->BytesPerRow;
    UWORD w0, sw, sn, rows, ks, kd, k, i, pi;
    WORD np, h, bx0, bx1, sbx;
    LONG sstep, dstep;
    ULONG so, dof;
    BOOL up, aligned;
    PLANEPTR pp;
    UWORD *d, *s;

    pens.gp_APen = pens.gp_BPen = 0;
    pens.gp_Mask = mask;
    np = gfx_Planes(dst, &pens, dp, &bpp);
    if (np == 0)
        return;
    shi = IS_GFXBM(src) ? GFXBM(src)->gb_Half1 : NULL;
    h = r->MaxY - r->MinY + 1;
    bx0 = r->MinX * bpp;
    bx1 = r->MaxX * bpp + bpp - 1;
    w0 = bx0 >> 4;
    cj.cj_N = (bx1 >> 4) - w0 + 1;
    cj.cj_LM = 0xFFFF >> (bx0 & 15);
    cj.cj_RM = 0xFFFF << (15 - (bx1 & 15));
    if (cj.cj_N == 1) {
        cj.cj_LM &= cj.cj_RM;
        cj.cj_RM = 0xFFFF;
    }
    cj.cj_MT = minterm;
    /* source bit, from its word sw, of bit 15 of destination word w0 */
    sbx = sx * bpp - (bx0 & 15);
    sw = sbx < 0 ? 0 : (UWORD)sbx >> 4;
    cj.cj_TB = sbx - (WORD)(sw << 4);
    /* the last source byte a row needs, and its words */
    cj.cj_SQ = (UWORD)((sx * bpp + (r->MaxX - r->MinX + 1) * bpp - 1) >> 3) -
        (sw << 1);
    sn = (cj.cj_SQ >> 1) + 1;
    /* bottom up when the destination is below the source */
    up = src == dst && r->MinY > sy;
    sstep = up ? -(LONG)sbpr : sbpr;
    dstep = up ? -(LONG)dbpr : dbpr;
    /* right to left on the same row when the destination is right of it */
    cj.cj_Left = src == dst && r->MinY == sy && r->MinX > sx;
    /* whole rows of words, copied, and for 0x30 then inverted: the same
       bits of the source words, or shifted; right to left only from a
       source bit at or after the destination's */
    aligned = (minterm == 0xC0 || minterm == 0x30) &&
        (cj.cj_TB >= 0 || !cj.cj_Left);

    for (i = 0, pi = 0; i < np; i++) {
        /* the source plane of the destination's, or its constant */
        if (bpp == 4)
            pp = src->Planes[0];
        else {
            while (dst->Planes[pi] != dp[i].pl_Lo)
                pi++;
            pp = pi < src->Depth ? src->Planes[pi] : NULL;
            pi++;
        }
        cj.cj_Dst = &dp[i];
        cj.cj_Const = pp == (PLANEPTR)-1 ? 0xFFFF : 0;
        cj.cj_Lo = pp == (PLANEPTR)-1 ? NULL : pp;
        cj.cj_Hi = pp == src->Planes[0] ? shi : NULL;
        so = (up ? (ULONG)(sy + h - 1) * sbpr : (ULONG)sy * sbpr) + (sw << 1);
        dof = (up ? (ULONG)r->MaxY * dbpr : (ULONG)r->MinY * dbpr) + (w0 << 1);
        for (rows = h; rows > 0; rows -= k) {
            k = 1;
            if (aligned && cj.cj_Lo && dp[i].pl_M == 0xFFFF) {
                /* runs of rows of whole words, each in one half */
                ks = runrows(so, sn << 1, sstep, rows);
                kd = runrows(dof, cj.cj_N << 1, dstep, rows);
                k = ks < kd ? ks : kd;
                if (k > 0) {
                    d = (UWORD *)PL_AT(&dp[i], dof);
                    s = (UWORD *)(so < 0x10000L ? cj.cj_Lo + (UWORD)so :
                        cj.cj_Hi + (UWORD)so);
                    if (cj.cj_TB == 0)
                        gfx_CopyRows(d, s, cj.cj_N, k, (WORD)dstep,
                            (WORD)sstep, cj.cj_LM, cj.cj_RM,
                            cj.cj_Left ? -1 : 1);
                    else
                        gfx_ShiftRows(d, s, cj.cj_N, k, (WORD)dstep,
                            (WORD)sstep, cj.cj_TB, cj.cj_LM, cj.cj_RM,
                            cj.cj_Left ? -1 : 1);
                    if (minterm == 0x30)
                        fillrect(up ? (UWORD *)((UBYTE *)d -
                            (ULONG)(k - 1) * dbpr) : d, cj.cj_N, k, dbpr,
                            0xFFFF, 0xFFFF, cj.cj_LM, cj.cj_RM);
                    so = up ? so - (ULONG)k * sbpr : so + (ULONG)k * sbpr;
                    dof = up ? dof - (ULONG)k * dbpr : dof + (ULONG)k * dbpr;
                    continue;
                }
                k = 1;
            }
            copyrow(&cj, so, dof);
            so += sstep;
            dof += dstep;
        }
    }
}

/* gfx_LineRun's job, the words it loads into r0-r11, then steep */
struct linejob {
    UWORD	 lj_Mask;	/* the first pixel's bits */
    UWORD	 lj_K;		/* K << 8 */
    UBYTE	*lj_At;		/* its byte */
    WORD	 lj_YStep;
    UWORD	 lj_Pens;	/* P0 << 8 | P1 */
    UWORD	 lj_Reload;	/* skip0 << 8 | the bits of the next byte */
    WORD	 lj_Err, lj_IncrE, lj_IncrNE;
    UWORD	 lj_Ptrn;
    WORD	 lj_Shift;
    UWORD	 lj_Steep;
};

/*
 * A line of dM steps on its major axis and dm on its minor one, dM under
 * 32768: the minor steps after i steps, (dm i + dM / 2) / dM; the first
 * step with at least k; the last with at most k.  The blitter's line
 * mode steps diagonally on a tie.
 */
static WORD fstep(WORD dM, WORD dm, WORD i)
{
    return gfx_MulDiv(dm, i, dM >> 1, dM, NULL);
}

static WORD firstf(WORD dM, WORD dm, WORD k)
{
    if (k <= 0)
        return 0;
    if (dm == 0 || k > dm)
        return dM + 1;
    return gfx_MulDiv(dM, k, dm - 1 - (dM >> 1), dm, NULL);
}

static WORD lastf(WORD dM, WORD dm, WORD k)
{
    if (k < 0)
        return -1;
    if (dm == 0 || k >= dm)
        return dM;
    return gfx_MulDiv(dM, k + 1, -1 - (dM >> 1), dm, NULL);
}

/* What stays the same for the pieces of a line */
struct line {
    WORD	 x0, y0, sx, sy, dM, dm;
    BOOL	 steep;
    UWORD	 bpp, bpr, ptrn;
};

/* The last step whose row is not past row yl */
static WORD lastrow(const struct line *ln, WORD yl)
{
    WORD k = ln->sy * (yl - ln->y0);

    if (ln->steep)
        return k < 0 ? -1 : k;
    return lastf(ln->dM, ln->dm, k);
}

/* Steps s to e of the line in one half of plane pl */
static void linerun(const struct line *ln, const struct GfxPlane *pl,
    struct linejob *lj, WORD s, WORD e)
{
    UWORD r, rot = s & 15;
    ULONG o;
    WORD f = gfx_MulDiv(ln->dm, s, ln->dM >> 1, ln->dM, &r), x, y;
    UBYTE pm = (UBYTE)pl->pl_M;

    x = ln->x0 + ln->sx * (ln->steep ? f : s);
    y = ln->y0 + ln->sy * (ln->steep ? s : f);
    /* dm (s + 1) + dM / 2 - dM (f + 1), from the remainder of f: in
       dm - dM to dm, so a word */
    lj->lj_Err = r + ln->dm - ln->dM;
    o = (ULONG)y * ln->bpr + (ln->bpp == 1 ? x >> 3 : x >> 1);
    lj->lj_At = PL_AT(pl, o);
    lj->lj_Mask = (ln->bpp == 1 ? 0x80 >> (x & 7) :
        ((x & 1) ? 0x0F : 0xF0)) & pm;
    lj->lj_Ptrn = rot ? (ln->ptrn << rot) | (ln->ptrn >> (16 - rot)) :
        ln->ptrn;
    gfx_LineRun(lj, ln->sx, e - s + 1);
}

void gfx_Line(struct BitMap *bm, const struct Rectangle *clip,
    WORD x0, WORD y0, WORD x1, WORD y1, UWORD ptrn,
    const struct GfxPens *p)
{
    struct GfxPlane pl[8];
    struct linejob lj;
    struct line ln;
    WORD dx, dy, np, i, a, b, s, e, lo, hi, m0, sm, xs, yb, rb;
    UBYTE pm, skip0;
    UWORD bpp, udx, udy;

    np = gfx_Planes(bm, p, pl, &bpp);
    ln.bpp = bpp;
    ln.bpr = bm->BytesPerRow;
    ln.ptrn = ptrn;
    ln.x0 = x0;
    ln.y0 = y0;
    ln.sx = x1 > x0 ? 1 : -1;
    ln.sy = y1 > y0 ? 1 : -1;
    udx = x1 > x0 ? (UWORD)x1 - (UWORD)x0 : (UWORD)x0 - (UWORD)x1;
    udy = y1 > y0 ? (UWORD)y1 - (UWORD)y0 : (UWORD)y0 - (UWORD)y1;
    /* the step counts are words */
    if (udx > 32767 || udy > 32767)
        return;
    dx = udx;
    dy = udy;
    ln.steep = dx < dy;
    ln.dM = ln.steep ? dy : dx;
    ln.dm = ln.steep ? dx : dy;
    /* the steps with their pixel in the clip: along the major axis */
    if (ln.steep) {
        a = ln.sy > 0 ? clip->MinY - y0 : y0 - clip->MaxY;
        b = ln.sy > 0 ? clip->MaxY - y0 : y0 - clip->MinY;
        m0 = x0;
        sm = ln.sx;
        lo = sm > 0 ? clip->MinX - m0 : m0 - clip->MaxX;
        hi = sm > 0 ? clip->MaxX - m0 : m0 - clip->MinX;
    } else {
        a = ln.sx > 0 ? clip->MinX - x0 : x0 - clip->MaxX;
        b = ln.sx > 0 ? clip->MaxX - x0 : x0 - clip->MinX;
        m0 = y0;
        sm = ln.sy;
        lo = sm > 0 ? clip->MinY - m0 : m0 - clip->MaxY;
        hi = sm > 0 ? clip->MaxY - m0 : m0 - clip->MinY;
    }
    if (a < 0)
        a = 0;
    if (b > ln.dM)
        b = ln.dM;
    /* and the minor */
    s = firstf(ln.dM, ln.dm, lo);
    if (s > a)
        a = s;
    e = lastf(ln.dM, ln.dm, hi);
    if (e < b)
        b = e;
    if (a > b)
        return;

    lj.lj_IncrE = ln.dm;
    lj.lj_IncrNE = ln.dm - ln.dM;
    lj.lj_YStep = ln.sy > 0 ? ln.bpr : -ln.bpr;
    lj.lj_Shift = ln.sx > 0 ? -bpp : bpp;
    lj.lj_Steep = ln.steep;
    for (i = 0; i < np; i++) {
        pm = (UBYTE)pl[i].pl_M;
        if (p->gp_DrMd & COMPLEMENT) {
            lj.lj_K = 0;
            lj.lj_Pens = 0xFF;
            skip0 = 1;
        } else {
            lj.lj_K = 0xFF00;
            lj.lj_Pens = ((UBYTE)pl[i].pl_B << 8) | (UBYTE)pl[i].pl_F;
            skip0 = !(p->gp_DrMd & JAM2);
        }
        lj.lj_Reload = (skip0 << 8) | (pm & (ln.sx > 0 ?
            (bpp == 1 ? 0x80 : 0xF0) : (bpp == 1 ? 0x01 : 0x0F)));
        if (pl[i].pl_Hi == NULL) {
            linerun(&ln, &pl[i], &lj, a, b);
            continue;
        }
        /* pieces that stay in one half: the rows below yb are in the
           first 64 KB, row yb from byte rb (pixel xs) on */
        yb = 0xFFFF / ln.bpr;
        rb = 0xFFFF % ln.bpr;
        if (++rb == ln.bpr) {
            yb++;
            rb = 0;
        }
        xs = bpp == 1 ? rb << 3 : rb << 1;
        for (s = a; s <= b; s = e + 1) {
            dy = y0 + ln.sy * (ln.steep ? s : fstep(ln.dM, ln.dm, s));
            if (dy < yb)
                e = ln.sy > 0 ? lastrow(&ln, yb - 1) : b;
            else if (dy > yb || rb == 0)
                e = ln.sy > 0 ? b : lastrow(&ln, rb == 0 ? yb : yb + 1);
            else {
                /* the row that crosses, at pixel xs */
                e = lastrow(&ln, yb);
                dx = x0 + ln.sx * s;
                if (!ln.steep && ln.sx > 0 && dx < xs && xs - 1 - x0 < e)
                    e = xs - 1 - x0;
                if (!ln.steep && ln.sx < 0 && dx >= xs && x0 - xs < e)
                    e = x0 - xs;
            }
            if (e > b)
                e = b;
            if (e < s)
                e = s;
            linerun(&ln, &pl[i], &lj, s, e);
        }
    }
}
