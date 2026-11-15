/*
 * AROS's atomic counter updates: one CPU, so Forbid() keeps other tasks
 * out; no interrupt code touches these counters.
 */
#ifndef AROS_ATOMIC_H
#define AROS_ATOMIC_H

#include <exec/exec.h>

#define AROS_ATOMIC_INC(var)	do { Forbid(); (var)++; Permit(); } while (0)
#define AROS_ATOMIC_DEC(var)	do { Forbid(); (var)--; Permit(); } while (0)
#define AROS_ATOMIC_OR(var, mask)	do { Forbid(); (var) |= (mask); Permit(); } while (0)
#define AROS_ATOMIC_AND(var, mask)	do { Forbid(); (var) &= (mask); Permit(); } while (0)

#endif
