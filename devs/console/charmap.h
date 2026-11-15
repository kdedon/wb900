/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.
*/

#ifndef CHARMAP_H
#define CHARMAP_H

#include <exec/types.h>

// FIXME:
// This wastes a lot of memory - most characters are likely to share the
// same fg/bg/flags. One option would be to optimize away the fgpen/bgpen/
// flags buffers if they're the same for every character in the line
/* Number of per-cell arrays of a line: text, fgpen, bgpen and flags */
#define CHARMAP_PLANES 4

struct charmap_line
{
    // FIXME: Replace with a MinNode?
    struct charmap_line *next;
    struct charmap_line *prev;

    UWORD size;
    UWORD capacity;

    /* The per-cell arrays. They share one allocation of capacity bytes
       each, in this order, and CM_PLANE() takes them by number. */
    char *text;
    BYTE *fgpen;
    BYTE *bgpen;
    BYTE *flags;
};

/* Per-cell array i of a line: text, fgpen, bgpen, flags */
#define CM_PLANE(line, i) (((UBYTE **)&(line)->text)[i])

VOID charmap_dispose_lines(struct charmap_line *line);
struct charmap_line *charmap_dispose_line(struct charmap_line *line);
struct charmap_line *charmap_newline(struct charmap_line *next,
    struct charmap_line *prev);
VOID charmap_resize(struct ConsoleBase *ConsoleDevice, struct charmap_line *line, ULONG newsize);

#endif
