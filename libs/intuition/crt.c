/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: The C library string and memory functions intuition uses
*/

#include "intuition_intern.h"

ULONG strlen(CONST_STRPTR s)
{
    CONST_STRPTR p = s;

    while (*p)
        p++;
    return (ULONG)(p - s);
}

char *strcpy(char *d, CONST_STRPTR s)
{
    char *p = d;

    while ((*p++ = *s++) != 0)
        ;
    return d;
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

int strcasecmp(CONST_STRPTR a, CONST_STRPTR b)
{
    UBYTE ca, cb;

    do
    {
        ca = *a++;
        cb = *b++;
        if (ca >= 'A' && ca <= 'Z')
            ca += 'a' - 'A';
        if (cb >= 'A' && cb <= 'Z')
            cb += 'a' - 'A';
    } while (ca && ca == cb);
    return (int)ca - (int)cb;
}

int strncmp(CONST_STRPTR a, CONST_STRPTR b, ULONG n)
{
    for (; n; n--, a++, b++)
    {
        if (*a != *b)
            return (int)(UBYTE)*a - (int)(UBYTE)*b;
        if (!*a)
            break;
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
    const UBYTE *sp = s;

    if (dp < sp)
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

int memcmp(CONST_APTR a, CONST_APTR b, ULONG n)
{
    const UBYTE *p = a, *q = b;

    for (; n; n--, p++, q++)
        if (*p != *q)
            return (int)*p - (int)*q;
    return 0;
}

int abs(int i)
{
    return i < 0 ? -i : i;
}

LONG atol(CONST_STRPTR s)
{
    LONG v = 0;
    BOOL neg = FALSE;

    while (*s == ' ' || *s == '\t')
        s++;
    if (*s == '-' || *s == '+')
        neg = (*s++ == '-');
    while (*s >= '0' && *s <= '9')
        v = v * 10 + (*s++ - '0');
    return neg ? -v : v;
}
