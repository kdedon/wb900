/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: The C library string and memory functions the handler uses
*/

#include <proto/exec.h>

#include "con_handler_intern.h"

ULONG strlen(CONST_STRPTR s)
{
    CONST_STRPTR p = s;

    while (*p)
        p++;
    return (ULONG)(p - s);
}

int strcmp(CONST_STRPTR a, CONST_STRPTR b)
{
    while (*a && *a == *b)
    {
        a++;
        b++;
    }
    return (int)(UBYTE)*a - (int)(UBYTE)*b;
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

char *strcpy(char *d, CONST_STRPTR s)
{
    char *p = d;

    while ((*p++ = *s++) != 0)
        ;
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
