/*
 * AROS library function definitions as plain C: the base is the first
 * argument, the others follow on the stack, and the function is named
 * after its library and vector, Library_N (vector N is at -6N), which
 * the library's fd file names after `@'.  The I (inline) forms are the
 * same.
 */
#ifndef AROS_LIBCALL_H
#define AROS_LIBCALL_H

#include <aros/types.h>

#define AROS_LHA(type, name, reg)	type name

#define AROS_LH0(t, n, bt, bn, o, s) \
	t s##_##o(bt bn)
#define AROS_LH1(t, n, a1, bt, bn, o, s) \
	t s##_##o(bt bn, a1)
#define AROS_LH2(t, n, a1, a2, bt, bn, o, s) \
	t s##_##o(bt bn, a1, a2)
#define AROS_LH3(t, n, a1, a2, a3, bt, bn, o, s) \
	t s##_##o(bt bn, a1, a2, a3)
#define AROS_LH4(t, n, a1, a2, a3, a4, bt, bn, o, s) \
	t s##_##o(bt bn, a1, a2, a3, a4)

#define AROS_LH5(t, n, a1, a2, a3, a4, a5, bt, bn, o, s) \
	t s##_##o(bt bn, a1, a2, a3, a4, a5)
#define AROS_LH6(t, n, a1, a2, a3, a4, a5, a6, bt, bn, o, s) \
	t s##_##o(bt bn, a1, a2, a3, a4, a5, a6)
#define AROS_LH7(t, n, a1, a2, a3, a4, a5, a6, a7, bt, bn, o, s) \
	t s##_##o(bt bn, a1, a2, a3, a4, a5, a6, a7)
#define AROS_LH8(t, n, a1, a2, a3, a4, a5, a6, a7, a8, bt, bn, o, s) \
	t s##_##o(bt bn, a1, a2, a3, a4, a5, a6, a7, a8)
#define AROS_LH9(t, n, a1, a2, a3, a4, a5, a6, a7, a8, a9, bt, bn, o, s) \
	t s##_##o(bt bn, a1, a2, a3, a4, a5, a6, a7, a8, a9)
#define AROS_LH10(t, n, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, bt, bn, o, s) \
	t s##_##o(bt bn, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10)
#define AROS_LH11(t, n, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, bt, bn, o, s) \
	t s##_##o(bt bn, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11)

#define AROS_LH1I	AROS_LH1
#define AROS_LH2I	AROS_LH2
#define AROS_LH4I	AROS_LH4

#define AROS_LIBFUNC_INIT
#define AROS_LIBFUNC_EXIT

#endif
