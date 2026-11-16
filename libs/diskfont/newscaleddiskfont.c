/*
    Copyright (C) 1995-2007, The AROS Development Team. All rights reserved.

    Desc: Diskfont function NewScaledDiskFont()
*/
#include "diskfont_intern.h"

/* n source units at dst/src, rounded */
#define SCALE(n, dst, src)  ((WORD)(((LONG)(n) * (dst) + (LONG)(src) / 2) / (src)))

/*****************************************************************************

    NAME */
#include <proto/diskfont.h>

        AROS_LH2(struct DiskFontHeader *, NewScaledDiskFont,

/*  SYNOPSIS */
        AROS_LHA(struct TextFont *, sourceFont, A0),
        AROS_LHA(struct TextAttr *, destTextAttr, A1),

/*  LOCATION */
        struct Library *, DiskfontBase, 9, Diskfont)

/*  FUNCTION
        Make a copy of a font scaled to another height; the width
        scales by the same factor.

    INPUTS
        sourceFont   - the font to scale
        destTextAttr - its ta_YSize is the new height

    RESULT
        A DiskFontHeader whose dfh_TF is the new font, not yet in the
        font list, and whose dfh_Segment UnLoadSeg() frees; or NULL.

    NOTES
        Nearest-neighbour scaling. The font is neither a ROM nor a disk
        font, nor designed.

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

    HISTORY
        27-11-96    digulla automatically created from
                            diskfont_lib.fd and clib/diskfont_protos.h

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct DiskFontHeader *dfh;
    struct TextFont       *tf;
    ULONG                 *srcloc = sourceFont->tf_CharLoc, *loc;
    WORD                  *srcspace = sourceFont->tf_CharSpace;
    WORD                  *srckern = sourceFont->tf_CharKern;
    WORD                  *space, *kern;
    UBYTE                 *src = sourceFont->tf_CharData, *data, *d;
    UWORD                  sy = sourceFont->tf_YSize, dy = destTextAttr->ta_YSize;
    UWORD                  numchars, i, x, y, w, nw, pos, modulo;
    UBYTE                  rows[255];
    ULONG                  width, size;

    if (sy == 0 || dy == 0 || dy > 255)
        return NULL;

    numchars = sourceFont->tf_HiChar - sourceFont->tf_LoChar + 2;

    /* The glyphs' new widths, side by side */
    for (width = 0, i = 0; i < numchars; i++)
        width += SCALE(srcloc[i] & 0xFFFF, dy, sy);
    modulo = (UWORD)((width + 15) / 16 * 2);
    if (width > 0xFFF0 || (ULONG)modulo * dy > 0xF000)
        return NULL;

    size = sizeof(struct DiskFontHeader) + numchars * sizeof(ULONG) +
           (srcspace ? numchars * sizeof(WORD) : 0) +
           (srckern ? numchars * sizeof(WORD) : 0) + (ULONG)modulo * dy;
    dfh = AllocSegment(NULL, size, MEMF_ANY | MEMF_CLEAR, DFB(DiskfontBase));
    if (dfh == NULL)
        return NULL;

    loc = (ULONG *)(dfh + 1);
    space = (WORD *)(loc + numchars);
    kern = space + (srcspace ? numchars : 0);
    data = (UBYTE *)(kern + (srckern ? numchars : 0));

    dfh->dfh_DF.ln_Name = dfh->dfh_Name;
    dfh->dfh_FileID = DFH_ID;
    dfh->dfh_Segment = (BPTR)((UBYTE *)dfh - sizeof(BPTR));
    for (i = 0; i < MAXFONTNAME - 1 && sourceFont->tf_Message.mn_Node.ln_Name[i]; i++)
        dfh->dfh_Name[i] = sourceFont->tf_Message.mn_Node.ln_Name[i];

    tf = &dfh->dfh_TF;
    tf->tf_Message.mn_Node.ln_Type = NT_FONT;
    tf->tf_Message.mn_Node.ln_Name = dfh->dfh_Name;
    tf->tf_YSize = dy;
    tf->tf_Style = sourceFont->tf_Style & ~FSF_TAGGED;
    tf->tf_Flags = sourceFont->tf_Flags &
                   ~(FPF_ROMFONT | FPF_DISKFONT | FPF_DESIGNED | FPF_REMOVED);
    tf->tf_XSize = SCALE(sourceFont->tf_XSize, dy, sy);
    tf->tf_Baseline = SCALE(sourceFont->tf_Baseline + 1, dy, sy) - 1;
    tf->tf_BoldSmear = sourceFont->tf_BoldSmear;
    tf->tf_LoChar = sourceFont->tf_LoChar;
    tf->tf_HiChar = sourceFont->tf_HiChar;
    tf->tf_CharData = data;
    tf->tf_Modulo = modulo;
    tf->tf_CharLoc = loc;
    tf->tf_CharSpace = srcspace ? space : NULL;
    tf->tf_CharKern = srckern ? kern : NULL;

    for (pos = 0, i = 0; i < numchars; i++)
    {
        nw = SCALE(srcloc[i] & 0xFFFF, dy, sy);
        loc[i] = ((ULONG)pos << 16) | nw;
        pos += nw;
        if (srcspace)
            space[i] = SCALE(srcspace[i], dy, sy);
        if (srckern)
            kern[i] = SCALE(srckern[i], dy, sy);
    }

    /* Each row from the source row at its centre */
    for (y = 0; y < dy; y++)
        rows[y] = (UBYTE)(((ULONG)y * 2 + 1) * sy / (2 * dy));

    for (i = 0; i < numchars; i++)
    {
        UWORD b = (UWORD)(srcloc[i] >> 16), dp = (UWORD)(loc[i] >> 16);
        UWORD r0, den, w2, srcmod = sourceFont->tf_Modulo;
        UBYTE *s0, smask0, dmask0;
        register UBYTE *sb, *db;
        register UWORD r, smask, dmask;

        w = (UWORD)(srcloc[i] & 0xFFFF);
        nw = (UWORD)(loc[i] & 0xFFFF);
        if (nw == 0)
            continue;

        /* Source pixel (2x + 1) * w / 2nw, stepped with remainder r */
        den = 2 * nw;
        w2 = 2 * w;
        for (r0 = w; r0 >= den; r0 -= den)
            b++;
        s0 = src + (b >> 3);
        smask0 = 0x80 >> (b & 7);
        dmask0 = 0x80 >> (dp & 7);

        for (y = 0, d = data + (dp >> 3); y < dy; y++, d += modulo)
        {
            if (y > 0 && rows[y] == rows[y - 1])
                continue;
            sb = s0 + (UWORD)(rows[y] * srcmod);
            smask = smask0;
            db = d;
            dmask = dmask0;
            r = r0;
            for (x = 0; x < nw; x++)
            {
                if (*sb & smask)
                    *db |= dmask;
                if (!(dmask >>= 1))
                    dmask = 0x80, db++;
                for (r += w2; r >= den; r -= den)
                    if (!(smask >>= 1))
                        smask = 0x80, sb++;
            }
        }
    }

    /* A row from the same source row as the one above, copied */
    for (y = 1, d = data + modulo; y < dy; y++, d += modulo)
        if (rows[y] == rows[y - 1])
            CopyMem(d - modulo, d, modulo);

    return dfh;

    AROS_LIBFUNC_EXIT
} /* NewScaledDiskFont */
