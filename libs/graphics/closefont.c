/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: Graphics function CloseFont()
*/
#include "graphics_intern.h"
#include <graphics/text.h>

/*****************************************************************************

    NAME */
#include <graphics/text.h>
#include <proto/graphics.h>

        AROS_LH1(void, CloseFont,

/*  SYNOPSIS */
        AROS_LHA(struct TextFont *, textFont, A1),

/*  LOCATION */
        struct GfxBase *, GfxBase, 13, Graphics)

/*  FUNCTION
        Close a font.

    INPUTS
        font - font pointer from OpenFont() or OpenDiskFont()

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

    if(!textFont) return;


    ObtainSemaphore(&PrivGBase(GfxBase)->fontsem);
    Forbid();
    /* A disk font stays listed until diskfont.library frees it */
    textFont->tf_Accessors--;
    Permit();
    ReleaseSemaphore(&PrivGBase(GfxBase)->fontsem);

    AROS_LIBFUNC_EXIT

} /* CloseFont */
