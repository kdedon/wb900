/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: The C library string and memory functions console.device uses
*/

#include "console_gcc.h"

ULONG strlen(CONST_STRPTR s)
{
    CONST_STRPTR p = s;

    while (*p)
        p++;
    return (ULONG)(p - s);
}

int strncmp(CONST_STRPTR a, CONST_STRPTR b, ULONG n)
{
    for (; n; n--, a++, b++)
    {
        if (*a != *b || !*a)
            return (int)(UBYTE)*a - (int)(UBYTE)*b;
    }
    return 0;
}

APTR memcpy(APTR d, CONST_APTR s, ULONG n)
{
    CopyMem((APTR)s, d, n);
    return d;
}

APTR memmove(APTR d, CONST_APTR s, ULONG n)
{
    UBYTE *dp = d;
    CONST UBYTE *sp = s;

    if (dp <= sp)
        while (n--)
            *dp++ = *sp++;
    else
    {
        dp += n;
        sp += n;
        while (n--)
            *--dp = *--sp;
    }
    return d;
}

APTR memset(APTR d, int c, ULONG n)
{
    UBYTE *p = d;

    while (n--)
        *p++ = c;
    return d;
}
