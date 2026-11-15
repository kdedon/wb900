/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: The C library string functions commodities uses
*/

#include "cxintern.h"

ULONG strlen(CONST_STRPTR s)
{
    CONST_STRPTR p = s;

    while (*p)
        p++;
    return (ULONG)(p - s);
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
