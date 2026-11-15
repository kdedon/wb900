/ SPDX-License-Identifier: MIT
/ The word loops of graphics.library's row code (gfxformat.c).  A run
/ never crosses a 64 KB segment: the callers split rows there, so only
/ the offset words step.  r6-r13 are the caller's and are saved.

	.globl	gfx_FillRows_, gfx_FillRect_, gfx_TmplRows_, gfx_CopyRows_, gfx_ShiftRows_
	.globl	gfx_LineRun_, gfx_WCopy_, gfx_Bits_

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

/ gfx_TmplRows(job): rows of a template, job (gfxformat.c) the words
/  0 d (long)	4 dstep	6 rows	8 src (long)	12 srcmod
/  14 tb	16 n	18 f	20 b	22 m	24 lm	26 rm
/  28 mode	30 nib (long)
/ d, rows and src step through the rows.  Each row's template words
/ are the bits from bit tb (-15 on) of src, read up to the byte after
/ the last: with nib NULL (1 bit a pixel) one a word of d, else 16 a
/ group of 4 words, each 4 bits looked up in nib.  Then n words of d,
/ the first under lm and the last under rm, from the template words t
/ under the mask m by mode: 0 JAM2, ((f & t) ^ b) (f is APen ^ BPen);
/ 1 JAM1, f where t; 4 complement where t; with 2 added, where not t;
/ 8, chunky with m all 1s, the words nib holds for the 4 bits.
/ The frame: 0 the template words, 64 the expanded words, 128 job.
gfx_TmplRows_:
	sub	r15, $16
	ldm	(rr14), r6, $8
	ldl	rr12, rr14(20)
	sub	r15, $132
	ldl	rr14(128), rr12
tr1:	ldl	rr12, rr14(128)
	ld	r0, rr12(6)
	test	r0
	jp	z, tr9
	dec	r0, $1
	ld	rr12(6), r0
/ the template words: r1 of them from tb (r9) of src (rr4) to rr2
	ld	r1, rr12(16)
	ldl	rr6, rr12(30)
	test	r6
	jr	z, tr2
	add	r1, $3
	srl	r1, $2
tr2:	ld	r9, rr12(14)
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
/ chunky: each template word to 4 words of masks through nib (rr6), or
/ in mode 8 straight to the words of d: nib holds them
tr5:	ldl	rr12, rr14(128)
	ldl	rr6, rr12(30)
	ldl	rr4, rr14
	test	r6
	jr	z, tr7
	ld	r1, rr12(16)
	ld	r0, rr12(28)
	cp	r0, $8
	jr	eq, trd
	ldl	rr2, rr14
	add	r3, $64
tr6:	ld	r8, (rr4)
	inc	r5, $2
	ld	r10, $4
tr61:	ld	r9, r8
	srl	r9, $12
	add	r9, r9
	ld	r0, rr6(r9)
	ld	(rr2), r0
	inc	r3, $2
	sll	r8, $4
	dec	r1, $1
	jr	z, tr62
	djnz	r10, tr61
	jr	tr6
tr62:	ldl	rr4, rr14
	add	r5, $64
	jr	tr7
/ mode 8: r12 this word's mask, lm then all; r13 rm
trd:	ldl	rr2, rr12(0)
	ld	r11, rr12(24)
	ld	r13, rr12(26)
	ld	r12, r11
trd1:	ld	r8, (rr4)
	inc	r5, $2
	ld	r10, $4
trd2:	ld	r9, r8
	srl	r9, $12
	add	r9, r9
	ld	r0, rr6(r9)
	cp	r1, $1
	jr	ne, trd3
	and	r12, r13
trd3:	cp	r12, $0xffff
	jr	eq, trd4
	ld	r11, (rr2)
	xor	r0, r11
	and	r0, r12
	xor	r0, r11
	ld	r12, $0xffff
trd4:	ld	(rr2), r0
	inc	r3, $2
	sll	r8, $4
	dec	r1, $1
	jr	z, tr8
	djnz	r10, trd2
	jr	trd1
/ the words of d from the template words at rr4
tr7:	ldl	rr2, rr12(0)
	ld	r1, rr12(16)
	ld	r6, rr12(18)
	ld	r7, rr12(20)
	ld	r8, rr12(22)
	ld	r9, rr12(24)
	ld	r10, rr12(26)
	ld	r12, rr12(28)
	and	r9, r8
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
tr9:	add	r15, $132
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
/ whichever is shorter.
gfx_ShiftRows_:
	sub	r15, $16
	ldm	(rr14), r6, $8
	ld	r8, rr14(36)
	cp	r8, $8
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
	ld	r12, (rr2)
	inc	r3, $2
	ld	r13, (rr2)
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

/ gfx_LineRun(job): the pixels of a line in one plane, job (gfxformat.c)
/  0 the pixel masks of x & 7 (bytes)	8 lo	12 hi	16 yo (long)
/  20 x	22 y	24 d	26 incrE	28 incrNE	30 count
/  32 sx	34 sy	36 ystep (long)	40 steep	42 ptrn
/  44 cx0	46 cx1 - cx0	48 cy0	50 cy1 - cy0	52 f	54 b
/  56 mode	58 -(log2 pixels a byte)
/ yo is the row's offset plus lo's: x's byte is in lo below 0x10000 and
/ in hi from there.  ptrn bit 15 goes with the first pixel; 1 is f, 0 b
/ in mode 0 (JAM2) and nothing in 1 (JAM1); mode 2 complements on 1.
gfx_LineRun_:
	sub	r15, $16
	ldm	(rr14), r6, $8
	ldl	rr4, rr14(20)
	ldl	rr6, rr4(16)
	ld	r8, rr4(20)
	ld	r9, rr4(22)
	ld	r10, rr4(24)
	ld	r12, rr4(26)
	ld	r13, rr4(28)
	ld	r1, rr4(30)
	ld	r11, rr4(42)
	test	r1
	jp	z, ln9
ln1:	rl	r11, $1
	jr	c, ln2
	ld	r0, rr4(56)
	test	r0
	jr	nz, ln8
	ld	r0, rr4(54)
	jr	ln3
ln2:	ld	r0, rr4(52)
/ inside the clip
ln3:	ld	r2, r8
	ld	r3, rr4(44)
	sub	r2, r3
	ld	r3, rr4(46)
	cp	r2, r3
	jr	ugt, ln8
	ld	r2, r9
	ld	r3, rr4(48)
	sub	r2, r3
	ld	r3, rr4(50)
	cp	r2, r3
	jr	ugt, ln8
/ rr2 = yo + x's byte, in lo or hi
	push	(rr14), r1
	push	(rr14), r13
	ld	r3, r8
	ld	r13, rr4(58)
	sdl	r3, r13
	clr	r2
	addl	rr2, rr6
	test	r2
	jr	nz, ln4
	ld	r2, rr4(8)
	jr	ln5
ln4:	ld	r2, rr4(12)
ln5:	ld	r13, r8
	and	r13, $7
	ldb	rh0, rr4(r13)
	ld	r13, rr4(56)
	ldb	rl1, (rr2)
	cp	r13, $2
	jr	ne, ln6
	xorb	rl1, rh0
	jr	ln7
/ d ^ ((d ^ pen) & m)
ln6:	ldb	rh1, rl1
	xorb	rh1, rl0
	andb	rh1, rh0
	xorb	rl1, rh1
ln7:	ldb	(rr2), rl1
	pop	r13, (rr14)
	pop	r1, (rr14)
/ the next pixel
ln8:	test	r10
	jr	mi, ln10
	ld	r0, rr4(32)
	add	r8, r0
	ld	r0, rr4(34)
	add	r9, r0
	ldl	rr2, rr4(36)
	addl	rr6, rr2
	add	r10, r13
	jr	ln12
ln10:	add	r10, r12
	ld	r0, rr4(40)
	test	r0
	jr	nz, ln11
	ld	r0, rr4(32)
	add	r8, r0
	jr	ln12
ln11:	ld	r0, rr4(34)
	add	r9, r0
	ldl	rr2, rr4(36)
	addl	rr6, rr2
ln12:	dec	r1, $1
	jp	nz, ln1
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
