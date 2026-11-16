/* SPDX-License-Identifier: MIT */
#include <exec/exec.h>
#include <exec/resident.h>
#include <proto/exec.h>
#include <resources/framebuf.h>

#include "vdcmodel.h"
#include "lctest.h"

/* Expected rates at 6 MHz, in microseconds a byte or an action */
#define US_PUSH		15
#define US_SEEK		15
#define US_REG		8
#define US_FILLB	1
#define US_COPYB	2

extern struct Resident lctag;

UWORD lct_allocs, lct_frees;

static char *test;
static struct FBMode fbmode = {
	640, 200, 1, 80, FBF_MONO, 0xFFFF, 0, { NULL, NULL }, 0x10000L
};
static struct FBInfo fbinfo = { FBC_LC, 1, &fbmode, 0 };
static char *owner;
static struct Library *fbres;
static struct LCMStats last;

static struct FBInfo *fb_getinfo(APTR base)
{
	return &fbinfo;
}

static char *fb_alloc(APTR base, char *name)
{
	if (owner)
		return owner;
	owner = name;
	lct_allocs++;
	return NULL;
}

static void fb_free(APTR base)
{
	owner = NULL;
	lct_frees++;
}

static APTR fbfuncs[] = {
	(APTR)fb_getinfo, (APTR)fb_alloc, (APTR)fb_free, (APTR)-1
};

BOOL lct_setup(char *name, UBYTE fill)
{
	test = name;
	if (OpenResource(FRAMEBUFNAME) != NULL) {
		xKPrintf("FAIL %s: a framebuf.resource is there\n", name);
		return FALSE;
	}
	if (!lcm_init(1, fill))
		return FALSE;
	fbres = MakeLibrary(fbfuncs, NULL, NULL, sizeof(struct Library), NULL);
	if (fbres == NULL)
		return FALSE;
	fbres->lib_Node.ln_Name = FRAMEBUFNAME;
	fbres->lib_Node.ln_Type = NT_RESOURCE;
	AddResource(fbres);
	if (InitResident(&lctag, NULL) == NULL)
		return FALSE;
	lcm_watch(TRUE);
	last = lcm_stats;
	return TRUE;
}

void lct_cleanup(void)
{
	lcm_watch(FALSE);
}

BOOL lct_step(struct BitMap *bm, char *what, ULONG *us)
{
	struct LCMStats *s = &lcm_stats;
	UBYTE *p = bm->Planes[0];
	ULONG push, seek, regs, t;
	UWORD a, i, n = bm->BytesPerRow * bm->Rows;
	BOOL ok = TRUE;

	for (a = 0; a < n; a++)
		if (lcm_peek(a) != p[a]) {
			xKPrintf("FAIL %s: %s: byte %u (row %u) is %x, shadow %x\n",
			    test, what, a, a / bm->BytesPerRow, lcm_peek(a), p[a]);
			ok = FALSE;
			break;
		}
	if (!lcm_idle()) {
		xKPrintf("FAIL %s: %s: the chip is left busy\n", test, what);
		ok = FALSE;
	}
	if (s->ls_Lost || s->ls_NVIOn || s->ls_Small || s->ls_Stray) {
		xKPrintf("FAIL %s: %s: lost %ld, NVI on %ld, 16K %ld, stray %ld\n",
		    test, what, s->ls_Lost, s->ls_NVIOn, s->ls_Small, s->ls_Stray);
		ok = FALSE;
	}
	push = s->ls_Data[31] - last.ls_Data[31];
	seek = s->ls_Data[19] - last.ls_Data[19];
	regs = 0;
	for (i = 0; i < 38; i++)
		regs += s->ls_Data[i] - last.ls_Data[i];
	regs -= push + seek;
	t = push * US_PUSH + seek * US_SEEK + regs * US_REG +
	    (s->ls_Fill - last.ls_Fill) * US_FILLB +
	    (s->ls_Copy - last.ls_Copy) * US_COPYB;
	xKPrintf("%s: %s: push %ld seek %ld regs %ld fill %ld copy %ld"
	    " ~%ld us\n", test, what, push, seek, regs,
	    s->ls_Fill - last.ls_Fill, s->ls_Copy - last.ls_Copy, t);
	last = *s;
	if (us)
		*us = t;
	return ok;
}

BOOL lct_mode(struct BitMap *bm, UWORD hz)
{
	struct LCMFrame f;
	ULONG mhz;

	if (!lcm_frame(&f)) {
		xKPrintf("FAIL %s: not a bitmap of 8-pixel columns\n", test);
		return FALSE;
	}
	/* frames a second, times 100 */
	mhz = 100000000L / (f.lf_LineNs * f.lf_Total / 1000);
	xKPrintf("%s: %u x %u, %u dots a pixel, stride %u, %u lines of %ld ns,"
	    " %ld.%02ld Hz\n", test, f.lf_Width, f.lf_Lines, f.lf_Dots,
	    f.lf_Stride, f.lf_Total, f.lf_LineNs, mhz / 100, mhz % 100);
	if (f.lf_Start != 0 || f.lf_Width != bm->BytesPerRow * 8 ||
	    f.lf_Stride != bm->BytesPerRow || f.lf_Lines != bm->Rows ||
	    f.lf_Width * f.lf_Dots != 640 || mhz / 100 + 1 < hz ||
	    mhz / 100 > hz) {
		xKPrintf("FAIL %s: the registers show another bitmap\n", test);
		return FALSE;
	}
	return TRUE;
}
