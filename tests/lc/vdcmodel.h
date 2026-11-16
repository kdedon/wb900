/* SPDX-License-Identifier: MIT */
/*
 * A model of the LC card's 8568 ports for the LC tests: the registers,
 * 64 KB of RAM, the update address with auto-increment, block fill and
 * copy, and Update Ready, which stays low for a random number of port
 * accesses after each action.  An action runs when it completes, so a
 * write that replaces a pending one loses it, as on the chip.
 * lcm_frame() reads the display's geometry from the registers.
 */
#ifndef VDCMODEL_H
#define VDCMODEL_H

#include <exec/types.h>

struct LCMStats {
	ULONG	ls_Status;	/* status reads */
	ULONG	ls_Busy;	/* of them, not ready */
	ULONG	ls_Select;	/* index writes */
	ULONG	ls_Data[38];	/* data writes by register */
	ULONG	ls_Fill;	/* bytes block fills wrote */
	ULONG	ls_Copy;	/* bytes block copies wrote */
	/* faults: each should stay 0 */
	ULONG	ls_Lost;	/* R18, R19, R30-R33 written while busy */
	ULONG	ls_NVIOn;	/* accesses with NVI enabled */
	ULONG	ls_Small;	/* RAM actions without R28's 64 KB bit */
	ULONG	ls_Stray;	/* Enable() with an action pending or INTR set */
};

extern struct LCMStats lcm_stats;
/* As the chip reads them back: unused bits are 1 */
extern UBYTE lcm_reg[38];

/* The bitmap the registers show, without interlace (R8 = 0) */
struct LCMFrame {
	UWORD	lf_Start;	/* R12/R13 */
	UWORD	lf_Width;	/* pixels a line: R1 columns of 8 */
	UWORD	lf_Stride;	/* bytes from a line to the next: R1 + R27 */
	UWORD	lf_Lines;	/* R6 rows of R9 + 1 lines */
	UWORD	lf_Dots;	/* dots a pixel: 2 at double width (R25 bit 4) */
	UWORD	lf_Total;	/* lines a frame: R4 + 1 rows, plus R5 */
	ULONG	lf_LineNs;	/* time of a line, at 16 MHz dots */
};

/* FALSE unless bitmap mode, no attributes and 8 pixels a column */
BOOL lcm_frame(struct LCMFrame *f);

/* Reset with all registers 0 and RAM filled with fill; FALSE without memory */
BOOL lcm_init(UWORD seed, UBYTE fill);
void lcm_free(void);
UBYTE lcm_peek(UWORD a);
void lcm_poke(UWORD a, UBYTE v);
/* Keep a pending action from completing */
void lcm_hang(BOOL on);
/* No action pending and INTR released */
BOOL lcm_idle(void);
/* Count Enable() calls that leave the chip able to raise an NVI */
void lcm_watch(BOOL on);

/* The port accesses, with the FCW at the access */
int lcm_status(int fcw);
void lcm_select(int v, int fcw);
void lcm_data(int v, int fcw);

#endif
