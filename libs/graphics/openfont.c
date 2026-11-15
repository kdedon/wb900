/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: Graphics function OpenFont()
*/
#include "graphics_intern.h"
#include <graphics/text.h>


static BOOL samename(CONST_STRPTR a, CONST_STRPTR b)
{
    while(*a == *b && *a != '\0')
        a++, b++;
    return *a == *b;
}

/*****************************************************************************

    NAME */
#include <graphics/text.h>
#include <proto/graphics.h>

        AROS_LH1(struct TextFont *, OpenFont,

/*  SYNOPSIS */
        AROS_LHA(const struct TextAttr *, textAttr, A0),

/*  LOCATION */
        struct GfxBase *, GfxBase, 12, Graphics)

/*  FUNCTION
        Searches for a text font which best matches the specified attributes.

    INPUTS
        textAttr - pointer to a TextAttr or TTextAttr font description.

    RESULT
        Returns NULL if the font can't be found.

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO
        CloseFont(), SetFont(), diskfont.library/OpenDiskFont()

    INTERNALS

    HISTORY
        29-10-95    digulla automatically created from
                            graphics_lib.fd and clib/graphics_protos.h

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct TextFont *tf, *best_so_far = NULL;
    WORD            bestmatch = 0;


    if(!textAttr->ta_Name) return NULL;

    /* Search for font in the fontlist */
    ObtainSemaphore(&PrivGBase(GfxBase)->fontsem);
    Forbid();
    for(tf = (struct TextFont *)GfxBase->TextFonts.lh_Head;
            tf->tf_Message.mn_Node.ln_Succ;
            tf = (struct TextFont *)tf->tf_Message.mn_Node.ln_Succ) {
        if(samename(tf->tf_Message.mn_Node.ln_Name, textAttr->ta_Name)) {
            UWORD           match;
            struct TextAttr match_ta;

            match_ta.ta_Name  = tf->tf_Message.mn_Node.ln_Name;
            match_ta.ta_YSize = tf->tf_YSize;
            match_ta.ta_Style = tf->tf_Style;
            match_ta.ta_Flags = tf->tf_Flags;

            /* No font has a TextFontExtension, so none has tags */
            match = WeighTAMatch(textAttr, &match_ta, NULL);
            if(match > bestmatch) {
                bestmatch = match;
                best_so_far = tf;
            }
        }
    }

    if(best_so_far) best_so_far->tf_Accessors ++;

    Permit();
    ReleaseSemaphore(&PrivGBase(GfxBase)->fontsem);

    return best_so_far;

    AROS_LIBFUNC_EXIT

} /* OpenFont */
