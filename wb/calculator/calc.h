/* SPDX-License-Identifier: MIT */
#ifndef CALC_H
#define CALC_H

/* ND decimal digits, NF of them after the point, least significant first */
#define ND 24
#define NF 10

struct Num
{
    BYTE neg;
    UBYTE d[ND];
};

void num_zero(struct Num *r);
void num_parse(struct Num *r, char *s);
BOOL num_add(struct Num *a, struct Num *b, struct Num *r);
BOOL num_sub(struct Num *a, struct Num *b, struct Num *r);
BOOL num_mul(struct Num *a, struct Num *b, struct Num *r);
BOOL num_div(struct Num *a, struct Num *b, struct Num *r);
BOOL num_format(struct Num *a, char *buf, WORD max);

#endif
