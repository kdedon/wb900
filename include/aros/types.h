/*
 * The types and macros of AROS's <exec/types.h> and <aros/cpu.h> that
 * Exec900's headers lack.
 */
#ifndef AROS_TYPES_H
#define AROS_TYPES_H

#include <exec/types.h>

typedef ULONG IPTR;		/* an integer that holds a pointer */
typedef APTR RAWARG;		/* RawDoFmt's data stream */
typedef ULONG STACKULONG;	/* a ULONG passed on the stack */
typedef IPTR STACKIPTR;		/* an IPTR passed on the stack */

/* BOOPSI messages keep the classic packed layout. */
#define STACKED

/* The Z8000 reads a long at any even address. */
#define AROS_LONGALIGN	2

#endif
