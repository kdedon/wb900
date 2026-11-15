/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: The C library functions asl uses
*/

#include "asl_intern.h"

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

char *strncpy(char *d, CONST_STRPTR s, ULONG n)
{
    char *p = d;

    for (; n && *s; n--)
        *p++ = *s++;
    for (; n; n--)
        *p++ = 0;
    return d;
}

ULONG strlcpy(char *d, CONST_STRPTR s, ULONG n)
{
    ULONG len = strlen(s);

    if (n)
    {
        if (n > len + 1)
            n = len + 1;
        CopyMem((APTR)s, d, n - 1);
        d[n - 1] = 0;
    }
    return len;
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

STRPTR strstr(CONST_STRPTR s, CONST_STRPTR w)
{
    ULONG n = strlen(w);

    for (; *s; s++)
    {
        ULONG i;

        for (i = 0; i < n && s[i] == w[i]; i++)
            ;
        if (i == n)
            return (STRPTR)s;
    }
    return n ? NULL : (STRPTR)s;
}

APTR memcpy(APTR d, CONST_APTR s, ULONG n)
{
    CopyMem((APTR)s, d, n);
    return d;
}

/* Overlapping blocks: copy from the end when the target is above */
APTR memmove(APTR d, CONST_APTR s, ULONG n)
{
    UBYTE *dp = d;
    const UBYTE *sp = s;

    if (dp > sp)
    {
        dp += n;
        sp += n;
        while (n--)
            *--dp = *--sp;
    }
    else
    {
        while (n--)
            *dp++ = *sp++;
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

static void putch(UBYTE c, STRPTR *p)
{
    *(*p)++ = c;
}

/* RawDoFmt's conversions: %d takes a word */
int sprintf(char *buf, CONST_STRPTR fmt, ...)
{
    STRPTR p = buf;

    RawDoFmt(fmt, (RAWARG)(&fmt + 1), (APTR)putch, &p);
    return (int)(p - buf - 1);
}

/* Shell sort; the lists are short */
void qsort(APTR base, ULONG n, ULONG size, int (*cmp)(CONST_APTR, CONST_APTR))
{
    UBYTE *b = base, *x, *y, t;
    ULONG gap, i, j, k;

    for (gap = n / 2; gap > 0; gap /= 2)
        for (i = gap; i < n; i++)
            for (j = i; j >= gap; j -= gap)
            {
                x = b + (j - gap) * size;
                y = b + j * size;
                if (cmp(x, y) <= 0)
                    break;
                for (k = 0; k < size; k++)
                {
                    t = x[k];
                    x[k] = y[k];
                    y[k] = t;
                }
            }
}
