/ SPDX-License-Identifier: MIT
/ The LC card's 8568 ports for lc.gfxdriver.  Each data write first waits
/ for Update Ready (status bit 7), at most LCPOLL polls for a write, or
/ for a whole lcv_push run.  The callers keep interrupts off around a
/ run and end it with lcv_ready, which releases the chip's INTR.
/ Port accesses take only these forms, r2 the status and index port, r3
/ the data port, so that they can be replaced by calls:
/	inb rl0, @r2	outb @r2, rl1	outb @r3, rl1	outib @r3, @rr4, r1

	.globl	lcv_wr_, lcv_seek_, lcv_push_, lcv_ready_

LCPOLL	= 30000

	.shri

/ Wait for Update Ready: Z clear when ready, set after LCPOLL polls.
/ Uses r0 and r4.
rdy:
	ld	r4, $LCPOLL
1:	inb	rl0, @r2
	bitb	rl0, $7
	ret	nz
	djnz	r4, 1b
	ret

/ Select register rl1, then write rh1 to it when ready; Z set on time-out.
wr:
	outb	@r2, rl1
	calr	rdy
	ret	z
	ldb	rl1, rh1
	outb	@r3, rl1
	ret

/ int lcv_wr(reg, val): FALSE if the chip stays busy
lcv_wr_:
	ld	r2, $0x181
	ld	r3, $0x183
	ld	r1, rr14(6)
	ldb	rh1, rl1
	ld	r0, rr14(4)
	ldb	rl1, rl0
	calr	wr
	jr	z, 9f
	ld	r1, $1
	ret
9:	clr	r1
	ret

/ int lcv_seek(addr): the update address (R18, R19); FALSE on time-out
lcv_seek_:
	ld	r2, $0x181
	ld	r3, $0x183
	ld	r5, rr14(4)
	ldb	rh1, rh5
	ldb	rl1, $18
	calr	wr
	jr	z, 9f
	ldb	rh1, rl5
	ldb	rl1, $19
	calr	wr
	jr	z, 9f
	ld	r1, $1
	ret
9:	clr	r1
	ret

/ int lcv_push(src, n): n bytes from src through R31; FALSE on time-out
lcv_push_:
	push	(rr14), r6
	ld	r2, $0x181
	ld	r3, $0x183
	ldl	rr4, rr14(6)
	ld	r1, $31
	outb	@r2, rl1
	ld	r1, rr14(10)
	test	r1
	jr	z, 8f
	ld	r6, $LCPOLL
1:	inb	rl0, @r2
	bitb	rl0, $7
	jr	nz, 2f
	djnz	r6, 1b
	clr	r1
	jr	9f
2:	outib	@r3, @rr4, r1
	jr	nov, 1b
8:	ld	r1, $1
9:	pop	r6, (rr14)
	ret

/ int lcv_ready(): wait until the chip is idle; FALSE on time-out
lcv_ready_:
	ld	r2, $0x181
	calr	rdy
	jr	z, 9f
	ld	r1, $1
	ret
9:	clr	r1
	ret
