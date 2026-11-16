/ SPDX-License-Identifier: MIT
/ The word loops of graphics.library's row code (gfxformat.c).  A run
/ never crosses a 64 KB segment: the callers split rows there, so only
/ the offset words step.  r6-r13 are the caller's and are saved.

	.globl	gfx_FillRows_, gfx_FillRect_, gfx_TmplRows_, gfx_TmplRows1_
	.globl	gfx_CopyRows_, gfx_ShiftRows_
	.globl	gfx_LineRun_, gfx_WCopy_, gfx_WSwap_, gfx_Bits_, gfx_MulDiv_, gfx_Glyphs_, gfx_FastChars_

	.shri

/ gfx_FillRows(d, n, rows, bpr, tab, pmask, pidx): n words of each of
/ rows rows bpr bytes apart, d = (d & a) ^ x.  Row r takes a[0-3] and
/ x[0-3], words in turn, from entry (pidx + r) & pmask of tab, 8 words
/ an entry.
gfx_FillRows_:
	sub	r15, $16
	ldm	(rr14), r6, $8
	ld	r0, rr14(26)
	test	r0
	jr	z, 3f
	ldl	rr12, rr14(20)
	ld	r0, rr14(34)
	test	r0
	jr	nz, 2f
/ one entry for every row: stores, or read, and, xor, write
	ldl	rr2, rr14(30)
	ldm	r4, (rr2), $8
	ld	r0, r4
	or	r0, r5
	or	r0, r6
	or	r0, r7
	jr	nz, 4f
	ld	r4, r8
	ld	r5, r9
	ld	r6, r10
	ld	r7, r11
1:	ldl	rr2, rr12
	ld	r1, rr14(24)
	calr	wstore
	ld	r0, rr14(28)
	add	r13, r0
	ld	r0, rr14(26)
	dec	r0, $1
	ld	rr14(26), r0
	jr	nz, 1b
	jr	3f
4:	ldl	rr2, rr12
	ld	r1, rr14(24)
	calr	wrmw
	ld	r0, rr14(28)
	add	r13, r0
	ld	r0, rr14(26)
	dec	r0, $1
	ld	rr14(26), r0
	jr	nz, 4b
	jr	3f
/ the entry of each row's pattern row
2:	ld	r0, rr14(36)
	ld	r1, r0
	inc	r1, $1
	ld	rr14(36), r1
	ld	r1, rr14(34)
	and	r0, r1
	sll	r0, $4
	ldl	rr2, rr14(30)
	add	r3, r0
	ldm	r4, (rr2), $8
	ldl	rr2, rr12
	ld	r1, rr14(24)
	calr	wrun
	ld	r0, rr14(28)
	add	r13, r0
	ld	r0, rr14(26)
	dec	r0, $1
	ld	rr14(26), r0
	jr	nz, 2b
3:	ldm	r6, (rr14), $8
	add	r15, $16
	ret

/ gfx_FillRect(d, n, rows, bpr, a, x, lm, rm): n words of each of rows
/ rows bpr bytes apart, d = (d & a) ^ x, the first word under lm and
/ the last under rm (a single word under lm).  Middles of 8 words or
/ more with a 0 are stored 8 words at a time, x in r4-r11 and the
/ edges' a and x on the frame.
gfx_FillRect_:
	sub	r15, $16
	ldm	(rr14), r6, $8
	ld	r12, rr14(26)
	test	r12
	jp	z, fr9
	ldl	rr2, rr14(20)
	ld	r10, rr14(24)
	ld	r13, rr14(28)
	sub	r13, r10
	sub	r13, r10
	ld	r4, rr14(30)
	ld	r5, rr14(32)
	ld	r6, rr14(34)
	ld	r7, r6
	com	r6
	or	r6, r4
	and	r7, r5
	ld	r8, rr14(36)
	ld	r9, r8
	com	r8
	or	r8, r4
	and	r9, r5
	dec	r10, $1
	jr	nz, fr2
/ one word a row
fr1:	ld	r0, (rr2)
	and	r0, r6
	xor	r0, r7
	ld	(rr2), r0
	inc	r3, $2
	add	r3, r13
	djnz	r12, fr1
	jr	fr9
fr2:	dec	r10, $1
	test	r4
	jr	nz, fr3
	cp	r10, $8
	jr	uge, fr5
/ read, and, xor, write
fr3:	ld	r0, (rr2)
	and	r0, r6
	xor	r0, r7
	ld	(rr2), r0
	inc	r3, $2
	ld	r1, r10
	test	r1
	jr	z, fr4
fr31:	ld	r0, (rr2)
	and	r0, r4
	xor	r0, r5
	ld	(rr2), r0
	inc	r3, $2
	djnz	r1, fr31
fr4:	ld	r0, (rr2)
	and	r0, r8
	xor	r0, r9
	ld	(rr2), r0
	inc	r3, $2
	add	r3, r13
	djnz	r12, fr3
	jr	fr9
/ stores
fr5:	ld	rr14(30), r6
	ld	rr14(32), r7
	ld	rr14(34), r8
	ld	rr14(36), r9
	ld	rr14(24), r10
	ld	r4, r5
	ld	r6, r5
	ld	r7, r5
	ld	r8, r5
	ld	r9, r5
	ld	r10, r5
	ld	r11, r5
fr6:	ld	r0, (rr2)
	ld	r1, rr14(30)
	and	r0, r1
	ld	r1, rr14(32)
	xor	r0, r1
	ld	(rr2), r0
	inc	r3, $2
	ld	r1, rr14(24)
fr7:	ldm	(rr2), r4, $8
	add	r3, $16
	sub	r1, $8
	cp	r1, $8
	jr	uge, fr7
	test	r1
	jr	z, fr8
fr71:	ld	(rr2), r4
	inc	r3, $2
	djnz	r1, fr71
fr8:	ld	r0, (rr2)
	ld	r1, rr14(34)
	and	r0, r1
	ld	r1, rr14(36)
	xor	r0, r1
	ld	(rr2), r0
	inc	r3, $2
	add	r3, r13
	djnz	r12, fr6
fr9:	ldm	r6, (rr14), $8
	add	r15, $16
	ret

/ r1 words from rr2 = (word & a) ^ x, a in r4-r7 and x in r8-r11 in
/ turn.  With all of a 0 only stores, 16 words at a time with LDM.
/ Uses r0-r11.
wrun:
	ld	r0, r4
	or	r0, r5
	or	r0, r6
	or	r0, r7
	jr	nz, wrmw
	ld	r4, r8
	ld	r5, r9
	ld	r6, r10
	ld	r7, r11
/ stores of r4-r11, x0-x3 twice; r0-r3 change
wstore:
	test	r1
	jr	z, 9f
1:	cp	r1, $16
	jr	ult, 2f
	ldm	(rr2), r4, $8
	add	r3, $16
	ldm	(rr2), r4, $8
	add	r3, $16
	sub	r1, $16
	jr	1b
2:	cp	r1, $8
	jr	ult, 3f
	ldm	(rr2), r4, $8
	add	r3, $16
	sub	r1, $8
3:	test	r1
	jr	z, 9f
	ld	(rr2), r4
	inc	r3, $2
	dec	r1, $1
	jr	z, 9f
	ld	(rr2), r5
	inc	r3, $2
	dec	r1, $1
	jr	z, 9f
	ld	(rr2), r6
	inc	r3, $2
	dec	r1, $1
	jr	z, 9f
	ld	(rr2), r7
	inc	r3, $2
	dec	r1, $1
	jr	z, 9f
	ld	(rr2), r8
	inc	r3, $2
	dec	r1, $1
	jr	z, 9f
	ld	(rr2), r9
	inc	r3, $2
	dec	r1, $1
	jr	z, 9f
	ld	(rr2), r10
	ret
/ read, and, xor, write; r0-r3 change
wrmw:
	test	r1
	jr	z, 9f
5:	cp	r1, $4
	jr	ult, 6f
	ld	r0, (rr2)
	and	r0, r4
	xor	r0, r8
	ld	(rr2), r0
	inc	r3, $2
	ld	r0, (rr2)
	and	r0, r5
	xor	r0, r9
	ld	(rr2), r0
	inc	r3, $2
	ld	r0, (rr2)
	and	r0, r6
	xor	r0, r10
	ld	(rr2), r0
	inc	r3, $2
	ld	r0, (rr2)
	and	r0, r7
	xor	r0, r11
	ld	(rr2), r0
	inc	r3, $2
	sub	r1, $4
	jr	5b
6:	test	r1
	jr	z, 9f
	ld	r0, (rr2)
	and	r0, r4
	xor	r0, r8
	ld	(rr2), r0
	inc	r3, $2
	dec	r1, $1
	jr	z, 9f
	ld	r0, (rr2)
	and	r0, r5
	xor	r0, r9
	ld	(rr2), r0
	inc	r3, $2
	dec	r1, $1
	jr	z, 9f
	ld	r0, (rr2)
	and	r0, r6
	xor	r0, r10
	ld	(rr2), r0
9:	ret

/ gfx_TmplRows(job): rows of a template at 4 bits a pixel, job
/ (gfxformat.c) the words
/  0 d (long)	4 dstep	6 rows	8 src (long)	12 srcmod
/  14 tb	16 n	18 f	20 b	22 m	24 lm	26 rm
/  28 mode	30 nib (long)
/ d, rows and src step through the rows.  Each row's template words
/ are the bits from bit tb (-15 on) of src, read up to the byte after
/ the last, 16 a group of 4 words, each 8 bits looked up in nib's
/ longs.  Then n words
/ of d, the first under lm and the last under rm, from the template
/ words t under the mask m by mode: 0 JAM2, ((f & t) ^ b) (f is APen ^
/ BPen); 1 JAM1, f where t; 4 complement where t; with 2 added, where
/ not t.
/ Chunky JAM2 under all of m writes the groups of 4 words straight to d
/ and puts back what lm and rm leave of the end words.
/ The frame: 0 the template words, 64 the expanded words, 128 job,
/ 132 and 134 the end words of d before.
gfx_TmplRows_:
	sub	r15, $16
	ldm	(rr14), r6, $8
	ldl	rr12, rr14(20)
	sub	r15, $136
	ldl	rr14(128), rr12
tr1:	ldl	rr12, rr14(128)
	ld	r0, rr12(6)
	test	r0
	jp	z, tr9
	dec	r0, $1
	ld	rr12(6), r0
/ the template words: r1 of them from tb (r9) of src (rr4) to rr2
	ld	r1, rr12(16)
	add	r1, $3
	srl	r1, $2
	ld	r9, rr12(14)
	ldl	rr4, rr12(8)
	ldl	rr2, rr14
	test	r9
	jr	pl, tr3
	ldb	rh0, (rr4)
	ldb	rl0, rr4(1)
	sdl	r0, r9
	ld	(rr2), r0
	inc	r3, $2
	add	r9, $16
	dec	r1, $1
	jr	z, tr5
tr3:	ld	r8, r9
	and	r8, $7
	srl	r9, $3
	add	r5, r9
	ldb	rl0, (rr4)
tr4:	ldb	rh6, rl0
	ldb	rl6, rr4(1)
	ldb	rl0, rr4(2)
	ldb	rh7, rl0
	sdll	rr6, r8
	ld	(rr2), r6
	inc	r3, $2
	inc	r5, $2
	djnz	r1, tr4
/ each template byte to 2 words of masks through nib (rr6)
tr5:	ldl	rr12, rr14(128)
	ldl	rr6, rr12(30)
	ldl	rr4, rr14
	ld	r0, rr12(28)
	test	r0
	jr	nz, tr60
	ld	r0, rr12(22)
	cp	r0, $0xffff
	jr	ne, tr60
	ld	r9, rr12(16)
	srl	r9, $2
	jr	z, tr60
/ JAM2: r12 f, r13 b, r9 the groups of 4 words in n
	ldl	rr2, rr12(0)
	ld	r0, (rr2)
	ld	rr14(132), r0
	ld	r1, rr12(16)
	dec	r1, $1
	add	r1, r1
	ld	r0, rr2(r1)
	ld	rr14(134), r0
	ld	r10, rr12(18)
	ld	r11, rr12(20)
	ld	r12, r10
	ld	r13, r11
tr6f:	ld	r0, (rr4)
	inc	r5, $2
	ldb	rl1, rh0
	clrb	rh1
	sll	r1, $2
	ldl	rr10, rr6(r1)
	and	r10, r12
	xor	r10, r13
	and	r11, r12
	xor	r11, r13
	ldl	(rr2), rr10
	inc	r3, $4
	ldb	rl1, rl0
	clrb	rh1
	sll	r1, $2
	ldl	rr10, rr6(r1)
	and	r10, r12
	xor	r10, r13
	and	r11, r12
	xor	r11, r13
	ldl	(rr2), rr10
	inc	r3, $4
	djnz	r9, tr6f
/ the 0-3 words left, the last under rm (r8)
	ldl	rr10, rr14(128)
	ld	r9, rr10(16)
	ld	r8, rr10(26)
	and	r9, $3
	jr	nz, tr6g
	dec	r3, $2
	ld	r0, (rr2)
	ld	r10, rr14(134)
	xor	r0, r10
	and	r0, r8
	xor	r0, r10
	ld	(rr2), r0
	jr	tr6j
tr6g:	ld	r0, (rr4)
	ldb	rl1, rh0
	clrb	rh1
	sll	r1, $2
	ldl	rr10, rr6(r1)
	ldb	rl1, rl0
	clrb	rh1
	sll	r1, $2
	ldl	rr0, rr6(r1)
	and	r10, r12
	xor	r10, r13
	and	r11, r12
	xor	r11, r13
	and	r0, r12
	xor	r0, r13
tr6h:	cp	r9, $1
	jr	eq, tr6i
	ld	(rr2), r10
	inc	r3, $2
	ld	r10, r11
	ld	r11, r0
	dec	r9, $1
	jr	tr6h
tr6i:	ld	r1, (rr2)
	xor	r10, r1
	and	r10, r8
	xor	r10, r1
	ld	(rr2), r10
/ the first word under lm
tr6j:	ldl	rr12, rr14(128)
	ldl	rr2, rr12(0)
	ld	r1, rr12(24)
	ld	r0, (rr2)
	ld	r10, rr14(132)
	xor	r0, r10
	and	r0, r1
	xor	r0, r10
	ld	(rr2), r0
	jp	tr8
/ 4 words a template word, as many as n rounded up
tr60:	ld	r9, rr12(16)
	ldl	rr2, rr14
	add	r3, $64
tr6:	ld	r0, (rr4)
	inc	r5, $2
	ldb	rl1, rh0
	clrb	rh1
	sll	r1, $2
	ldl	rr10, rr6(r1)
	ldl	(rr2), rr10
	inc	r3, $4
	ldb	rl1, rl0
	clrb	rh1
	sll	r1, $2
	ldl	rr10, rr6(r1)
	ldl	(rr2), rr10
	inc	r3, $4
	sub	r9, $4
	jr	gt, tr6
	ldl	rr4, rr14
	add	r5, $64
/ the words of d from the template words at rr4
	ldl	rr2, rr12(0)
	ld	r1, rr12(16)
	ld	r6, rr12(18)
	ld	r7, rr12(20)
	ld	r8, rr12(22)
	ld	r9, rr12(24)
	ld	r10, rr12(26)
	ld	r12, rr12(28)
	and	r9, r8
/ JAM2 under all of m, 3 words or more: only the end words read d
	test	r12
	jr	nz, tr71
	cp	r8, $0xffff
	jr	ne, tr71
	cp	r1, $3
	jr	ult, tr71
	ld	r0, (rr4)
	inc	r5, $2
	and	r0, r6
	xor	r0, r7
	ld	r11, (rr2)
	xor	r0, r11
	and	r0, r9
	xor	r0, r11
	ld	(rr2), r0
	inc	r3, $2
	sub	r1, $2
tr70:	ld	r0, (rr4)
	inc	r5, $2
	and	r0, r6
	xor	r0, r7
	ld	(rr2), r0
	inc	r3, $2
	djnz	r1, tr70
	ld	r0, (rr4)
	and	r0, r6
	xor	r0, r7
	ld	r11, (rr2)
	xor	r0, r11
	and	r0, r10
	xor	r0, r11
	ld	(rr2), r0
	jr	tr8
/ r13 is this word's mask: r9 for the first, then m; rm for the last
tr71:	ld	r13, r9
	ld	r9, r8
	cp	r1, $1
	jr	ne, tr72
	and	r13, r10
tr72:	ld	r0, (rr4)
	inc	r5, $2
	test	r12
	jr	nz, tr74
/ JAM2
	and	r0, r6
	xor	r0, r7
	cp	r13, $0xffff
	jr	ne, tr73
	ld	(rr2), r0
	jr	tr78
tr73:	ld	r11, (rr2)
	xor	r0, r11
	and	r0, r13
	xor	r0, r11
	ld	(rr2), r0
	jr	tr78
tr74:	bit	r12, $1
	jr	z, tr75
	com	r0
tr75:	and	r0, r13
	jr	z, tr78
	ld	r11, (rr2)
	cp	r12, $4
	jr	uge, tr76
/ JAM1: d ^ ((d ^ f) & k)
	ld	r13, r11
	xor	r13, r6
	and	r0, r13
tr76:	xor	r11, r0
	ld	(rr2), r11
tr78:	inc	r3, $2
	djnz	r1, tr71
/ the next row
tr8:	ldl	rr12, rr14(128)
	ldl	rr2, rr12(0)
	ld	r0, rr12(4)
	add	r3, r0
	ldl	rr12(0), rr2
	ldl	rr2, rr12(8)
	ld	r0, rr12(12)
	add	r3, r0
	ldl	rr12(8), rr2
	jp	tr1
tr9:	add	r15, $136
	ldm	r6, (rr14), $8
	add	r15, $16
	ret

/ gfx_TmplRows1(job): gfx_TmplRows for 1 bit a pixel, a word of d from
/ each template word as it is read.  The frame: 0 d, 4 src, 8 dstep,
/ 10 srcmod, 12 rows, 14 n, 16 the first word's mask, 18 m, 20 the last
/ word's, 22 tb when negative (the first word from 2 bytes), 24 the
/ byte of the template words after the first, 26 the mode: 0 JAM2 under
/ all of m, 1 JAM2, 2 JAM1, 3 complement.  r8 is the template words'
/ shift, r10 f and r11 b, or with JAM1 and complement all 1s for a
/ template taken where not set.
gfx_TmplRows1_:
	sub	r15, $16
	ldm	(rr14), r6, $8
	sub	r15, $28
	ldl	rr12, rr14(48)
	ld	r0, rr12(6)
	test	r0
	jp	z, t10
	ld	rr14(12), r0
	ldl	rr2, rr12(0)
	ldl	rr14(0), rr2
	ldl	rr2, rr12(8)
	ldl	rr14(4), rr2
	ld	r0, rr12(4)
	ld	rr14(8), r0
	ld	r0, rr12(12)
	ld	rr14(10), r0
	ld	r9, rr12(16)
	ld	rr14(14), r9
	ld	r1, rr12(22)
	ld	rr14(18), r1
	ld	r2, rr12(26)
	and	r2, r1
	ld	rr14(20), r2
	ld	r0, rr12(24)
	and	r0, r1
	cp	r9, $1
	jr	ne, 1f
	and	r0, r2
1:	ld	rr14(16), r0
	ld	r0, rr12(14)
	clr	r2
	test	r0
	jr	pl, 2f
	ld	r2, r0
	add	r0, $16
2:	ld	rr14(22), r2
	ld	r8, r0
	and	r8, $7
	srl	r0, $3
	ld	rr14(24), r0
	ld	r10, rr12(18)
	ld	r11, rr12(20)
	ld	r0, rr12(28)
	clr	r2
	test	r0
	jr	nz, 3f
	cp	r1, $0xffff
	jr	eq, 5f
	inc	r2, $1
	jr	5f
3:	ld	r2, $2
	bit	r0, $2
	jr	z, 4f
	inc	r2, $1
4:	clr	r11
	bit	r0, $1
	jr	z, 5f
	com	r11
5:	ld	rr14(26), r2
/ a row: the first template word
t1:	ldl	rr2, rr14(0)
	ldl	rr4, rr14(4)
	ld	r9, rr14(14)
	ld	r12, rr14(16)
	ld	r1, rr14(22)
	test	r1
	jr	z, 6f
	ldb	rh6, (rr4)
	ldb	rl6, rr4(1)
	sdl	r6, r1
	ld	r1, rr14(24)
	add	r5, r1
	ldb	rl0, (rr4)
	jr	7f
6:	ld	r1, rr14(24)
	add	r5, r1
	ldb	rl0, (rr4)
	ldb	rh6, rl0
	ldb	rl6, rr4(1)
	ldb	rl0, rr4(2)
	ldb	rh7, rl0
	sdll	rr6, r8
	inc	r5, $2
7:	ld	r1, rr14(26)
	test	r1
	jr	z, t2fa
	dec	r1, $1
	jp	z, t2a
	dec	r1, $1
	jp	z, t1a
	jp	tca
/ JAM2 under all of m: the middle words are only written
t2fa:
	and	r6, r10
	xor	r6, r11
	ld	r13, (rr2)
	xor	r6, r13
	and	r6, r12
	xor	r13, r6
	ld	(rr2), r13
	inc	r3, $2
	dec	r9, $1
	jp	z, t9
	ld	r12, rr14(18)
	dec	r9, $1
	jr	z, t2fc
t2fb:
	ldb	rh6, rl0
	ldb	rl6, rr4(1)
	ldb	rl0, rr4(2)
	ldb	rh7, rl0
	sdll	rr6, r8
	inc	r5, $2
	and	r6, r10
	xor	r6, r11
	ld	(rr2), r6
	inc	r3, $2
	djnz	r9, t2fb
t2fc:	ld	r12, rr14(20)
	ldb	rh6, rl0
	ldb	rl6, rr4(1)
	ldb	rl0, rr4(2)
	ldb	rh7, rl0
	sdll	rr6, r8
	inc	r5, $2
	and	r6, r10
	xor	r6, r11
	ld	r13, (rr2)
	xor	r6, r13
	and	r6, r12
	xor	r13, r6
	ld	(rr2), r13
	inc	r3, $2
	jp	t9
/ JAM2
t2a:
	and	r6, r10
	xor	r6, r11
	ld	r13, (rr2)
	xor	r6, r13
	and	r6, r12
	xor	r13, r6
	ld	(rr2), r13
	inc	r3, $2
	dec	r9, $1
	jp	z, t9
	ld	r12, rr14(18)
	dec	r9, $1
	jr	z, t2c
t2b:
	ldb	rh6, rl0
	ldb	rl6, rr4(1)
	ldb	rl0, rr4(2)
	ldb	rh7, rl0
	sdll	rr6, r8
	inc	r5, $2
	and	r6, r10
	xor	r6, r11
	ld	r13, (rr2)
	xor	r6, r13
	and	r6, r12
	xor	r13, r6
	ld	(rr2), r13
	inc	r3, $2
	djnz	r9, t2b
t2c:	ld	r12, rr14(20)
	ldb	rh6, rl0
	ldb	rl6, rr4(1)
	ldb	rl0, rr4(2)
	ldb	rh7, rl0
	sdll	rr6, r8
	inc	r5, $2
	and	r6, r10
	xor	r6, r11
	ld	r13, (rr2)
	xor	r6, r13
	and	r6, r12
	xor	r13, r6
	ld	(rr2), r13
	inc	r3, $2
	jp	t9
/ JAM1
t1a:
	xor	r6, r11
	and	r6, r12
	jr	z, t1s1
	ld	r13, (rr2)
	ld	r1, r13
	xor	r1, r10
	and	r1, r6
	xor	r13, r1
	ld	(rr2), r13
t1s1:	inc	r3, $2
	dec	r9, $1
	jp	z, t9
	ld	r12, rr14(18)
	dec	r9, $1
	jr	z, t1c
t1b:
	ldb	rh6, rl0
	ldb	rl6, rr4(1)
	ldb	rl0, rr4(2)
	ldb	rh7, rl0
	sdll	rr6, r8
	inc	r5, $2
	xor	r6, r11
	and	r6, r12
	jr	z, t1s2
	ld	r13, (rr2)
	ld	r1, r13
	xor	r1, r10
	and	r1, r6
	xor	r13, r1
	ld	(rr2), r13
t1s2:	inc	r3, $2
	djnz	r9, t1b
t1c:	ld	r12, rr14(20)
	ldb	rh6, rl0
	ldb	rl6, rr4(1)
	ldb	rl0, rr4(2)
	ldb	rh7, rl0
	sdll	rr6, r8
	inc	r5, $2
	xor	r6, r11
	and	r6, r12
	jr	z, t1s3
	ld	r13, (rr2)
	ld	r1, r13
	xor	r1, r10
	and	r1, r6
	xor	r13, r1
	ld	(rr2), r13
t1s3:	inc	r3, $2
	jp	t9
/ complement
tca:
	xor	r6, r11
	and	r6, r12
	jr	z, tcs1
	ld	r13, (rr2)
	xor	r13, r6
	ld	(rr2), r13
tcs1:	inc	r3, $2
	dec	r9, $1
	jp	z, t9
	ld	r12, rr14(18)
	dec	r9, $1
	jr	z, tcc
tcb:
	ldb	rh6, rl0
	ldb	rl6, rr4(1)
	ldb	rl0, rr4(2)
	ldb	rh7, rl0
	sdll	rr6, r8
	inc	r5, $2
	xor	r6, r11
	and	r6, r12
	jr	z, tcs2
	ld	r13, (rr2)
	xor	r13, r6
	ld	(rr2), r13
tcs2:	inc	r3, $2
	djnz	r9, tcb
tcc:	ld	r12, rr14(20)
	ldb	rh6, rl0
	ldb	rl6, rr4(1)
	ldb	rl0, rr4(2)
	ldb	rh7, rl0
	sdll	rr6, r8
	inc	r5, $2
	xor	r6, r11
	and	r6, r12
	jr	z, tcs3
	ld	r13, (rr2)
	xor	r13, r6
	ld	(rr2), r13
tcs3:	inc	r3, $2
	jp	t9
/ the next row
t9:	ldl	rr2, rr14(0)
	ld	r0, rr14(8)
	add	r3, r0
	ldl	rr14(0), rr2
	ldl	rr4, rr14(4)
	ld	r0, rr14(10)
	add	r5, r0
	ldl	rr14(4), rr4
	ld	r0, rr14(12)
	dec	r0, $1
	ld	rr14(12), r0
	jp	nz, t1
t10:	add	r15, $28
	ldm	r6, (rr14), $8
	add	r15, $16
	ret

/ gfx_CopyRows(d, s, n, rows, dstep, sstep, lm, rm, dir): n words of each
/ of rows rows from s to d, the first under lm and the last under rm (n
/ 1: lm only), rows dstep and sstep bytes apart.  Upwards, or downwards
/ from the last word when dir < 0, so an overlap on a row is safe.
gfx_CopyRows_:
	sub	r15, $16
	ldm	(rr14), r6, $8
	ld	r6, rr14(36)
	ld	r7, rr14(38)
	ld	r8, rr14(28)
	ld	r9, rr14(40)
	ld	r10, rr14(30)
	test	r10
	jr	z, 9f
/ r11: words between the ends; r12: offset of the last word
	ld	r11, r8
	cp	r6, $0xffff
	jr	eq, 1f
	dec	r11, $1
1:	cp	r7, $0xffff
	jr	eq, 1f
	dec	r11, $1
1:	ld	r12, r8
	dec	r12, $1
	add	r12, r12
2:	ldl	rr4, rr14(20)
	ldl	rr2, rr14(24)
	test	r9
	jr	mi, 5f
	cp	r6, $0xffff
	jr	eq, 3f
	ld	r0, (rr2)
	ld	r13, (rr4)
	xor	r0, r13
	and	r0, r6
	xor	r0, r13
	ld	(rr4), r0
	inc	r5, $2
	inc	r3, $2
3:	test	r11
	jr	z, 4f
	ld	r1, r11
	ldir	(rr4), (rr2), r1
4:	cp	r7, $0xffff
	jr	eq, 8f
	ld	r0, (rr2)
	ld	r13, (rr4)
	xor	r0, r13
	and	r0, r7
	xor	r0, r13
	ld	(rr4), r0
	jr	8f
5:	add	r5, r12
	add	r3, r12
	cp	r7, $0xffff
	jr	eq, 6f
	ld	r0, (rr2)
	ld	r13, (rr4)
	xor	r0, r13
	and	r0, r7
	xor	r0, r13
	ld	(rr4), r0
	dec	r5, $2
	dec	r3, $2
6:	test	r11
	jr	z, 7f
	ld	r1, r11
	lddr	(rr4), (rr2), r1
7:	cp	r6, $0xffff
	jr	eq, 8f
	ld	r0, (rr2)
	ld	r13, (rr4)
	xor	r0, r13
	and	r0, r6
	xor	r0, r13
	ld	(rr4), r0
8:	ldl	rr4, rr14(20)
	ld	r0, rr14(32)
	add	r5, r0
	ldl	rr14(20), rr4
	ldl	rr2, rr14(24)
	ld	r0, rr14(34)
	add	r3, r0
	ldl	rr14(24), rr2
	dec	r10, $1
	jr	nz, 2b
9:	ldm	r6, (rr14), $8
	add	r15, $16
	ret

/ gfx_ShiftRows(d, s, n, rows, dstep, sstep, sh, lm, rm, dir): as
/ gfx_CopyRows, but word k of a row is (s[k] << sh) | (s[k + 1] >>
/ (16 - sh)), sh 1-15, reading s[0] to s[n].  Downwards (dir < 0) each
/ word is read before the word to its left is written.  The pair of
/ source words shifts left by sh, or right by 16 - sh (r8 negative),
/ whichever is shorter.  Upwards sh may be -15 to -1: word k is then
/ (s[k - 1] << (16 + sh)) | (s[k] >> -sh), reading s[0] to s[n - 1],
/ with 0 for s[-1].
gfx_ShiftRows_:
	sub	r15, $16
	ldm	(rr14), r6, $8
	ld	r8, rr14(36)
	test	r8
	jr	pl, 1f
	add	r8, $16
1:	cp	r8, $8
	jr	ule, 1f
	sub	r8, $16
1:	ld	r9, rr14(42)
	ld	r10, rr14(30)
	test	r10
	jp	z, sr9
sr1:	ldl	rr4, rr14(20)
	ldl	rr2, rr14(24)
	ld	r11, rr14(28)
	test	r9
	jp	mi, srdn
/ up: the first word under lm, the middle, the last under rm
	ld	r6, rr14(38)
	ld	r7, rr14(40)
	clr	r12
	ld	r0, rr14(36)
	test	r0
	jr	mi, 4f
	ld	r12, (rr2)
	inc	r3, $2
4:	ld	r13, (rr2)
	cp	r11, $1
	jr	ne, 2f
	and	r6, r7
2:	calr	sredge
	inc	r5, $2
	dec	r11, $1
	jp	z, sr8
	dec	r11, $1
	jr	z, srupl
	test	r8
	jr	mi, srupr
srupf:	ld	r12, r13
	inc	r3, $2
	ld	r13, (rr2)
	ld	r0, r12
	ld	r1, r13
	sdll	rr0, r8
	ld	(rr4), r0
	inc	r5, $2
	djnz	r11, srupf
	jr	srupl
srupr:	ld	r12, r13
	inc	r3, $2
	ld	r13, (rr2)
	ld	r0, r12
	ld	r1, r13
	sdll	rr0, r8
	ld	(rr4), r1
	inc	r5, $2
	djnz	r11, srupr
srupl:	ld	r12, r13
	inc	r3, $2
	ld	r13, (rr2)
	ld	r6, r7
	calr	sredge
	jr	sr8
/ down from the last word: under rm, the middle, the first under lm
srdn:	ld	r6, rr14(40)
	ld	r7, rr14(38)
	ld	r0, r11
	add	r0, r0
	add	r3, r0
	dec	r0, $2
	add	r5, r0
	ld	r13, (rr2)
	dec	r3, $2
	ld	r12, (rr2)
	cp	r11, $1
	jr	ne, 3f
	and	r6, r7
3:	calr	sredge
	dec	r11, $1
	jr	z, sr8
	dec	r11, $1
	jr	z, srdnl
	test	r8
	jr	mi, srdnr
srdnf:	ld	r13, r12
	dec	r3, $2
	ld	r12, (rr2)
	ld	r0, r12
	ld	r1, r13
	sdll	rr0, r8
	dec	r5, $2
	ld	(rr4), r0
	djnz	r11, srdnf
	jr	srdnl
srdnr:	ld	r13, r12
	dec	r3, $2
	ld	r12, (rr2)
	ld	r0, r12
	ld	r1, r13
	sdll	rr0, r8
	dec	r5, $2
	ld	(rr4), r1
	djnz	r11, srdnr
srdnl:	ld	r13, r12
	dec	r3, $2
	ld	r12, (rr2)
	dec	r5, $2
	ld	r6, r7
	calr	sredge
sr8:	ldl	rr4, rr14(20)
	ld	r0, rr14(32)
	add	r5, r0
	ldl	rr14(20), rr4
	ldl	rr2, rr14(24)
	ld	r0, rr14(34)
	add	r3, r0
	ldl	rr14(24), rr2
	dec	r10, $1
	jp	nz, sr1
sr9:	ldm	r6, (rr14), $8
	add	r15, $16
	ret

/ the word of r12 and r13 shifted by r8 to rr4, under the mask r6
sredge:	ld	r0, r12
	ld	r1, r13
	sdll	rr0, r8
	test	r8
	jr	pl, 1f
	ld	r0, r1
1:	cp	r6, $0xffff
	jr	eq, 2f
	ld	r1, (rr4)
	xor	r0, r1
	and	r0, r6
	xor	r0, r1
2:	ld	(rr4), r0
	ret

/ gfx_LineRun(job, xstep, count): count pixels of a line, none past its
/ half of the plane.  The job's words go to r0-r11:
/  rl0 the pixel's bits	rh1 K	rr2 its byte	r4 the byte step of y
/  rl5 P1, rh5 P0	rl6 the bits after a byte step, rh6 skip0
/  r7 err	r8 incrE	r9 incrNE	r10 ptrn	r11 the bits' shift
/ then steep at 24; count is at least 1.  A pixel of pattern bit 1 becomes
/ d ^ (((d & K) ^ P1) & bits), of bit 0 the same with P0, or stays
/ with skip0.  Each step takes the major axis, and the minor one when
/ err is not negative; an x step shifts the bits by r11 and when they
/ run out reloads them and steps the byte by xstep (r12).
gfx_LineRun_:
	sub	r15, $16
	ldm	(rr14), r6, $8
	ldl	rr12, rr14(20)
	ldm	r0, (rr12), $12
	ld	r13, rr12(24)
	ld	r12, rr14(24)
	test	r13
	ld	r13, rr14(26)
	jp	nz, ln5
/ x major
ln1:	rl	r10, $1
	jr	nc, ln4
	ldb	rh0, (rr2)
	ldb	rl1, rh0
	andb	rl1, rh1
	xorb	rl1, rl5
	andb	rl1, rl0
	xorb	rh0, rl1
	ldb	(rr2), rh0
ln2:	test	r7
	jr	mi, ln3
	add	r7, r9
	add	r3, r4
	jr	ln31
ln3:	add	r7, r8
ln31:	sdlb	rl0, r11
	jr	nz, ln32
	ldb	rl0, rl6
	add	r3, r12
ln32:	djnz	r13, ln1
	jr	ln9
ln4:	testb	rh6
	jr	nz, ln2
	ldb	rh0, (rr2)
	ldb	rl1, rh0
	andb	rl1, rh1
	xorb	rl1, rh5
	andb	rl1, rl0
	xorb	rh0, rl1
	ldb	(rr2), rh0
	jr	ln2
/ y major
ln5:	rl	r10, $1
	jr	nc, ln8
	ldb	rh0, (rr2)
	ldb	rl1, rh0
	andb	rl1, rh1
	xorb	rl1, rl5
	andb	rl1, rl0
	xorb	rh0, rl1
	ldb	(rr2), rh0
ln6:	test	r7
	jr	mi, ln7
	add	r7, r9
	sdlb	rl0, r11
	jr	nz, ln71
	ldb	rl0, rl6
	add	r3, r12
	jr	ln71
ln7:	add	r7, r8
ln71:	add	r3, r4
	djnz	r13, ln5
	jr	ln9
ln8:	testb	rh6
	jr	nz, ln6
	ldb	rh0, (rr2)
	ldb	rl1, rh0
	andb	rl1, rh1
	xorb	rl1, rh5
	andb	rl1, rl0
	xorb	rh0, rl1
	ldb	(rr2), rh0
	jr	ln6
ln9:	ldm	r6, (rr14), $8
	add	r15, $16
	ret

/ gfx_WCopy(d, s, n): n words from s to d, upwards.
gfx_WCopy_:
	ldl	rr4, rr14(4)
	ldl	rr2, rr14(8)
	ld	r1, rr14(12)
	test	r1
	jr	z, 1f
	ldir	(rr4), (rr2), r1
1:	ret

/ gfx_WSwap(a, b, n): n words of a and b exchanged, 4 at a time.
gfx_WSwap_:
	sub	r15, $16
	ldm	(rr14), r6, $8
	ldl	rr2, rr14(20)
	ldl	rr4, rr14(24)
	ld	r1, rr14(28)
	ld	r0, r1
	and	r0, $3
	srl	r1, $2
	jr	z, 2f
1:	ldm	r6, (rr2), $4
	ldm	r10, (rr4), $4
	ldm	(rr2), r10, $4
	ldm	(rr4), r6, $4
	inc	r3, $8
	inc	r5, $8
	djnz	r1, 1b
2:	test	r0
	jr	z, 4f
3:	ld	r6, (rr2)
	ld	r7, (rr4)
	ld	(rr2), r7
	ld	(rr4), r6
	inc	r3, $2
	inc	r5, $2
	djnz	r0, 3b
4:	ldm	r6, (rr14), $8
	add	r15, $16
	ret

/ gfx_Bits(out, s, sh, n): n words of the bits of byte string s from bit
/ sh (0-7): bytes s[0] to s[2n], or to s[2n - 1] when sh is 0.
gfx_Bits_:
	ldl	rr4, rr14(4)
	ldl	rr2, rr14(8)
	ld	r0, rr14(12)
	ld	r1, rr14(14)
	test	r1
	jr	z, 9f
	test	r0
	jr	nz, 3f
	bit	r3, $0
	jr	nz, 2f
	ldir	(rr4), (rr2), r1
	ret
/ sh 0 from an odd byte
2:	ldb	rh0, (rr2)
	ldb	rl0, rr2(1)
	ld	(rr4), r0
	inc	r5, $2
	inc	r3, $2
	djnz	r1, 2b
	ret
/ rr6 = b0 b1 b2 -, shifted left by sh (r8); b2, in rl0, is the next
/ word's b0
3:	sub	r15, $6
	ld	rr14(0), r6
	ld	rr14(2), r7
	ld	rr14(4), r8
	ld	r8, r0
	ldb	rl0, (rr2)
4:	ldb	rh6, rl0
	ldb	rl6, rr2(1)
	ldb	rl0, rr2(2)
	ldb	rh7, rl0
	sdll	rr6, r8
	ld	(rr4), r6
	inc	r5, $2
	inc	r3, $2
	djnz	r1, 4b
	ld	r6, rr14(0)
	ld	r7, rr14(2)
	ld	r8, rr14(4)
	add	r15, $6
9:	ret

/ gfx_MulDiv(a, b, c, d, rem): (a * b + c) / d, a and b 0-32767, the sum
/ not negative, d 1-32767 and the quotient under 32768; the remainder to
/ rem unless it is NULL.  C would divide the long sum with DIVL.
gfx_MulDiv_:
	ld	r1, rr14(4)
	ld	r3, rr14(6)
	mult	rr0, r3
	ldl	rr2, rr14(8)
	addl	rr0, rr2
	ld	r3, rr14(12)
	div	rr0, r3
	ldl	rr2, rr14(14)
	testl	rr2
	jr	z, 1f
	ld	(rr2), r0
1:	ret

/ gfx_FastChars(job): whether each glyph of the text of a gfx_Glyphs
/ job is 8 wide from a byte, and with the kerning at 28 and the spacing
/ at 32 (NULL: none), unkerned and 8 apart.
gfx_FastChars_:
	sub	r15, $16
	ldm	(rr14), r6, $8
	ldl	rr12, rr14(20)
	ldl	rr8, rr12(4)
	ld	r7, rr12(16)
	ld	r6, rr12(24)
	ldl	rr10, rr12(12)
	test	r7
	jr	z, 8f
1:	ldb	rl0, (rr8)
	inc	r9, $1
	cpb	rl0, rh6
	jr	ult, 2f
	cpb	rl0, rl6
	jr	ugt, 2f
	subb	rl0, rh6
	clrb	rh0
	ld	r1, r0
	jr	3f
2:	ld	r1, rr12(26)
3:	add	r1, r1
	ld	r2, r1
	add	r2, r2
	ld	r0, rr10(r2)
	and	r0, $7
	jr	nz, 7f
	inc	r2, $2
	ld	r0, rr10(r2)
	cp	r0, $8
	jr	ne, 7f
	ldl	rr2, rr12(28)
	testl	rr2
	jr	z, 4f
	ld	r0, rr2(r1)
	test	r0
	jr	nz, 7f
4:	ldl	rr2, rr12(32)
	testl	rr2
	jr	z, 5f
	ld	r0, rr2(r1)
	cp	r0, $8
	jr	ne, 7f
5:	djnz	r7, 1b
8:	ld	r1, $1
	jr	9f
7:	clr	r1
9:	ldm	r6, (rr14), $8
	add	r15, $16
	ret

/ gfx_Glyphs(job): the template of a fixed 8-pixel font, a glyph a byte
/ column (text.c).  The job's words: 0 the template, 4 the text, 8 the
/ font data, 12 its locations, 16 the count, 18 bpr, 20 rows, 22 modulo,
/ 24 lo << 8 | hi, 26 the glyph of a character outside them.
gfx_Glyphs_:
	sub	r15, $16
	ldm	(rr14), r6, $8
	ldl	rr12, rr14(20)
	ldl	rr10, rr12(0)
	ldl	rr8, rr12(4)
	ld	r7, rr12(16)
	test	r7
	jr	z, 9f
/ the glyph's index, times 4
1:	ldl	rr12, rr14(20)
	ld	r6, rr12(24)
	ldb	rl0, (rr8)
	inc	r9, $1
	cpb	rl0, rh6
	jr	ult, 2f
	cpb	rl0, rl6
	jr	ugt, 2f
	subb	rl0, rh6
	clrb	rh0
	ld	r1, r0
	jr	3f
2:	ld	r1, rr12(26)
3:	sll	r1, $2
/ its bits from bit 0 of a byte: rr2, down the rows to rr4
	ldl	rr2, rr12(12)
	ld	r0, rr2(r1)
	srl	r0, $3
	ldl	rr2, rr12(8)
	add	r3, r0
	ldl	rr4, rr10
	ld	r1, rr12(20)
	ld	r6, rr12(18)
	ld	r12, rr12(22)
4:	ldb	rl0, (rr2)
	ldb	(rr4), rl0
	add	r3, r12
	add	r5, r6
	djnz	r1, 4b
	inc	r11, $1
	djnz	r7, 1b
9:	ldm	r6, (rr14), $8
	add	r15, $16
	ret
