/ SPDX-License-Identifier: MIT
/ The 8568 loops tests/lcbench.c times, each with the cycles of one pass
/ when the chip is ready at the first poll.  Polls that find it busy
/ are counted in r5 and returned.  r2 is the status and index port, r3
/ the data port.

	.globl	lcb_djnz_, lcb_sel_, lcb_selp_, lcb_r31_, lcb_r31np_
	.globl	lcb_otir_, lcb_read_, lcb_seek_, lcb_block_, lcb_wr_
	.globl	lcb_ready_, lcb_rd_

	.shri

ports:
	ld	r2, $0x181
	ld	r3, $0x183
	clr	r5
	ret

/ Wait for Update Ready, counting busy polls.
rdy:
	inb	rl0, @r2
	bitb	rl0, $7
	ret	nz
	inc	r5, $1
	jr	rdy

/ lcb_djnz(n): djnz 11
lcb_djnz_:
	ld	r1, rr14(4)
1:	djnz	r1, 1b
	ret

/ lcb_sel(n): outb 10, djnz 11 = 21
lcb_sel_:
	calr	ports
	ld	r4, rr14(4)
	ld	r1, $31
1:	outb	@r2, rl1
	djnz	r4, 1b
	ld	r1, r5
	ret

/ lcb_selp(n): outb 10, inb 10, bitb 4, jr 6, djnz 11 = 41
lcb_selp_:
	calr	ports
	ld	r4, rr14(4)
	ld	r1, $31
1:	outb	@r2, rl1
2:	inb	rl0, @r2
	bitb	rl0, $7
	jr	nz, 3f
	inc	r5, $1
	jr	2b
3:	djnz	r4, 1b
	ld	r1, r5
	ret

/ lcb_r31(n, v): R31 writes, each after a poll: inb 10, bitb 4, jr 6,
/ outb 10, djnz 11 = 41
lcb_r31_:
	calr	ports
	ld	r4, rr14(4)
	ld	r1, $31
	outb	@r2, rl1
	ld	r1, rr14(6)
1:	inb	rl0, @r2
	bitb	rl0, $7
	jr	nz, 2f
	inc	r5, $1
	jr	1b
2:	outb	@r3, rl1
	djnz	r4, 1b
	ld	r1, r5
	ret

/ lcb_r31np(src, n): R31 writes with no poll: outib 21, jr 6 = 27
lcb_r31np_:
	calr	ports
	ldl	rr4, rr14(4)
	ld	r1, $31
	outb	@r2, rl1
	ld	r1, rr14(8)
1:	outib	@r3, @rr4, r1
	jr	nov, 1b
	clr	r1
	ret

/ lcb_otir(src, n): R31 writes by otirb, 10 a byte
lcb_otir_:
	calr	ports
	ldl	rr4, rr14(4)
	ld	r1, $31
	outb	@r2, rl1
	ld	r1, rr14(8)
	otirb	@r3, @rr4, r1
	clr	r1
	ret

/ lcb_read(dst, n): R31 reads, each after a poll
lcb_read_:
	pushl	(rr14), rr6
	calr	ports
	ldl	rr6, rr14(8)
	ld	r4, rr14(12)
	ld	r1, $31
	outb	@r2, rl1
1:	calr	rdy
	inb	rl0, @r3
	ldb	(rr6), rl0
	inc	r7, $1
	djnz	r4, 1b
	ld	r1, r5
	popl	rr6, (rr14)
	ret

/ lcb_seek(n, a): R18 and R19, each after a poll: 2 x (ldb 5, outb 10,
/ calr 15, inb 10, bitb 4, ret 13, ldb 3, outb 10) + djnz 11 = 151
lcb_seek_:
	pushl	(rr14), rr6
	calr	ports
	ld	r4, rr14(8)
	ld	r6, rr14(10)
1:	ldb	rl1, $18
	outb	@r2, rl1
	calr	rdy
	ldb	rl1, rh6
	outb	@r3, rl1
	ldb	rl1, $19
	outb	@r2, rl1
	calr	rdy
	ldb	rl1, rl6
	outb	@r3, rl1
	djnz	r4, 1b
	ld	r1, r5
	popl	rr6, (rr14)
	ret

/ lcb_block(n, count): R30 writes, each after a poll
lcb_block_:
	pushl	(rr14), rr6
	calr	ports
	ld	r4, rr14(8)
	ld	r6, rr14(10)
1:	ldb	rl1, $30
	outb	@r2, rl1
	calr	rdy
	ldb	rl1, rl6
	outb	@r3, rl1
	djnz	r4, 1b
	calr	rdy
	ld	r1, r5
	popl	rr6, (rr14)
	ret

/ lcb_wr(reg, val)
lcb_wr_:
	calr	ports
	ld	r1, rr14(4)
	outb	@r2, rl1
	calr	rdy
	ld	r1, rr14(6)
	outb	@r3, rl1
	ld	r1, r5
	ret

/ lcb_ready(): busy polls until ready
lcb_ready_:
	calr	ports
	calr	rdy
	ld	r1, r5
	ret

/ lcb_rd(reg): register reg as it reads
lcb_rd_:
	calr	ports
	ld	r1, rr14(4)
	outb	@r2, rl1
	inb	rl1, @r3
	clrb	rh1
	ret
