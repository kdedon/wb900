/* SPDX-License-Identifier: MIT */
/*
 * What the LC tests share: lc.gfxdriver, linked with the 8568 model in
 * place of the card, behind a framebuf.resource that reports an LC card,
 * and the checks run after each drawing step.
 */
#ifndef LCTEST_H
#define LCTEST_H

#include <exec/types.h>
#include <graphics/gfx.h>

/* framebuf.resource's AllocFB and FreeFB calls so far */
extern UWORD lct_allocs, lct_frees;

/* Add the resource and the driver, with the model's RAM all fill */
BOOL lct_setup(char *name, UBYTE fill);
void lct_cleanup(void);

/*
 * After step what: the model's bitmap is the shadow's, the chip is idle,
 * no fault; then the step's port actions and their time on the plan's
 * rates.  FALSE on a mismatch.  Returns the time in microseconds in *us.
 */
BOOL lct_step(struct BitMap *bm, char *what, ULONG *us);

/*
 * The registers show bm, 640 dots wide, from VRAM 0x0000 at hz frames a
 * second (rounded down, or 1 less); the geometry is printed.
 */
BOOL lct_mode(struct BitMap *bm, UWORD hz);

#endif
