/*
    Copyright (C) 1995-2020, The AROS Development Team. All rights reserved.

    Desc: Replacements for some C runtime functions
*/

#include <aros/crt_replacement.h>

char *Strcpy(char *dest, const char *src)
{
    char *ptr = dest;

    while ((*ptr = *src))
    {
        ptr ++;
        src ++;
    }

    return dest;
}

ULONG Strlen(CONST_STRPTR string)
{
    CONST_STRPTR str_start = (CONST_STRPTR)string;

    while (*string++);

    return (ULONG)(((IPTR)string) - ((IPTR)str_start)) - 1;
}
