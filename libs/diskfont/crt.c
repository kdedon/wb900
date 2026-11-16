/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: The C library functions diskfont uses
*/

#include "diskfont_intern.h"

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

char *strcat(char *d, CONST_STRPTR s)
{
    strcpy(d + strlen(d), s);
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

STRPTR strrchr(CONST_STRPTR s, int c)
{
    CONST_STRPTR r = NULL;

    for (;; s++)
    {
        if (*s == (UBYTE)c)
            r = s;
        if (!*s)
            return (STRPTR)r;
    }
}
