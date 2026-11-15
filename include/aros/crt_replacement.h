/*
    Copyright (C) 1995-2020, The AROS Development Team. All rights reserved.

    Desc: Replacements for some C runtime functions
*/

#ifndef CRT_REPLACEMENT_H
#define CRT_REPLACEMENT_H

#include <aros/types.h>

/* string.h replacments */

char *Strcpy(char *dest, const char *src);
ULONG Strlen(CONST_STRPTR string);

#endif
