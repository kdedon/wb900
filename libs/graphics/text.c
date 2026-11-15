/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: Graphics function Text()
*/
#include "graphics_intern.h"
#include <graphics/rastport.h>

#include "gfxfuncsupport.h"
#include "gfxformat.h"

void BltTemplateBasedText(struct RastPort *rp, CONST_STRPTR text, ULONG len,
                          struct GfxBase *GfxBase);

/* gfx_Glyphs' job */
struct glyphjob {
    UBYTE       *gj_Ras;
    CONST_STRPTR gj_Text;
    UBYTE       *gj_Data;
    UWORD       *gj_Loc;
    UWORD        gj_Len, gj_Bpr, gj_H, gj_Mod, gj_LoHi, gj_Last;
    WORD        *gj_Kern, *gj_Space;
};

/* The job of len glyphs of text in tf */
static void glyphjob(struct glyphjob *gj, CONST_STRPTR text, UWORD len,
                     struct TextFont *tf)
{
    gj->gj_Text = text;
    gj->gj_Data = tf->tf_CharData;
    gj->gj_Loc = (UWORD *)tf->tf_CharLoc;
    gj->gj_Len = len;
    gj->gj_Mod = tf->tf_Modulo;
    gj->gj_LoHi = (tf->tf_LoChar << 8) | tf->tf_HiChar;
    gj->gj_Last = NUMCHARS(tf) - 1;
    gj->gj_Kern = (WORD *)tf->tf_CharKern;
    gj->gj_Space = (WORD *)tf->tf_CharSpace;
}

/*
 * The template of len glyphs of a fixed 8-pixel font, each from a byte:
 * byte i of each row of bpr bytes from glyph i; a byte after the last
 * glyph is cleared.
 */
static void fastglyphs(UBYTE *raster, struct glyphjob *gj, UWORD bpr,
                       UWORD h)
{
    register UBYTE *d;
    register UWORD n;

    if (bpr > gj->gj_Len)
        for (d = raster + gj->gj_Len, n = h; n > 0; n--, d += bpr)
            *d = 0;
    gj->gj_Ras = raster;
    gj->gj_Bpr = bpr;
    gj->gj_H = h;
    gfx_Glyphs(gj);
}


/*****************************************************************************

    NAME */
#include <graphics/rastport.h>
#include <proto/graphics.h>

        AROS_LH3(void, Text,

/*  SYNOPSIS */
        AROS_LHA(struct RastPort *, rp, A1),
        AROS_LHA(CONST_STRPTR     , string, A0),
        AROS_LHA(WORD             , count, D0),

/*  LOCATION */
        struct GfxBase *, GfxBase, 10, Graphics)

/*  FUNCTION
        Write text to the rastport at the current position.
        The current position is updated to a position after the text.

    INPUTS
        rp     - RastPort
        string - string to print
        count  - number of characters to print

    RESULT

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

    HISTORY
        29-10-95    digulla automatically created from
                            graphics_lib.fd and clib/graphics_protos.h

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    /* Colour and antialiased fonts need diskfont.library: plain fonts only */
    if(count)
        BltTemplateBasedText(rp, string, count, GfxBase);

    AROS_LIBFUNC_EXIT

} /* Text */

/***************************************************************************/

void BltTemplateBasedText(struct RastPort *rp, CONST_STRPTR text, ULONG len,
                          struct GfxBase *GfxBase)
{
    struct TextExtent    te;
    struct glyphjob      gj;
    struct TextFont     *tf;
    WORD                 raswidth, raswidth16, raswidth_bpr, rasheight, x, y, gx;
    UBYTE               *raster;
    BOOL                 is_bold, is_italic, fastfont;
    BOOL                 allocated = FALSE, kept = FALSE;
    ULONG                rassize;

    tf = rp->Font;
    is_bold = (rp->AlgoStyle & FSF_BOLD) != 0;
    is_italic = (rp->AlgoStyle & FSF_ITALIC) != 0;
    fastfont = !is_bold && !is_italic && tf->tf_XSize == 8 &&
               rp->TxSpacing == 0;

    if (fastfont)
    {
        glyphjob(&gj, text, len, tf);
        fastfont = gfx_FastChars(&gj);
    }

    if (fastfont)
    {
        te.te_Width = len * 8;
        te.te_Height = tf->tf_YSize;
        te.te_Extent.MinX = 0;
        te.te_Extent.MaxX = te.te_Width - 1;
        te.te_Extent.MinY = -tf->tf_Baseline;
        te.te_Extent.MaxY = te.te_Height - 1 - tf->tf_Baseline;
    }
    else
        TextExtent(rp, text, len, &te);

    raswidth  = te.te_Extent.MaxX - te.te_Extent.MinX + 1;
    rasheight = te.te_Extent.MaxY - te.te_Extent.MinY + 1;

    raswidth16 = (raswidth + 15) & ~15;
    raswidth_bpr = raswidth16 / 8;
    rassize = RASSIZE(raswidth, rasheight);

    if (rp->TmpRas && rp->TmpRas->RasPtr && rp->TmpRas->Size >= rassize)
        raster = rp->TmpRas->RasPtr;
    else
    {
        if (rassize <= TEXTRAS)
        {
            Forbid();
            kept = !PrivGBase(GfxBase)->textrasbusy;
            PrivGBase(GfxBase)->textrasbusy = TRUE;
            Permit();
        }
        if (kept)
            raster = PrivGBase(GfxBase)->textras;
        else
        {
            raster = AllocRaster(raswidth, rasheight);
            allocated = raster != NULL;
        }
    }

    if(raster) {
        if (!fastfont) {
            ULONG i;

            for (i = 0; i < rassize; i++)
                raster[i] = 0;
        }

        x = -te.te_Extent.MinX;

        /* The standard Amiga screen font is an unstyled, fixed-width,
         * byte-aligned 8-pixel font. Build its template a byte at a time
         * instead of running the generic bit-copy loop for every pixel. */
        if (fastfont)
        {
            fastglyphs(raster, &gj, raswidth_bpr, rasheight);
            x += len * 8;
        }
        else while(len--) {
            UBYTE c = *text++;
            ULONG idx;
            ULONG charloc;
            UWORD glyphwidth, glyphpos, bold;
            UBYTE *glyphdata;
            UBYTE *dst;
            ULONG srcmask;
            ULONG dstmask;

            if(c < tf->tf_LoChar || c > tf->tf_HiChar) {
                idx = NUMCHARS(tf) - 1;
            } else {
                idx = c - tf->tf_LoChar;
            }

            charloc = ((ULONG *)tf->tf_CharLoc)[idx];

            glyphwidth = charloc & 0xFFFF;
            glyphpos = charloc >> 16;

            if(tf->tf_CharKern) {
                x += ((WORD *)tf->tf_CharKern)[idx];
            }


            if (!is_bold && !is_italic && glyphwidth <= 8) {
                WORD wx = x;
                UWORD srcshift = glyphpos & 7;
                UWORD dstshift = wx & 7;

                glyphdata = (UBYTE *)tf->tf_CharData + glyphpos / 8;
                dst = raster + wx / 8;

                for (y = 0; y < rasheight; y++) {
                    UWORD source = (UWORD)glyphdata[0] << 8;
                    UBYTE bits;
                    UWORD packed;

                    if (srcshift + glyphwidth > 8)
                        source |= glyphdata[1];
                    bits = (source >> (8 - srcshift)) &
                           (0xff << (8 - glyphwidth));
                    packed = ((UWORD)bits << 8) >> dstshift;

                    if (packed & 0xff00)
                        dst[0] |= packed >> 8;
                    if ((dstshift + glyphwidth > 8) && (packed & 0xff))
                        dst[1] |= packed;

                    glyphdata += tf->tf_Modulo;
                    dst += raswidth_bpr;
                }
            } else for(bold = 0; bold <= is_bold; bold++) {
                WORD wx;
                WORD italicshift, italiccheck = 0;

                if(is_italic) {
                    italiccheck = tf->tf_Baseline;
                    italicshift = italiccheck / 2;
                } else {
                    italicshift = 0;
                }

                wx = x + italicshift + (bold ? tf->tf_BoldSmear : 0);

                glyphdata = ((UBYTE *)tf->tf_CharData) + glyphpos / 8;
                dst = raster + wx / 8;

                for(y = 0; y < rasheight; y++) {
                    UBYTE *glyphdatax = glyphdata;
                    UBYTE *dstx = dst;
                    UBYTE srcdata;

                    srcmask = 0x80 >> (glyphpos & 7);
                    dstmask = 0x80 >> (wx & 7);

                    srcdata = *glyphdatax;

                    for(gx = 0; gx < glyphwidth; gx++) {
                        if(srcdata & srcmask) {
                            *dstx |= dstmask;
                        }

                        if(dstmask == 0x1) {
                            dstmask = 0x80;
                            dstx++;
                        } else {
                            dstmask >>= 1;
                        }

                        if(srcmask == 0x1) {
                            srcmask = 0x80;
                            glyphdatax++;
                            srcdata = *glyphdatax;
                        } else {
                            srcmask >>= 1;
                        }

                    } /* for(gx = 0; gx < glyphwidth; gx++) */

                    glyphdata += tf->tf_Modulo;
                    dst += raswidth_bpr;

                    if(is_italic) {
                        italiccheck--;
                        if(italiccheck & 1) {
                            italicshift--;

                            wx--;
                            if((wx & 7) == 7) dst--;

                        }
                    }

                } /* for(y = 0; y < rasheight; y++) */

            } /* for(bold = 0; bold < ((rp->AlgoStyle & FSF_BOLD) ? 2 : 1); bold++) */

            if(tf->tf_CharSpace) {
                x += ((WORD *)tf->tf_CharSpace)[idx];
            } else {
                x += tf->tf_XSize;
            }

            x += rp->TxSpacing;

        } /* while(len--) */

        if(rp->AlgoStyle & FSF_UNDERLINED) {
            UBYTE *dst;
            ULONG prev_word, act_word = 0, next_word, word;
            WORD count;
            LONG underline;

            underline = rp->TxBaseline + 1;
            if(underline < rasheight - 1) underline++;

            if(underline < rasheight) {
                dst = raster + underline * (LONG)raswidth_bpr;
                next_word = *(UWORD *)dst;
                count  = raswidth16 / 16;

                while(count--) {
                    prev_word = act_word;
                    act_word = next_word;
                    if(count > 1) {
                        next_word = ((UWORD *)dst)[1];
                    } else {
                        next_word = 0;
                    }
                    word = ((act_word << 1) & 0xFFFF) + (next_word >> 15);
                    word |= (act_word >> 1) + ((prev_word << 15) & 0xFFFF);
                    word &= ~act_word;

                    word = 0xFFFF & ~ word;

                    *(UWORD *)dst = word;
                    dst += 2;

                } /* while(count--) */

            } /* if (underline < rasheight) */

        } /* if (rp->AlgoStyle & FSF_UNDERLINED) */

        BltTemplate(raster,
                    0,
                    raswidth_bpr,
                    rp,
                    rp->cp_x + te.te_Extent.MinX,
                    rp->cp_y - rp->TxBaseline,
                    raswidth,
                    rasheight);

        if (allocated)
            FreeRaster(raster, raswidth, rasheight);
        if (kept)
            PrivGBase(GfxBase)->textrasbusy = FALSE;

    } /* if ((raster = AllocRaster(raswidth, rasheight))) */

    Move(rp, rp->cp_x + te.te_Width, rp->cp_y);

}

