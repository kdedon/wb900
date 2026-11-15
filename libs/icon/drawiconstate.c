/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.
*/

#include <workbench/icon.h>
#include <intuition/imageclass.h>

#include "icon_intern.h"

/* The pens of an icon not laid out on a screen, without a DrawInfo */
static const UWORD defpens[NUMDRIPENS] =
{
    0, 1, 1, 2, 1, 3, 1, 0, 2, 1, 2, 1
};

/*****************************************************************************

    NAME */
#include <proto/icon.h>

        AROS_LH7(void, DrawIconStateA,

/*  SYNOPSIS */
        AROS_LHA(struct RastPort *,   rp,       A0),
        AROS_LHA(struct DiskObject *, icon,     A1),
        AROS_LHA(STRPTR,              label,    A2),
        AROS_LHA(LONG,                leftEdge, D0),
        AROS_LHA(LONG,                topEdge,  D1),
        AROS_LHA(ULONG,               state,    D2),
        AROS_LHA(struct TagItem *,    tags,     A3),

/*  LOCATION */
        struct IconBase *, IconBase, 27, Icon)

/*  FUNCTION
        Draw an icon like an image.
        
    INPUTS
        rp       - rastport to draw into
        icon     - the icon
        label    - label string
        leftEdge,
        topEdge  - drawing position
        state    - drawing state, see intuition/imageclass.h
    
    RESULT

    NOTES
        leftEdge, topEdge is the top left corner of the frame, as in
        GetIconRectangleA().

    EXAMPLE

    BUGS

    SEE ALSO
        intuition/imageclass.h

    INTERNALS

    HISTORY

*****************************************************************************/
{
    AROS_LIBFUNC_INIT
    LONG textTop,textLeft;
    struct NativeIcon *ni;
    struct Rectangle rect;
    struct DrawInfo *DrawInfo;
    BOOL EraseBackground;
    BOOL Frameless;
    BOOL Borderless;
    BOOL selected = (state == IDS_SELECTED);
    UWORD Pens[NUMDRIPENS];
    UWORD Width, Height;
    int i;

    /* No GfxBase? No RastPort? Then we can't draw anything. */
    if (!GfxBase || !rp)
        return;

    ni = GetNativeIcon(icon, LB(IconBase));

    Frameless = GetTagData(ICONDRAWA_Frameless, IconBase->ib_Frameless || (ni && ni->ni_Frameless), tags);
    Borderless = GetTagData(ICONDRAWA_Borderless, FALSE, tags);
    EraseBackground = GetTagData(ICONDRAWA_EraseBackground, !(IconBase->ib_Frameless || (ni && ni->ni_Frameless)), tags);
    DrawInfo = (struct DrawInfo *)GetTagData(ICONDRAWA_DrawInfo, (IPTR)NULL, tags);

    for (i = 0; i < NUMDRIPENS; i++) {
        if (DrawInfo && DrawInfo->dri_NumPens > i)
            Pens[i] = DrawInfo->dri_Pens[i];
        else if (ni && ni->ni_Screen)
            Pens[i] = ni->ni_Pens[i];
        else
            Pens[i] = defpens[i];
    }

    rect.MinX = rect.MinY = rect.MaxX = rect.MaxY = 0;
    if (!Borderless) {
        rect = IconBase->ib_EmbossRectangle;
        if (!Frameless) {
            EraseBackground = TRUE;
            rect.MinX -= 1;
            rect.MinY -= 1;
            rect.MaxX += 1;
            rect.MaxY += 1;
        }
    } else {
        Frameless = TRUE;
    }

    if (ni && ni->ni_Screen) {
        Width = ni->ni_Width;
        Height = ni->ni_Height;
    } else {
        Width = icon->do_Gadget.Width;
        Height = icon->do_Gadget.Height;
    }

    if (label != NULL) {
        struct TextExtent extent;
        LONG txtlen = strlen(label);
        LONG wDelta;

        if (txtlen > IconBase->ib_MaxNameLength)
            txtlen = IconBase->ib_MaxNameLength;

        wDelta = Width + (rect.MaxX - rect.MinX);
        textTop = Height + (rect.MaxY - rect.MinY);

        TextExtent(rp, label, txtlen, &extent);

        /* leftEdge is the icon's left edge. Center the label around it. */
        textLeft = (wDelta - extent.te_Width - 2 * LABEL_MX) / 2 + LABEL_MX;

        /* A solid box keeps the text readable on a patterned backdrop */
        SetDrMd(rp, JAM1);
        SetAPen(rp, Pens[BACKGROUNDPEN]);
        RectFill(rp, leftEdge + textLeft - LABEL_MX, topEdge + textTop,
                 leftEdge + textLeft + extent.te_Width - 1 + LABEL_MX,
                 topEdge + textTop + extent.te_Height - 1 + 2 * LABEL_MY);
        SetAPen(rp, Pens[TEXTPEN]);

        Move(rp, leftEdge + textLeft,
             topEdge + textTop + LABEL_MY - extent.te_Extent.MinY);
        Text(rp, label, txtlen);
    }
    
    topEdge -= rect.MinY;
    leftEdge -=  rect.MinX;

    if (EraseBackground) {
        SetAPen(rp, Pens[BACKGROUNDPEN]);
        RectFill(rp, leftEdge + rect.MinX, topEdge + rect.MinY,
                     leftEdge + Width + rect.MaxX - 1, topEdge + Height + rect.MaxY - 1);
    }

    if (ni && ni->ni_Screen)
    {
        struct BitMap *bm;

        /* If we don't have selected bitmap,
         * use the normal bitmap
         */
        bm = ni->ni_Image[selected ? 1 : 0].BitMap;
        if (bm == NULL)
            bm = ni->ni_Image[0].BitMap;

        if (bm)
        {
            BltBitMapRastPort(bm, 0, 0,
                              rp, leftEdge, topEdge,
                              Width, Height, 0xC0);
            goto emboss;
        }
    }

    /* No layout: the image itself */
    {
        struct Image *image = icon->do_Gadget.GadgetRender;
        ULONG imagestate = IDS_NORMAL;

        if (selected)
        {
            if ((icon->do_Gadget.Flags & GFLG_GADGHIGHBITS) == GFLG_GADGHIMAGE &&
                icon->do_Gadget.SelectRender)
                image = icon->do_Gadget.SelectRender;
            else
                imagestate = IDS_SELECTED;
        }
        if (image)
            DrawImageState(rp, image, leftEdge, topEdge, imagestate, DrawInfo);
    }

emboss:
    /* Draw the 3D border */
    if (!Frameless) {

        rect.MinX += leftEdge;
        rect.MaxX += leftEdge + Width - 1;
        rect.MinY += topEdge;
        rect.MaxY += topEdge + Height - 1;

        /* Draw the left and top lines */
        SetAPen(rp, selected ? Pens[SHADOWPEN] : Pens[SHINEPEN]);
        Move(rp, rect.MinX, rect.MaxY - 1);
        Draw(rp, rect.MinX, rect.MinY);
        Draw(rp, rect.MaxX - 1, rect.MinY);

        /* Draw the right and bottom lines */
        SetAPen(rp, selected ? Pens[SHINEPEN] : Pens[SHADOWPEN]);
        Move(rp, rect.MaxX, rect.MinY + 1);
        Draw(rp, rect.MaxX, rect.MaxY);
        Draw(rp, rect.MinX + 1, rect.MaxY);

        SetAPen(rp, Pens[BACKGROUNDPEN]);
        WritePixel(rp, rect.MinX, rect.MaxY);
        WritePixel(rp, rect.MaxX, rect.MinY);

    }

    AROS_LIBFUNC_EXIT
} /* DrawIconStateA() */
