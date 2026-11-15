/* SPDX-License-Identifier: MIT */
/* The C library string functions icon.library uses */

#include "icon_intern.h"

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

APTR memset(APTR d, int c, ULONG n)
{
    UBYTE *p = d;

    while (n--)
        *p++ = c;
    return d;
}
