/* SPDX-License-Identifier: MIT */
/* The C library string functions workbench.library uses */

#include "workbench_intern.h"

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
