/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: The C library functions More uses
*/

#include <exec/types.h>
#include <proto/exec.h>

#include "global.h"

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

char *strchr(CONST_STRPTR s, int c)
{
    for (; *s; s++)
        if (*s == (char)c)
            return (char *)s;
    return NULL;
}

static void putch(UBYTE c, STRPTR *p)
{
    *(*p)++ = c;
}

/* RawDoFmt's conversions: %d takes a word, %ld a long */
void sprintf(char *buf, CONST_STRPTR fmt, ...)
{
    STRPTR p = buf;

    RawDoFmt(fmt, (APTR)(&fmt + 1), (APTR)putch, &p);
}
