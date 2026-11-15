/* SPDX-License-Identifier: MIT */
/*
 * Decimal fixed-point numbers for Calculator: ND digits, NF of them after
 * the point, least significant first.  Integer maths only, as the soft
 * float is slow at 6 MHz.
 */
#include <exec/types.h>

#include "calc.h"

static BOOL iszero(struct Num *a)
{
    WORD i;

    for (i = 0; i < ND; i++)
        if (a->d[i])
            return FALSE;
    return TRUE;
}

static WORD ucmp(struct Num *a, struct Num *b)
{
    WORD i;

    for (i = ND - 1; i >= 0; i--)
        if (a->d[i] != b->d[i])
            return a->d[i] > b->d[i] ? 1 : -1;
    return 0;
}

/* |a| + |b|; FALSE on overflow */
static BOOL uadd(struct Num *a, struct Num *b, struct Num *r)
{
    WORD i, c = 0;

    for (i = 0; i < ND; i++) {
        c += a->d[i] + b->d[i];
        r->d[i] = c % 10;
        c /= 10;
    }
    return c == 0;
}

/* |a| - |b|, |a| >= |b| */
static void usub(struct Num *a, struct Num *b, struct Num *r)
{
    WORD i, c = 0;

    for (i = 0; i < ND; i++) {
        c = a->d[i] - b->d[i] - c;
        r->d[i] = c < 0 ? c + 10 : c;
        c = c < 0;
    }
}

void num_zero(struct Num *r)
{
    WORD i;

    r->neg = 0;
    for (i = 0; i < ND; i++)
        r->d[i] = 0;
}

/* An edit buffer such as "-12.5"; digits past NF are dropped */
void num_parse(struct Num *r, char *s)
{
    char *p, *dot;
    WORD i;

    num_zero(r);
    if (*s == '-') {
        r->neg = 1;
        s++;
    }
    for (dot = s; *dot && *dot != '.'; dot++)
        ;
    for (p = dot, i = NF; p > s && i < ND; i++)
        r->d[i] = *--p - '0';
    if (*dot)
        for (p = dot + 1, i = NF - 1; *p && i >= 0; i--)
            r->d[i] = *p++ - '0';
    if (iszero(r))
        r->neg = 0;
}

BOOL num_add(struct Num *a, struct Num *b, struct Num *r)
{
    BYTE neg;

    if (a->neg == b->neg) {
        neg = a->neg;
        if (!uadd(a, b, r))
            return FALSE;
    } else if (ucmp(a, b) >= 0) {
        neg = a->neg;
        usub(a, b, r);
    } else {
        neg = b->neg;
        usub(b, a, r);
    }
    r->neg = iszero(r) ? 0 : neg;
    return TRUE;
}

BOOL num_sub(struct Num *a, struct Num *b, struct Num *r)
{
    struct Num t;

    t = *b;
    t.neg = !t.neg;
    return num_add(a, &t, r);
}

/* Rounded to NF places */
BOOL num_mul(struct Num *a, struct Num *b, struct Num *r)
{
    UWORD acc[2 * ND + 1];
    WORD i, j;
    BYTE neg = a->neg != b->neg;

    for (i = 0; i <= 2 * ND; i++)
        acc[i] = 0;
    for (i = 0; i < ND; i++)
        if (a->d[i])
            for (j = 0; j < ND; j++)
                acc[i + j] += a->d[i] * b->d[j];
    for (i = 0; i < 2 * ND; i++) {
        acc[i + 1] += acc[i] / 10;
        acc[i] %= 10;
    }
    if (acc[NF - 1] >= 5)
        for (i = NF; ++acc[i] == 10; i++)
            acc[i] = 0;
    for (i = ND + NF; i <= 2 * ND; i++)
        if (acc[i])
            return FALSE;
    for (i = 0; i < ND; i++)
        r->d[i] = acc[i + NF];
    r->neg = iszero(r) ? 0 : neg;
    return TRUE;
}

/* Truncated to NF places; FALSE for a zero divisor or overflow */
BOOL num_div(struct Num *a, struct Num *b, struct Num *r)
{
    UBYTE rem[ND + 1], q[ND + NF];
    WORD i, k, c;
    BYTE neg = a->neg != b->neg;

    if (iszero(b))
        return FALSE;
    for (i = 0; i <= ND; i++)
        rem[i] = 0;
    /* a * 10^NF, a digit at a time from the top */
    for (k = ND + NF - 1; k >= 0; k--) {
        for (i = ND; i > 0; i--)
            rem[i] = rem[i - 1];
        rem[0] = k >= NF ? a->d[k - NF] : 0;
        for (q[k] = 0;; q[k]++) {
            if (rem[ND] == 0) {
                for (i = ND - 1; i >= 0 && rem[i] == b->d[i]; i--)
                    ;
                if (i >= 0 && rem[i] < b->d[i])
                    break;
            }
            for (i = c = 0; i <= ND; i++) {
                c = rem[i] - (i < ND ? b->d[i] : 0) - c;
                rem[i] = c < 0 ? c + 10 : c;
                c = c < 0;
            }
        }
    }
    for (k = ND; k < ND + NF; k++)
        if (q[k])
            return FALSE;
    for (i = 0; i < ND; i++)
        r->d[i] = q[i];
    r->neg = iszero(r) ? 0 : neg;
    return TRUE;
}

/*
 * At most max characters and a sign, rounded to fit, without trailing
 * zeros.  FALSE when the integer part does not fit.
 */
BOOL num_format(struct Num *a, char *buf, WORD max)
{
    struct Num n, unit;
    WORD top, nf, i, cut;
    char *p = buf;

    n = *a;
    for (top = ND - 1; top > NF && n.d[top] == 0; top--)
        ;
    if (top - NF + 1 > max)
        return FALSE;
    nf = max - (top - NF + 1) - 1;
    if (nf > NF)
        nf = NF;
    if (nf < 0)
        nf = 0;
    cut = NF - nf;
    if (cut > 0) {
        num_zero(&unit);
        unit.d[cut] = n.d[cut - 1] >= 5;
        for (i = 0; i < cut; i++)
            n.d[i] = 0;
        if (!uadd(&n, &unit, &n))
            return FALSE;
        /* A carry into a new integer digit leaves zeros behind it */
        for (top = ND - 1; top > NF && n.d[top] == 0; top--)
            ;
        if (top - NF + 1 > max)
            return FALSE;
    }
    if (n.neg && !iszero(&n))
        *p++ = '-';
    for (i = top; i >= NF; i--)
        *p++ = '0' + n.d[i];
    for (cut = 0; cut < NF && n.d[cut] == 0; cut++)
        ;
    if (cut < NF) {
        *p++ = '.';
        for (i = NF - 1; i >= cut; i--)
            *p++ = '0' + n.d[i];
    }
    *p = 0;
    return TRUE;
}
