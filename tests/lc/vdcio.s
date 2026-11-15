/ SPDX-License-Identifier: MIT
/ The port accesses of lc.gfxdriver's port layer as calls into the 8568
/ model (vdcmodel.c).  Each keeps every register but the one its
/ instruction writes, and the flags but those outib sets, and passes
/ the FCW at the access as the last argument:
/	inb rl0, @r2		lcm_inb		rl0 = lcm_status(fcw)
/	outb @r2, rl1		lcm_outa	lcm_select(rl1, fcw)
/	outb @r3, rl1		lcm_outd	lcm_data(rl1, fcw)
/	outib @r3, @rr4, r1	lcm_outi	lcm_data(*rr4++, fcw), r1--, V if 0

	.globl	lcm_inb_, lcm_outa_, lcm_outd_, lcm_outi_
	.globl	lcm_status_, lcm_select_, lcm_data_

	.shri

lcm_inb_:
	push	(rr14), r1
	push	(rr14), r2
	push	(rr14), r3
	push	(rr14), r4
	push	(rr14), r5
	push	(rr14), r6
	ldctl	r6, FCW
	push	(rr14), r6
	call	lcm_status_
	inc	r15, $2
	ldctl	FCW, r6
	ldb	rl0, rl1
	pop	r6, (rr14)
	pop	r5, (rr14)
	pop	r4, (rr14)
	pop	r3, (rr14)
	pop	r2, (rr14)
	pop	r1, (rr14)
	ret

/ rl1 to lcm_select or, with Z set, lcm_data
out:
	push	(rr14), r0
	push	(rr14), r1
	push	(rr14), r2
	push	(rr14), r3
	push	(rr14), r4
	push	(rr14), r5
	ldctl	r0, FCW
	push	(rr14), r0
	clrb	rh1
	push	(rr14), r1
	jr	z, 1f
	call	lcm_select_
	jr	2f
1:	call	lcm_data_
2:	add	r15, $4
	pop	r5, (rr14)
	pop	r4, (rr14)
	pop	r3, (rr14)
	pop	r2, (rr14)
	pop	r1, (rr14)
	pop	r0, (rr14)
	ret

lcm_outa_:
	push	(rr14), r0
	ldctl	r0, FCW
	resflg	Z
	calr	out
	ldctl	FCW, r0
	pop	r0, (rr14)
	ret

lcm_outd_:
	push	(rr14), r0
	ldctl	r0, FCW
	setflg	Z
	calr	out
	ldctl	FCW, r0
	pop	r0, (rr14)
	ret

lcm_outi_:
	push	(rr14), r1
	ldb	rl1, @rr4
	setflg	Z
	calr	out
	pop	r1, (rr14)
	inc	r5, $1
	dec	r1, $1
	test	r1
	resflg	V
	jr	nz, 1f
	setflg	V
1:	ret
