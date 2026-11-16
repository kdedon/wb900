/* SPDX-License-Identifier: MIT */
#include <exec/exec.h>
#include <exec/execbase.h>
#include <proto/exec.h>

#include "vdcmodel.h"

#define A_NONE	0
#define A_READ	1
#define A_WRITE	2
#define A_FILL	3
#define A_COPY	4

#define FCW_NVIE	0x0800

struct LCMStats lcm_stats;
UBYTE lcm_reg[38];

static UBYTE *ram[2];		/* 0x0000-0x7FFF, 0x8000-0xFFFF */
static UBYTE sel;
static UWORD ua, ba;
static UBYTE wda, da;
static UWORD rnd;
static UBYTE act;		/* pending action */
static UWORD busy;		/* accesses until it completes */
static UWORD wc;
static BOOL intr;
static APTR oldenable;
static BOOL hang;

/* The bits each register reads as 1 */
static const UBYTE rdones[38] = {
	0, 0, 0, 0, 0, 0xE0, 0, 0, 0xFC, 0xE0, 0x80, 0xE0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0xE0, 0, 0, 0, 0, 0x0F, 0xE0, 0, 0,
	0, 0, 0, 0, 0xF0, 0x3F
};

#define RAM(a)	ram[(UWORD)(a) >> 15][(UWORD)(a) & 0x7FFF]

static UWORD random(void)
{
	rnd = rnd * 25173 + 13849;
	return rnd >> 8;
}

static void start(int a, UWORD cost)
{
	act = a;
	busy = cost + (random() & 7);
}

static void run(void)
{
	UWORD n;

	if (act != A_READ && !(lcm_reg[28] & 0x10))
		lcm_stats.ls_Small++;
	switch (act) {
	case A_WRITE:
		RAM(ua) = wda;
		ua++;
		da = RAM(ua);
		break;
	case A_READ:
		da = RAM(ua);
		break;
	case A_FILL:
		n = wc ? wc : 256;
		lcm_stats.ls_Fill += n;
		for (; n > 0; n--, ua++)
			RAM(ua) = wda;
		break;
	case A_COPY:
		n = wc ? wc : 256;
		lcm_stats.ls_Copy += n;
		for (; n > 0; n--, ua++, ba++)
			RAM(ua) = RAM(ba);
		break;
	}
	act = A_NONE;
	intr = TRUE;
}

/* Every access is one step of the chip's time */
static void tick(int fcw)
{
	if (fcw & FCW_NVIE)
		lcm_stats.ls_NVIOn++;
	if (act != A_NONE && !hang && --busy == 0)
		run();
}

int lcm_status(int fcw)
{
	tick(fcw);
	lcm_stats.ls_Status++;
	intr = FALSE;
	if (act != A_NONE) {
		lcm_stats.ls_Busy++;
		return 0x01;
	}
	return 0x81;
}

void lcm_select(int v, int fcw)
{
	tick(fcw);
	lcm_stats.ls_Select++;
	sel = v & 0x3F;
}

void lcm_data(int v, int fcw)
{
	tick(fcw);
	if (sel >= 38)
		return;
	lcm_stats.ls_Data[sel]++;
	if (act != A_NONE && (sel == 18 || sel == 19 || (sel >= 30 && sel <= 33)))
		lcm_stats.ls_Lost++;
	lcm_reg[sel] = v | rdones[sel];
	switch (sel) {
	case 18:
		ua = (ua & 0x00FF) | (v << 8);
		start(A_READ, 1);
		break;
	case 19:
		ua = (ua & 0xFF00) | (v & 0xFF);
		start(A_READ, 1);
		break;
	case 30:
		wc = v & 0xFF;
		if (lcm_reg[24] & 0x80)
			start(A_COPY, 2 * (wc ? wc : 256));
		else
			start(A_FILL, wc ? wc : 256);
		break;
	case 31:
		wda = v;
		start(A_WRITE, 2);
		break;
	case 32:
		ba = (ba & 0x00FF) | (v << 8);
		break;
	case 33:
		ba = (ba & 0xFF00) | (v & 0xFF);
		break;
	}
}

BOOL lcm_init(UWORD seed, UBYTE fill)
{
	UWORD i;

	if (ram[0] == NULL) {
		ram[0] = AllocMem(0x8000L, MEMF_ANY);
		ram[1] = AllocMem(0x8000L, MEMF_ANY);
		if (ram[0] == NULL || ram[1] == NULL) {
			lcm_free();
			return FALSE;
		}
	}
	for (i = 0; i < 0x8000; i++)
		ram[0][i] = ram[1][i] = fill;
	for (i = 0; i < 38; i++)
		lcm_reg[i] = rdones[i];
	lcm_reg[37] = 0xFF;
	sel = 0;
	ua = ba = wc = 0;
	wda = da = 0;
	act = A_NONE;
	intr = FALSE;
	hang = FALSE;
	rnd = seed;
	for (i = 0; i < sizeof(lcm_stats); i++)
		((UBYTE *)&lcm_stats)[i] = 0;
	return TRUE;
}

void lcm_free(void)
{
	if (ram[0])
		FreeMem(ram[0], 0x8000L);
	if (ram[1])
		FreeMem(ram[1], 0x8000L);
	ram[0] = ram[1] = NULL;
}

UBYTE lcm_peek(UWORD a)
{
	return RAM(a);
}

void lcm_poke(UWORD a, UBYTE v)
{
	RAM(a) = v;
}

void lcm_hang(BOOL on)
{
	hang = on;
}

BOOL lcm_idle(void)
{
	return act == A_NONE && !intr;
}

static void watchenable(void)
{
	if (!lcm_idle())
		lcm_stats.ls_Stray++;
	(*(void (*)())oldenable)();
}

void lcm_watch(BOOL on)
{
	if (on && oldenable == NULL)
		oldenable = SetFunction(SysBase, LVOEnable, watchenable);
	else if (!on && oldenable != NULL) {
		SetFunction(SysBase, LVOEnable, oldenable);
		oldenable = NULL;
	}
}

BOOL lcm_frame(struct LCMFrame *f)
{
	UWORD cth = lcm_reg[22] >> 4, dbl = (lcm_reg[25] >> 4) & 1;
	UWORD rows = lcm_reg[6];

	/* shown rows count from 1; VSYNC from row R7 blanks the rest */
	if (lcm_reg[7] != 0 && lcm_reg[7] <= rows)
		rows = lcm_reg[7] - 1;

	f->lf_Start = lcm_reg[12] << 8 | lcm_reg[13];
	f->lf_Width = lcm_reg[1] * 8;
	f->lf_Stride = lcm_reg[1] + lcm_reg[27];
	f->lf_Lines = rows * ((lcm_reg[9] & 0x1F) + 1);
	f->lf_Dots = dbl + 1;
	f->lf_Total = (lcm_reg[4] + 1) * ((lcm_reg[9] & 0x1F) + 1) +
	    (lcm_reg[5] & 0x1F);
	/* a column is pixels 0 to R22's upper nibble, or 1 to it doubled */
	f->lf_LineNs = (lcm_reg[0] + 1L) * (dbl ? 2 * cth : cth + 1) * 125 / 2;
	f->lf_Lace = (lcm_reg[8] & 3) == 3;
	f->lf_Even = lcm_reg[6] * ((lcm_reg[9] & 0x1F) + 1) / 2;
	return ((lcm_reg[8] & 3) == 0 || f->lf_Lace) &&
	    (lcm_reg[25] & 0xC0) == 0x80 && (dbl ? cth : cth + 1) == 8;
}

UWORD lcm_line(const struct LCMFrame *f, UWORD n)
{
	if (f->lf_Lace)
		n = n & 1 ? f->lf_Even + n / 2 : n / 2;
	return f->lf_Start + n * f->lf_Stride;
}
