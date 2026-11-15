/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.
*/

#include "icon_intern.h"

/*****************************************************************************

    NAME */
#include <proto/icon.h>

        AROS_LH5(BOOL, GetIconRectangleA,

/*  SYNOPSIS */
        AROS_LHA(struct RastPort *, rp, A0),
        AROS_LHA(struct DiskObject *, icon, A1),
        AROS_LHA(STRPTR, label, A2),
        AROS_LHA(struct Rectangle *, rectangle, A3),
        AROS_LHA(struct TagItem *, tags, A4),
/*  LOCATION */
        struct IconBase *, IconBase, 28, Icon)

/*  FUNCTION
        Query size of icon.
        
    INPUTS
        rp        - reference RastPort (for font)
        icon      - icon to be queried
        label     - label string
        rectangle - resulting size

    RESULT
        TRUE success

    NOTES
        Only very limited implemented.

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

    HISTORY

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct NativeIcon *nativeicon;
    BOOL isFrameless;
    BOOL isBorderless;
    struct Rectangle rect;
    LONG width, height;

    nativeicon = GetNativeIcon(icon, LB(IconBase));

    isFrameless = GetTagData(ICONDRAWA_Frameless, LB(IconBase)->ib_Frameless ||
        (nativeicon && nativeicon->ni_Frameless), tags);
    isBorderless = GetTagData(ICONDRAWA_Borderless, FALSE, tags);

    rect.MinX = rect.MinY = rect.MaxX = rect.MaxY = 0;
    if (!isBorderless) {
        rect = LB(IconBase)->ib_EmbossRectangle;
        if (!isFrameless) {
            rect.MinX -= 1;
            rect.MinY -= 1;
            rect.MaxX += 1;
            rect.MaxY += 1;
        }
    }

    if (nativeicon && nativeicon->ni_Screen)
    {
        width = nativeicon->ni_Width;
        height = nativeicon->ni_Height;
    } else
    {
        width = icon->do_Gadget.Width;
        height = icon->do_Gadget.Height;
    }

    /* The frame's top left corner is where DrawIconStateA() draws */
    rectangle->MinX = 0;
    rectangle->MinY = 0;
    rectangle->MaxX = rect.MaxX - rect.MinX + width - 1;
    rectangle->MaxY = rect.MaxY - rect.MinY + height - 1;

    if (label != NULL) {
        struct TextExtent extent;
        LONG txtlen = strlen(label);

        if (txtlen > IconBase->ib_MaxNameLength)
            txtlen = IconBase->ib_MaxNameLength;

        TextExtent(rp, label, txtlen, &extent);
        rectangle->MaxY += extent.te_Height + 2 * LABEL_MY;
        if (extent.te_Width + 2 * LABEL_MX > (rectangle->MaxX + 1))
        {
            LONG overhang = extent.te_Width + 2 * LABEL_MX - (rectangle->MaxX + 1);
            rectangle->MinX -= overhang / 2;
            rectangle->MaxX += overhang - (overhang / 2);
        }
    }

    return TRUE;

    AROS_LIBFUNC_EXIT
} /* GetIconRectangle */
