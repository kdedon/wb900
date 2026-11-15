/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: The C library string and memory functions gadtools uses
*/

#include "gadtools_intern.h"

ULONG strlen(CONST_STRPTR s)
{
    CONST_STRPTR p = s;

    while (*p)
        p++;
    return (ULONG)(p - s);
}

STRPTR strchr(CONST_STRPTR s, int c)
{
    for (;; s++)
    {
        if (*s == (UBYTE)c)
            return (STRPTR)s;
        if (!*s)
            return NULL;
    }
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

char *strcpy(char *d, CONST_STRPTR s)
{
    char *p = d;

    while ((*p++ = *s++) != 0)
        ;
    return d;
}

char *strncpy(char *d, CONST_STRPTR s, ULONG n)
{
    char *p = d;

    for (; n && *s; n--)
        *p++ = *s++;
    for (; n; n--)
        *p++ = 0;
    return d;
}

APTR memcpy(APTR d, CONST_APTR s, ULONG n)
{
    CopyMem((APTR)s, d, n);
    return d;
}

APTR memset(APTR d, int c, ULONG n)
{
    UBYTE *p = d;

    while (n--)
        *p++ = c;
    return d;
}

int abs(int i)
{
    return i < 0 ? -i : i;
}
