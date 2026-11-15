/ SPDX-License-Identifier: MIT
/ The LC card's 8568 ports for lc.gfxdriver (lcport.h).  Each data write
/ first waits for Update Ready (status bit 7).  Every action takes its
/ estimated time from the piece's budget; when that runs out, brk waits
/ for the chip, which releases its INTR, then calls Enable and Disable.
/ Port accesses take only these forms, r2 the status and index port, r3
/ the data port, so that they can be replaced by calls:
/	inb rl0, @r2	outb @r2, rl1	outb @r3, rl1	outib @r3, @rr4, r1
/
/ In the entries: rr12 the struct LCPort, r8 R24, r9 the copy source
/ address, r10 the update address, r11 the budget.

	.globl	lcv_wr_, lcv_ready_, lcv_push_, lcv_fill_, lcv_copy_
	.globl	lcv_flush_, lcv_mark_, SysBase_

LCPOLL	= 30000
PIECE	= 1000
US_PUSH	= 16
US_REG	= 10
GAP	= 1
SCRATCH	= 0x4000

/ struct LCPort
SHADOW	= 0
UA	= 4
BA	= 6
BUDGET	= 8
OK	= 10
R24	= 12
Y0	= 14
Y1	= 16
SP	= 20
LO	= 24
HI	= 224
BITS	= 424
RUNS	= 2424
CX	= 2506

	.shri

/ Entries start: sub r15, $16; ldm (rr14), r6, $8; ldl rr12, rr14(20);
/ calr load; jp z, fail.  The arguments follow the struct at rr14(24).
/ load keeps the frame for die; Z set when the chip is dead.
load:
	ldl	rr0, rr14
	inc	r1, $4
	ldl	rr12(SP), rr0
	ld	r2, $0x181
	ld	r3, $0x183
	ld	r8, rr12(R24)
	ld	r9, rr12(BA)
	ld	r10, rr12(UA)
	ld	r11, rr12(BUDGET)
	ld	r0, rr12(OK)
	test	r0
	ret

done:
	ld	rr12(R24), r8
	ld	rr12(BA), r9
	ld	rr12(UA), r10
	ld	rr12(BUDGET), r11
	ld	r1, $1
	jr	out

/ A poll timed out: the chip is dead.
die:
	ldl	rr14, rr12(SP)
	clr	r0
	ld	rr12(OK), r0
fail:
	clr	r1
out:
	ldm	r6, (rr14), $8
	add	r15, $16
	ret

/ Wait for Update Ready.  Uses r0, r7.
rdy:
	ld	r7, $LCPOLL
1:	inb	rl0, @r2
	bitb	rl0, $7
	ret	nz
	djnz	r7, 1b
	jp	die

/ End the piece and start the next.  Keeps r0-r10.
brk:
	calr	rdy
	sub	r15, $12
	ldm	(rr14), r0, $6
	ldl	rr4, SysBase_
	sub	r5, $126
	call	(rr4)
	ldl	rr4, SysBase_
	sub	r5, $120
	call	(rr4)
	ldm	r0, (rr14), $6
	add	r15, $12
	ld	r11, $PIECE
	ret

/ Write rh1 to register rl1.  Uses r0, r1, r7.
wreg:
	sub	r11, $US_REG
	jr	pl, 1f
	calr	brk
	sub	r11, $US_REG
1:	outb	@r2, rl1
	calr	rdy
	ldb	rl1, rh1
	outb	@r3, rl1
	ret

/ The update address to r6; R18 only when its high byte changes.
seek:
	cp	r10, r6
	ret	eq
	ld	r0, r10
	xor	r0, r6
	and	r0, $0xFF00
	jr	z, 1f
	ldb	rh1, rh6
	ldb	rl1, $18
	calr	wreg
1:	ldb	rh1, rl6
	ldb	rl1, $19
	calr	wreg
	ld	r10, r6
	ret

/ R24 bit 7 to rl0's.
mode:
	ld	r1, r8
	and	r1, $0x80
	cpb	rl1, rl0
	ret	eq
	and	r8, $0x7F
	clrb	rh0
	or	r8, r0
	ld	r1, r8
	ldb	rh1, rl1
	ldb	rl1, $24
	jr	wreg

/ r1 > 0 bytes from the shadow at r6, as many a piece as its budget
/ allows.  Uses r0, r1, r4-r7.
wpush:
	push	(rr14), r1
	calr	seek
	pop	r1, (rr14)
	ldl	rr4, rr12(SHADOW)
	add	r5, r6
	add	r10, r1
	ld	r6, r1
	ld	r1, $31
	outb	@r2, rl1
1:	ld	r1, r11
	sra	r1, $4
	test	r1
	jr	gt, 2f
	calr	brk
	jr	1b
2:	cp	r1, r6
	jr	le, 3f
	ld	r1, r6
3:	sub	r6, r1
	ld	r0, r1
	sll	r0, $4
	sub	r11, r0
	ld	r7, $LCPOLL
4:	inb	rl0, @r2
	bitb	rl0, $7
	jr	nz, 5f
	djnz	r7, 4b
	jp	die
5:	outib	@r3, @rr4, r1
	jr	nov, 4b
	test	r6
	jr	nz, 1b
	ret

/ R30 blocks of r4 bytes in all, fills (r5 = 0) or copies (r5 = 1),
/ at most 256 a block.  Fill costs 1 us a byte, copy 2.
blocks:
	test	r4
	ret	z
1:	ld	r1, r4
	cp	r1, $256
	jr	ule, 2f
	ld	r1, $256
2:	ld	r0, r1
	test	r5
	jr	z, 3f
	add	r0, r1
	add	r9, r1
3:	sub	r11, r0
	jr	pl, 4f
	calr	brk
	sub	r11, r0
4:	sub	r4, r1
	add	r10, r1
	ldb	rh1, rl1
	ldb	rl1, $30
	add	r11, $US_REG
	calr	wreg
	test	r4
	jr	nz, 1b
	ret

/ r4 > 0 bytes of rl5 at r6: R31 writes the first, blocks the rest.
fill:
	calr	seek
	clrb	rl0
	calr	mode
	ldb	rh1, rl5
	ldb	rl1, $31
	sub	r11, $US_PUSH-US_REG
	calr	wreg
	inc	r10, $1
	dec	r4, $1
	clr	r5
	jr	blocks

/ r4 > 0 bytes from r0 to r6.
copy:
	push	(rr14), r0
	calr	seek
	pop	r5, (rr14)
	cp	r5, r9
	jr	eq, 2f
	ld	r0, r5
	xor	r0, r9
	and	r0, $0xFF00
	jr	z, 1f
	ldb	rh1, rh5
	ldb	rl1, $32
	calr	wreg
1:	ldb	rh1, rl5
	ldb	rl1, $33
	calr	wreg
	ld	r9, r5
2:	ldb	rl0, $0x80
	calr	mode
	ld	r5, $1
	jr	blocks

/ int lcv_wr(lp, reg, val)
lcv_wr_:
	sub	r15, $16
	ldm	(rr14), r6, $8
	ldl	rr12, rr14(20)
	calr	load
	jp	z, fail
	ld	r1, rr14(26)
	ldb	rh1, rl1
	ld	r0, rr14(24)
	ldb	rl1, rl0
	cpb	rl1, $24
	jr	ne, 1f
	ldb	rl0, rh1
	clrb	rh0
	ld	r8, r0
1:	calr	wreg
	jp	done

/ int lcv_ready(lp)
lcv_ready_:
	sub	r15, $16
	ldm	(rr14), r6, $8
	ldl	rr12, rr14(20)
	calr	load
	jp	z, fail
	calr	rdy
	jp	done

/ int lcv_push(lp, a, n)
lcv_push_:
	sub	r15, $16
	ldm	(rr14), r6, $8
	ldl	rr12, rr14(20)
	calr	load
	jp	z, fail
	ld	r6, rr14(24)
	ld	r1, rr14(26)
	test	r1
	jp	z, done
	calr	wpush
	jp	done

/ int lcv_fill(lp, a, v, n, rows, edges)
lcv_fill_:
	sub	r15, $16
	ldm	(rr14), r6, $8
	ldl	rr12, rr14(20)
	calr	load
	jp	z, fail
1:	ld	r0, rr14(32)
	bit	r0, $0
	jr	z, 2f
	ld	r6, rr14(24)
	dec	r6, $1
	ld	r1, $1
	calr	wpush
2:	ld	r6, rr14(24)
	ld	r5, rr14(26)
	ld	r4, rr14(28)
	calr	fill
	ld	r0, rr14(32)
	bit	r0, $1
	jr	z, 3f
	ld	r6, rr14(24)
	ld	r0, rr14(28)
	add	r6, r0
	ld	r1, $1
	calr	wpush
3:	ld	r0, rr14(24)
	add	r0, $80
	ld	rr14(24), r0
	ld	r0, rr14(30)
	dec	r0, $1
	ld	rr14(30), r0
	jr	nz, 1b
	jp	done

/ int lcv_copy(lp, d, s, n, rows, step, edges)
lcv_copy_:
	sub	r15, $16
	ldm	(rr14), r6, $8
	ldl	rr12, rr14(20)
	calr	load
	jp	z, fail
1:	ld	r0, rr14(34)
	bit	r0, $2
	jr	z, 2f
	ld	r6, $SCRATCH
	ld	r0, rr14(26)
	ld	r4, rr14(28)
	calr	copy
2:	ld	r0, rr14(34)
	bit	r0, $0
	jr	z, 3f
	ld	r6, rr14(24)
	dec	r6, $1
	ld	r1, $1
	calr	wpush
3:	ld	r6, rr14(24)
	ld	r0, rr14(26)
	ld	r1, rr14(34)
	bit	r1, $2
	jr	z, 4f
	ld	r0, $SCRATCH
4:	ld	r4, rr14(28)
	calr	copy
	ld	r0, rr14(34)
	bit	r0, $1
	jr	z, 5f
	ld	r6, rr14(24)
	ld	r0, rr14(28)
	add	r6, r0
	ld	r1, $1
	calr	wpush
5:	ld	r1, rr14(32)
	ld	r0, rr14(24)
	add	r0, r1
	ld	rr14(24), r0
	ld	r0, rr14(26)
	add	r0, r1
	ld	rr14(26), r0
	ld	r0, rr14(30)
	dec	r0, $1
	ld	rr14(30), r0
	jr	nz, 1b
	jp	done

/ int lcv_flush(lp).  A row of one span pushes it; the bit bytes of a
/ row with more become a list of runs at RUNS (first and last byte, 0xFF
/ at the end), which are then pushed.  Locals: rr14(0) the next row,
/ (2) the row's address, (4) the next run.
lcv_flush_:
	sub	r15, $16
	ldm	(rr14), r6, $8
	ldl	rr12, rr14(20)
	calr	load
	jp	z, fail
	sub	r15, $6
	ld	r0, rr12(Y0)
	ld	rr14(0), r0
frow:
	ld	r1, rr14(0)
	ld	r7, rr12(Y1)
	ld	r0, r7
	sub	r0, r1
	jp	lt, fend
	inc	r0, $1
	ldl	rr4, rr12
	add	r5, $LO
	add	r5, r1
	ldb	rl6, $0xFF
	cpirb	rl6, (rr4), r0, ne
	jp	nz, fend
	dec	r5, $1
	sub	r5, r13
	ld	r4, r5
	ld	r1, r5
	sub	r1, $LO
	ldb	rl6, rr12(r4)
	ld	r0, r1
	inc	r0, $1
	ld	rr14(0), r0
	ld	r5, r4
	add	r5, $HI-LO
	ldb	rl7, rr12(r5)
	clrb	rh6
	clrb	rh7
	ldb	rl0, $0xFF
	ldb	rr12(r4), rl0
	clrb	rl0
	ldb	rr12(r5), rl0
	ld	r5, r1
	sll	r5, $4
	ld	r0, r5
	sll	r5, $2
	add	r5, r0
	ld	rr14(2), r5
	ld	r2, r4
	add	r2, $CX-LO
	ldb	rl0, rr12(r2)
	testb	rl0
	jr	nz, 3f
	ld	r2, $0x181
	ld	r1, r7
	sub	r1, r6
	inc	r1, $1
	add	r6, r5
	calr	wpush
	jr	frow
/ scan: r6 the byte, r7 the last, r4 the bit byte's offset, r5 the run
/ list, rl1/rh1 the run's first and last byte (rl1 0xFF: none)
3:	clrb	rl0
	ldb	rr12(r2), rl0
	ld	r4, r1
	sll	r4, $1
	ld	r0, r4
	sll	r4, $2
	add	r4, r0
	add	r4, $BITS
	ld	r0, r6
	srl	r0, $3
	add	r4, r0
	and	r6, $0xFFF8
	ld	r5, $RUNS
	ldb	rl1, $0xFF
fgrp:
	cp	r6, r7
	jr	gt, fscan
	ldb	rl0, rr12(r4)
	testb	rl0
	jr	z, fskip
	cpb	rl0, $0xFF
	jr	eq, ffull
	ldb	rh2, rl6
	clrb	rh0
1:	srl	r0, $1
	jr	nc, 2f
	calr	dirty
2:	inc	r6, $1
	test	r0
	jr	nz, 1b
	ldb	rr12(r4), rl0
	ldb	rl6, rh2
	jr	fskip
ffull:
	calr	dirty
	ldb	rh1, rl6
	addb	rh1, $7
	clrb	rl0
	ldb	rr12(r4), rl0
fskip:
	add	r6, $8
	inc	r4, $1
	jr	fgrp
fscan:
	cpb	rl1, $0xFF
	jr	eq, 1f
	ldb	rr12(r5), rl1
	inc	r5, $1
	ldb	rr12(r5), rh1
	inc	r5, $1
1:	ldb	rl0, $0xFF
	ldb	rr12(r5), rl0
	ld	r2, $0x181
	ld	r0, $RUNS
	ld	rr14(4), r0
femit:
	ld	r4, rr14(4)
	ldb	rl6, rr12(r4)
	cpb	rl6, $0xFF
	jp	eq, frow
	inc	r4, $1
	ldb	rl1, rr12(r4)
	inc	r4, $1
	ld	rr14(4), r4
	clrb	rh6
	clrb	rh1
	sub	r1, r6
	inc	r1, $1
	ld	r0, rr14(2)
	add	r6, r0
	calr	wpush
	jr	femit
fend:
	ld	r0, $200
	ld	rr12(Y0), r0
	ld	r0, $-1
	ld	rr12(Y1), r0
	add	r15, $6
	jp	done

/ Byte r6 is dirty: it extends the run in rl1/rh1, or ends it and
/ starts the next.  Uses rl2.
dirty:
	cpb	rl1, $0xFF
	jr	eq, 1f
	ldb	rl2, rl6
	subb	rl2, rh1
	cpb	rl2, $GAP+1
	jr	ule, 2f
	ldb	rr12(r5), rl1
	inc	r5, $1
	ldb	rr12(r5), rh1
	inc	r5, $1
1:	ldb	rl1, rl6
2:	ldb	rh1, rl6
	ret


/ void lcv_mark(lp, y0, y1, b0, b1, set).  A row keeps one span, LO to
/ HI, while its marks touch it; one that does not makes the row CX, its
/ bytes in BITS.  In the loop: r3 the rows left, r4 and r5 b0 and b1,
/ r7 the row's BITS offset, r9 set, r10 its LO offset, r11 HI's.
lcv_mark_:
	sub	r15, $16
	ldm	(rr14), r6, $8
	ldl	rr12, rr14(20)
	ld	r2, rr14(24)
	ld	r3, rr14(26)
	ld	r4, rr14(28)
	ld	r5, rr14(30)
	ld	r9, rr14(32)
	test	r9
	jr	z, 1f
	ld	r0, rr12(Y0)
	cp	r2, r0
	jr	ge, 2f
	ld	rr12(Y0), r2
2:	ld	r0, rr12(Y1)
	cp	r3, r0
	jr	le, 1f
	ld	rr12(Y1), r3
1:	sub	r3, r2
	inc	r3, $1
	ld	r7, r2
	sll	r7, $1
	ld	r0, r7
	sll	r7, $2
	add	r7, r0
	add	r7, $BITS
	ld	r10, r2
	add	r10, $LO
mrow:
	ldb	rl0, rr12(r10)
	ld	r11, r10
	add	r11, $HI-LO
	ldb	rl1, rr12(r11)
	ld	r2, r10
	add	r2, $CX-LO
	ldb	rh0, rr12(r2)
	test	r9
	jr	z, mclean
	cpb	rl0, $0xFF
	jr	ne, 1f
	ldb	rr12(r10), rl4
	ldb	rr12(r11), rl5
	jr	mnext
1:	testb	rh0
	jr	nz, mcx
	ldb	rh1, rl1
	addb	rh1, $GAP+1
	cpb	rl4, rh1
	jr	ugt, 2f
	ldb	rh1, rl5
	addb	rh1, $GAP+1
	cpb	rh1, rl0
	jr	uge, mwiden
/ a second span: the first goes to BITS
2:	ldb	rh0, $1
	ldb	rr12(r2), rh0
	calr	mspan
mcx:
	ld	r6, r4
	ld	r8, r5
	calr	bits
	ldb	rl0, rr12(r10)
	ld	r11, r10
	add	r11, $HI-LO
	ldb	rl1, rr12(r11)
mwiden:
	cpb	rl0, rl4
	jr	ule, 1f
	ldb	rr12(r10), rl4
1:	cpb	rl1, rl5
	jr	uge, mnext
	ldb	rr12(r11), rl5
mnext:
	inc	r10, $1
	add	r7, $10
	djnz	r3, mrow
	ldm	r6, (rr14), $8
	add	r15, $16
	ret
mclean:
	cpb	rl0, $0xFF
	jr	eq, mnext
	testb	rh0
	jr	z, 1f
	ld	r6, r4
	ld	r8, r5
	calr	bits
	jr	mnext
1:	cpb	rl5, rl0
	jr	ult, mnext
	cpb	rl4, rl1
	jr	ugt, mnext
	cpb	rl4, rl0
	jr	ugt, 3f
	cpb	rl5, rl1
	jr	ult, 2f
	ldb	rl0, $0xFF
	ldb	rr12(r10), rl0
	clrb	rl0
	ldb	rr12(r11), rl0
	jr	mnext
2:	ldb	rh1, rl5
	incb	rh1, $1
	ldb	rr12(r10), rh1
	jr	mnext
3:	cpb	rl5, rl1
	jr	ult, 4f
	ldb	rh1, rl4
	decb	rh1, $1
	ldb	rr12(r11), rh1
	jr	mnext
/ the middle of the span: the row goes to BITS
4:	ldb	rh0, $1
	ldb	rr12(r2), rh0
	ld	r9, $1
	calr	mspan
	clr	r9
	ld	r6, r4
	ld	r8, r5
	calr	bits
	jr	mnext

/ The row's span, rl0 to rl1, to its bits.
mspan:
	ld	r6, r0
	and	r6, $0xFF
	ld	r8, r1
	and	r8, $0xFF
/ Set (r9 != 0) or clear the bits of bytes r6-r8 of the row at BITS
/ offset r7.  Uses r0, r1, r6, r8, r11.
bits:
	ld	r0, r6
	and	r0, $7
	ld	r1, $0xFF
	sdl	r1, r0
	ld	r0, r8
	and	r0, $7
	sub	r0, $7
	ld	r11, $0xFF
	sdl	r11, r0
	ld	r0, r11
	ldb	rh1, rl0
	srl	r6, $3
	srl	r8, $3
	sub	r8, r6
	add	r6, r7
	test	r8
	jr	nz, 1f
	andb	rl1, rh1
1:	ldb	rl0, rr12(r6)
	test	r9
	jr	z, 2f
	orb	rl0, rl1
	jr	3f
2:	comb	rl1
	comb	rh1
	andb	rl0, rl1
3:	ldb	rr12(r6), rl0
	test	r8
	ret	z
	clrb	rl0
	test	r9
	jr	z, 4f
	ldb	rl0, $0xFF
4:	dec	r8, $1
	jr	z, 6f
5:	inc	r6, $1
	ldb	rr12(r6), rl0
	djnz	r8, 5b
6:	inc	r6, $1
	ldb	rl0, rr12(r6)
	test	r9
	jr	z, 7f
	orb	rl0, rh1
	jr	8f
7:	andb	rl0, rh1
8:	ldb	rr12(r6), rl0
	ret
