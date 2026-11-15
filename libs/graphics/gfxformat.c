/* SPDX-License-Identifier: MIT */
/*
 * The row code of both RAM bitmap formats: fills, templates, copies and
 * lines on rows of 1 or 4 bits a pixel, a word at a time, with the long
 * runs in gfxasm.s.  A row that crosses from the first 64 KB of a plane
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

WORD gfx_Planes(struct BitMap *bm, const struct GfxPens *p,
    struct GfxPlane *pl, UWORD *bpp)
{
    register struct GfxPlane *q = pl;
    register PLANEPTR *pp = bm->Planes;
    register UWORD i;
    UBYTE *hi = IS_GFXBM(bm) ? GFXBM(bm)->gb_Half1 : NULL;
    UBYTE m = p->gp_Mask, a = p->gp_APen, b = p->gp_BPen;
    UWORD depth = bm->Depth;

    if (GFX_DRIVER(bm)->gd_Format == GDF_CHUNKY4) {
        *bpp = 4;
        if ((m & 15) == 0)
            return 0;
        q->pl_Lo = *pp;
        q->pl_Hi = hi;
        q->pl_F = (a & 15) * 0x1111;
        q->pl_B = (b & 15) * 0x1111;
        q->pl_M = (m & 15) * 0x1111;
        return 1;
    }
    *bpp = 1;
    if (depth > 8)
        depth = 8;
    for (i = 0; i < depth; i++, pp++, a >>= 1, b >>= 1, m >>= 1) {
        if (!(m & 1) || *pp == NULL || *pp == (PLANEPTR)-1)
            continue;
        q->pl_Lo = *pp;
        q->pl_Hi = i == 0 ? hi : NULL;
        q->pl_F = (a & 1) ? 0xFFFF : 0;
        q->pl_B = (b & 1) ? 0xFFFF : 0;
        q->pl_M = 0xFFFF;
        q++;
    }
    return q - pl;
}

/*
 * Rows from offset o that stay in one half of the plane and do not
 * cross into the other, stepping step bytes: the first row's [o, o + len)
 * crosses (0), or the count.
 */
static UWORD runrows(ULONG o, UWORD len, LONG step, UWORD rows)
{
    ULONG k, last;

    if (o < 0x10000L && o + len > 0x10000L)
        return 0;
    /* the rows from o to the last are on one side of 0x10000 */
    last = o + (LONG)(rows - 1) * step;
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
 * pattern row, a[0-3] then x[0-3].  FALSE when
 * they change nothing.
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
    UWORD bpp, bpr = bm->BytesPerRow, w0, n, lm, rm, k, e, rows, t, m, a, x;
    UBYTE dm = p->gp_DrMd;
    WORD np, i, bx0, bx1;
    ULONG o;

    np = gfx_Planes(bm, p, pl, &bpp);
    bx0 = r->MinX * bpp;
    bx1 = r->MaxX * bpp + bpp - 1;
    w0 = bx0 >> 4;
    n = (bx1 >> 4) - w0 + 1;
    lm = 0xFFFF >> (bx0 & 15);
    rm = 0xFFFF << (15 - (bx1 & 15));
    rows = r->MaxY - r->MinY + 1;
    o = (ULONG)r->MinY * bpr + (w0 << 1);
    if (p->gp_Ptrn == NULL) {
        t = (dm & INVERSVID) ? 0 : 0xFFFF;
        for (i = 0; i < np; i++) {
            m = pl[i].pl_M;
            if (dm & COMPLEMENT) {
                a = 0xFFFF;
                x = t & m;
            } else if (dm & JAM2) {
                a = ~m;
                x = ((t & pl[i].pl_F) | (~t & pl[i].pl_B)) & m;
            } else {
                a = ~(t & m);
                x = pl[i].pl_F & t & m;
            }
            if (a != 0xFFFF || x != 0)
                fillspan(&pl[i], o, n, rows, bpr, a, x, lm, rm);
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

/* gfx_TmplRows' job; its modes */
struct tmpljob {
    UWORD	*tj_D;
    WORD	 tj_DStep;
    UWORD	 tj_Rows;
    const UBYTE	*tj_Src;
    UWORD	 tj_SrcMod;
    WORD	 tj_TB;
    UWORD	 tj_N, tj_F, tj_B, tj_M, tj_LM, tj_RM, tj_Mode;
    const UWORD	*tj_Nib;
};

#define T_JAM2		0
#define T_JAM1		1
#define T_COMP		4
#define T_NOT		2
#define T_PIX		8	/* chunky: nib holds the pixels to store */

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
        gfx_TmplRows(tj);
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
        gfx_TmplRows(tj);
    }
}

void gfx_Template(struct BitMap *bm, const struct Rectangle *r,
    const UBYTE *src, UWORD srcx, UWORD srcmod, const struct GfxPens *p)
{
    struct GfxPlane pl[8];
    struct tmpljob tj;
    UWORD pix[16];
    UWORD bpp, ppw, bpr = bm->BytesPerRow;
    UWORD qlen = ((srcx + (r->MaxX - r->MinX)) >> 3) + 1;
    UWORD w0, n, n1, lm, rm, c, cnt, rows, k, mode;
    UBYTE dm = p->gp_DrMd;
    WORD np, i, y, bx0, bx1, tx, tb;
    const UBYTE *s;
    ULONG o, oc;

    np = gfx_Planes(bm, p, pl, &bpp);
    if (np == 0)
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
    for (i = 0; i < np; i++) {
        tj.tj_F = pl[i].pl_F;
        tj.tj_B = pl[i].pl_B;
        if (mode == T_JAM2) {
            /* ((f ^ b) & t) ^ b, or with t inverted ^ f */
            tj.tj_F ^= pl[i].pl_B;
            if (dm & INVERSVID)
                tj.tj_B = pl[i].pl_F;
        }
        tj.tj_M = pl[i].pl_M;
        tj.tj_Mode = mode;
        tj.tj_Nib = bpp == 1 ? NULL : nib4;
        if (bpp == 4 && mode == T_JAM2 && tj.tj_M == 0xFFFF) {
            /* the words of pixels for each 4 template bits */
            for (k = 0; k < 16; k++)
                pix[k] = (tj.tj_F & nib4[k]) ^ tj.tj_B;
            tj.tj_Mode = T_PIX;
            tj.tj_Nib = pix;
        }
        for (c = 0; c < n; c += cnt) {
            cnt = n - c < TPASS ? n - c : TPASS;
            tb = tx + (WORD)(c * ppw);
            o = (ULONG)r->MinY * bpr + ((w0 + c) << 1);
            s = src;
            y = r->MinY;
            for (rows = r->MaxY - r->MinY + 1; rows > 0; rows -= k) {
                k = runrows(o, cnt << 1, bpr, rows);
                if (k > 0) {
                    tmplrows(&tj, (UWORD *)PL_AT(&pl[i], o), k, s, tb, cnt,
                        c == 0 ? lm : 0xFFFF, c + cnt == n ? rm : 0xFFFF,
                        y + k - 1 == r->MaxY, qlen);
                } else {
                    /* a row that crosses into the second half */
                    k = 1;
                    n1 = (UWORD)((0x10000L - o) >> 1);
                    tmplrows(&tj, (UWORD *)PL_AT(&pl[i], o), 1, s, tb, n1,
                        c == 0 ? lm : 0xFFFF, 0xFFFF, y == r->MaxY, qlen);
                    tmplrows(&tj, (UWORD *)pl[i].pl_Hi, 1, s,
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
    /* whole rows of words: the same bits of the source words, or shifted */
    aligned = cj.cj_TB >= 0 && minterm == 0xC0;
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
        so = (ULONG)(up ? sy + h - 1 : sy) * sbpr + (sw << 1);
        dof = (ULONG)(up ? r->MaxY : r->MinY) * dbpr + (w0 << 1);
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
                    so += sstep * k;
                    dof += dstep * k;
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

/* gfx_LineRun's job */
struct linejob {
    UBYTE	 lj_Mask[8];
    UBYTE	*lj_Lo, *lj_Hi;
    ULONG	 lj_YO;
    WORD	 lj_X, lj_Y, lj_D, lj_IncrE, lj_IncrNE, lj_Count;
    WORD	 lj_SX, lj_SY;
    LONG	 lj_YStep;
    WORD	 lj_Steep;
    UWORD	 lj_Ptrn;
    WORD	 lj_CX0, lj_CW, lj_CY0, lj_CH;
    UWORD	 lj_F, lj_B, lj_Mode;
    WORD	 lj_Shift;
};

void gfx_Line(struct BitMap *bm, const struct Rectangle *clip,
    WORD x0, WORD y0, WORD x1, WORD y1, UWORD ptrn,
    const struct GfxPens *p)
{
    struct GfxPlane pl[8];
    struct linejob lj;
    UWORD bpp, bpr = bm->BytesPerRow;
    WORD dx, dy, np, i, k;
    UBYTE m;

    np = gfx_Planes(bm, p, pl, &bpp);
    dx = x1 - x0;
    dy = y1 - y0;
    lj.lj_SX = dx > 0 ? 1 : -1;
    lj.lj_SY = dy > 0 ? 1 : -1;
    lj.lj_YStep = dy > 0 ? (LONG)bpr : -(LONG)bpr;
    if (dx < 0)
        dx = -dx;
    if (dy < 0)
        dy = -dy;
    lj.lj_Steep = dx < dy;
    if (lj.lj_Steep) {
        k = dx;
        dx = dy;
        dy = k;
    }
    /* The blitter's line mode steps diagonally on a tie. */
    lj.lj_IncrE = 2 * dy;
    lj.lj_IncrNE = 2 * (dy - dx);
    /* none after the major axis leaves the clip */
    if (lj.lj_Steep)
        k = lj.lj_SY > 0 ? clip->MaxY - y0 : y0 - clip->MinY;
    else
        k = lj.lj_SX > 0 ? clip->MaxX - x0 : x0 - clip->MinX;
    if (k < 0)
        return;
    lj.lj_Count = (k < dx ? k : dx) + 1;
    lj.lj_Ptrn = ptrn;
    lj.lj_CX0 = clip->MinX;
    lj.lj_CW = clip->MaxX - clip->MinX;
    lj.lj_CY0 = clip->MinY;
    lj.lj_CH = clip->MaxY - clip->MinY;
    lj.lj_Shift = bpp == 1 ? -3 : -1;
    if (p->gp_DrMd & COMPLEMENT)
        lj.lj_Mode = 2;
    else
        lj.lj_Mode = (p->gp_DrMd & JAM2) ? 0 : 1;
    for (i = 0; i < np; i++) {
        m = (UBYTE)pl[i].pl_M;
        for (k = 0; k < 8; k++)
            lj.lj_Mask[k] = (bpp == 1 ? 0x80 >> k : ((k & 1) ? 0x0F : 0xF0)) & m;
        lj.lj_Lo = pl[i].pl_Lo;
        lj.lj_Hi = pl[i].pl_Hi;
        /* x's byte from lo, or the second half from 0x10000 */
        lj.lj_YO = (ULONG)y0 * bpr + (UWORD)(ULONG)pl[i].pl_Lo;
        lj.lj_F = (UBYTE)pl[i].pl_F;
        lj.lj_B = (UBYTE)pl[i].pl_B;
        lj.lj_X = x0;
        lj.lj_Y = y0;
        lj.lj_D = 2 * dy - dx;
        gfx_LineRun(&lj);
    }
}
