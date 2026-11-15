/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: Graphics function SetRPAttrsA()
*/


#include "graphics_intern.h"
#include "gfxfuncsupport.h"

/*****************************************************************************

    NAME */
#include <graphics/rastport.h>
#include <graphics/rpattr.h>
#include <graphics/clip.h>
#include <utility/tagitem.h>
#include <proto/graphics.h>

        AROS_LH2(void, SetRPAttrsA,

/*  SYNOPSIS */
        AROS_LHA(struct RastPort *, rp, A0),
        AROS_LHA(struct TagItem  *, tags, A1),

/*  LOCATION */
        struct GfxBase *, GfxBase, 173, Graphics)

/*  FUNCTION
        Modify rastport with values from a taglist.

    INPUTS
        rp   - RastPort
        tags - see below

    TAGS
        RPTAG_Font (struct TextFont *)  - Font for Text()
        RPTAG_APen (UBYTE)              - Primary rendering pen
        RPTAG_BPen (UBYTE)              - Secondary rendering pen
        RPTAG_DrMd (UBYTE)              - Drawing mode (graphics/rastport.h)
        RPTAG_OutlinePen (UBYTE)        - Area Outline pen
        RPTAG_WriteMask (ULONG)         - Bit mask for writing

        The following tags are compatible with MorphOS (V51) :

        RPTAG_FgColor (ULONG)           - Primary rendering color in A8R8G8B8
                                          format. Only working on
                                          hicolor/truecolor bitmaps/screens.
        RPTAG_BgColor (ULONG)           - Secondary rendering color in
                                          A8R8G8B8 format. Only working on
                                          hicolor/truecolor bitmaps/screens.
        RPTAG_PenMode (BOOL)            - TRUE if traditional pen numbers
                                          should be used, FALSE if direct RGB
                                          colors should be used. Has no effect
                                          on non-RTG displays.

        The following tags are compatible with AmigaOSv4 (V51) :

        RPTAG_RemapColorFonts (BOOL)    - Automatically remap colorfonts to
                                          their color on hicolor/truecolor
                                          screens.

        AROS-specific extensions

        RPTAG_ClipRectangle (struct Rectangle *) - Clipping rectangle
        RPTAG_ClipRectangleFlags (LONG) - RPCRF_RELRIGHT | RPCRF_RELBOTTOM
                                          (see graphics/rpattrs.h)

    RESULT
        None.

    NOTES
        Setting one of RPTAG_ClipRectangle or RPTAG_ClipRectangleFlags
        allocates internal extra data for the RastPort. After finishing using
        this RastPort, you need to manually deallocate the extra data using
        FreeVec(rp->RP_Extra).

    EXAMPLE

    BUGS

    SEE ALSO
        GetRPAttrsA(), graphics/rpattr.h

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct TagItem *tag, *tstate = tags;

    while((tag = NextTagItem(&tstate))) {
        switch(tag->ti_Tag) {
        case RPTAG_Font:
            SetFont(rp, (struct TextFont *)(tag->ti_Data));
            break;

        case RPTAG_APen:
            SetAPen(rp, tag->ti_Data);
            break;

        case RPTAG_BPen:
            SetBPen(rp, tag->ti_Data);
            break;

        case RPTAG_DrMd:
            SetDrMd(rp, tag->ti_Data);
            break;

        case RPTAG_OutlinePen:
            SetOutlinePen(rp, tag->ti_Data);
            break;

        case RPTAG_WriteMask:
            SetWriteMask(rp, tag->ti_Data);
            break;

        case RPTAG_MaxPen:
            break;

        case RPTAG_DrawBounds:
            break;

        case RPTAG_PenMode:
            /* Pens are all there is: no direct colours */
            break;

        } /* switch (tag) */

    } /* while (tag) */

    AROS_LIBFUNC_EXIT
} /* SetRPAttrsA */
