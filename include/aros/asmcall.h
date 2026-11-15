/*
 * AROS's user function macros as plain C: the arguments are passed on
 * the stack in order, so a hook, an input handler or a BOOPSI dispatcher
 * is an ordinary C function.
 */
#ifndef AROS_ASMCALL_H
#define AROS_ASMCALL_H

#include <aros/types.h>

#define AROS_UFHA(type, name, reg)	type name
#define AROS_UFPA(type, name, reg)	type name
#define AROS_UFCA(type, value, reg)	(type)(value)

#define AROS_UFH2(t, n, a1, a2)		t n(a1, a2)
#define AROS_UFH3(t, n, a1, a2, a3)	t n(a1, a2, a3)
#define AROS_UFH4(t, n, a1, a2, a3, a4)	t n(a1, a2, a3, a4)
#define AROS_UFP2(t, n, a1, a2)		t n(a1, a2)
#define AROS_UFP3(t, n, a1, a2, a3)	t n(a1, a2, a3)
#define AROS_UFP4(t, n, a1, a2, a3, a4)	t n(a1, a2, a3, a4)
#define AROS_UFC3(t, n, a1, a2, a3)	(((t (*)())(n))(a1, a2, a3))
#define AROS_UFC4(t, n, a1, a2, a3, a4)	(((t (*)())(n))(a1, a2, a3, a4))

#define AROS_USERFUNC_INIT
#define AROS_USERFUNC_EXIT

#endif
