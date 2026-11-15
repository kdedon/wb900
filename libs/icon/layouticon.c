/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.
*/

#include <exec/types.h>
#include <workbench/icon.h>
#include <utility/tagitem.h>
#include <proto/icon.h>

#include "icon_intern.h"

/*****************************************************************************

    NAME */

    AROS_LH3(BOOL, LayoutIconA,

/*  SYNOPSIS */
        AROS_LHA(struct DiskObject *, icon,   A0),
        AROS_LHA(struct Screen *,     screen, A1),
        AROS_LHA(struct TagItem *,    tags,   A2),

/*  LOCATION */
        struct IconBase *, IconBase, 32, Icon)

/*  FUNCTION
        Draw the icon's images into bitmaps for the screen, which
        DrawIconStateA() then copies; NULL frees them.

    INPUTS

    RESULT

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    ULONG width, height;
    struct NativeIcon *ni;
    struct RastPort rp;
    struct DrawInfo *dri;
    BOOL ret;
    int i;

    ni = GetNativeIcon(icon, LB(IconBase));
    if (!ni)
        return TRUE;

    /* Already mapped to this screen?
     */
    if (screen == ni->ni_Screen)
        return TRUE;

    for (i = 0; i < 2; i++) {
        struct NativeIconImage *image = &ni->ni_Image[i];
        if (image->BitMap) {
            FreeBitMap(image->BitMap);
            image->BitMap = NULL;
        }
    }

    ni->ni_Screen = NULL;
    ni->ni_Width = icon->do_Gadget.Width;
    ni->ni_Height = icon->do_Gadget.Height;

    if (screen == NULL)
        return TRUE;

    dri = GetScreenDrawInfo(screen);
    if (dri == NULL)
        return FALSE;

    /* Look up the DrawInfo Pens we will need for the
     * layout of the border and frame.
     */
    for (i = 0; i < NUMDRIPENS; i++) {
        if (i < dri->dri_NumPens)
            ni->ni_Pens[i] = dri->dri_Pens[i];
        else
            ni->ni_Pens[i] = dri->dri_Pens[DETAILPEN];
    }

    ret = TRUE;

    width = icon->do_Gadget.Width;
    height = icon->do_Gadget.Height;

    for (i = 0; i < 2; i++) {
        struct NativeIconImage *image = &ni->ni_Image[i];
        struct Image *gi = NULL;

        if (i == 1 && ((icon->do_Gadget.Flags & GFLG_GADGHIGHBITS) == GFLG_GADGHIMAGE)) {
            gi = icon->do_Gadget.SelectRender;
        } else if (icon->do_Gadget.Flags & GFLG_GADGIMAGE) {
            gi = icon->do_Gadget.GadgetRender;
        }

        if (gi == NULL)
            continue;

        /* Allocate a bitmap, which is a 'friend' of the screen */
        image->BitMap = AllocBitMap(width, height, dri->dri_Depth, 0, screen->RastPort.BitMap);
        if (image->BitMap == NULL) {
            SetIoErr(ERROR_NO_FREE_STORE);
            ret = FALSE;
            goto exit;
        }

        InitRastPort(&rp);
        rp.BitMap = image->BitMap;
        /* Pen 0 under the image, as on the screen */
        SetRast(&rp, 0);

        if (i == 1 && ((icon->do_Gadget.Flags & GFLG_GADGHIGHBITS) == GFLG_GADGHCOMP)) {
            /* Complement the image's own planes only, so a 4-pen image
               keeps to pens 0-3 on a deeper screen */
            struct Image inv;
            ULONG size = RASSIZE(gi->Width, gi->Height) * gi->Depth;
            UWORD *data = AllocVec(size, MEMF_ANY);
            ULONG n;

            if (data == NULL) {
                SetIoErr(ERROR_NO_FREE_STORE);
                ret = FALSE;
                goto exit;
            }
            for (n = 0; n < size / 2; n++)
                data[n] = ~gi->ImageData[n];
            inv = *gi;
            inv.ImageData = data;
            inv.NextImage = NULL;
            DrawImageState(&rp, &inv, 0, 0, IDS_NORMAL, dri);
            FreeVec(data);
        } else {
            DrawImageState(&rp, gi, 0, 0, IDS_NORMAL, dri);
        }
    }

    ni->ni_Screen = screen;

exit:
    FreeScreenDrawInfo(screen, dri);
    if (ret == TRUE)
        SetIoErr(0);
    return ret;

    AROS_LIBFUNC_EXIT
} /* LayoutIconA() */
