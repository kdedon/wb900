/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.
    Copyright (C) 2001-2003, The MorphOS Development Team. All Rights Reserved.
*/

#include <proto/graphics.h>
#include <proto/layers.h>
#include "intuition_intern.h"
#include <intuition/classusr.h>
#include <proto/alib.h>

//#include <proto/arossupport.h>

/*****************************************************************************

    NAME */
#include <graphics/rastport.h>
#include <graphics/rpattr.h>
#include <intuition/intuition.h>
#include <intuition/imageclass.h>
#include <proto/intuition.h>

        AROS_LH6(void, DrawImageState,

/*  SYNOPSIS */
        AROS_LHA(struct RastPort *, rp,         A0),
        AROS_LHA(struct Image    *, image,      A1),
        AROS_LHA(WORD             , leftOffset, D0),
        AROS_LHA(WORD             , topOffset,  D1),
        AROS_LHA(ULONG            , state,      D2),
        AROS_LHA(struct DrawInfo *, drawInfo,   A2),

/*  LOCATION */
        struct IntuitionBase *, IntuitionBase, 103, Intuition)

/*  FUNCTION
        This function renders an image in a certain state.

    INPUTS
        rp - Render in this RastPort
        image - Render this image
        leftOffset, topOffset - Add this offset to the position stored in the
            image.
        state - Which state (see intuition/imageclass.h for possible
            values).
        drawInfo - The DrawInfo from the screen.

    RESULT
        None.

    NOTES
        DrawImageState(), handles both boopsi and conventional images.

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct GfxBase *GfxBase = GetPrivIBase(IntuitionBase)->GfxBase;
    struct LayersBase *LayersBase = GetPrivIBase(IntuitionBase)->LayersBase;
    IPTR apen, bpen, drmd;

    SANITY_CHECK(rp)
    SANITY_CHECK(image)

    if (rp->Layer) LockLayer(0,rp->Layer);

    /* Store important variables of the RastPort */
    {
        struct TagItem rptags[4];

        rptags[0].ti_Tag = RPTAG_APen; rptags[0].ti_Data = (IPTR)&apen;
        rptags[1].ti_Tag = RPTAG_BPen; rptags[1].ti_Data = (IPTR)&bpen;
        rptags[2].ti_Tag = RPTAG_DrMd; rptags[2].ti_Data = (IPTR)&drmd;
        rptags[3].ti_Tag = TAG_DONE;
        GetRPAttrsA(rp, rptags);
    }

    while(image)
    {
        if (image->Depth == CUSTOMIMAGEDEPTH)
        {
            struct impDraw method;

            method.MethodID = IM_DRAW;
            method.imp_RPort = rp;
            method.imp_Offset.X = leftOffset;
            method.imp_Offset.Y = topOffset;
            method.imp_State = state;
            method.imp_DrInfo = drawInfo;
            DoMethodA ((Object *)image, (Msg)&method);
        }
        else
        {
            WORD    x, y, d;
            UWORD   shift;
            ULONG   planeonoff, planepick;

            /*  kprintf("*** Drawing Image %x. Next Image = %x\n widht = %d  height = %d  depth = %d  planepick = %d  planeonoff = %d\n",
            image,image->NextImage,
            image->Width,image->Height,image->Depth,image->PlanePick,image->PlaneOnOff);*/

            planepick  = image->PlanePick;
            planeonoff = image->PlaneOnOff & ~planepick;

            if (planepick == 0)
            {
                SetAPen(rp, planeonoff);
                RectFill(rp, leftOffset + image->LeftEdge,
                         topOffset  + image->TopEdge,
                         leftOffset + image->LeftEdge + image->Width  - 1,
                         topOffset  + image->TopEdge  + image->Height - 1);

                image = image->NextImage;
                continue;
            }

            /* Use x to store size of one image plane */
            x = ((image->Width + 15) >> 4) * image->Height;
            y = 0;

            shift = 1;

            {
                struct BitMap bitmap;
                int depth;
                int imagedepth = image->Depth;

                /* That makes far more sense than just a 8
                * R.Schmidt...still doesn`t resolve some weird icon problem i have
                */
                //depth = rp->BitMap->Depth;

                /*
                ** Be system friendly. - Piru
                */
                depth = GetBitMapAttr(rp->BitMap, BMA_DEPTH);
                if (depth > 8)
                    depth = 8;
                InitBitMap(&bitmap, depth, image->Width, image->Height);

                for(d = 0; d < depth; d++)
                {
                    /* Don't use more planes than image->depth. This solves
                       corrupt images of Garshneblanker's FlyingToaster which
                       uses a PlanePick of 0xff. */
                    if ((imagedepth > 0) && (planepick & shift))
                    {
                        imagedepth--;
                        bitmap.Planes[d] = (PLANEPTR)(image->ImageData + (y++) * x);
                    }
                    else
                    {
                        bitmap.Planes[d] = (planeonoff & shift) ? (PLANEPTR)-1 : NULL;
                    }
                    shift <<= 1;
                }

                BltBitMapRastPort(&bitmap,
                                  0, 0,
                                  rp,
                                  leftOffset + image->LeftEdge, topOffset + image->TopEdge,
                                  image->Width, image->Height,
                                  (state == IDS_SELECTED) ? 0x30 : 0xC0);
            }

        }

        image = image->NextImage;
    }

    {
        struct TagItem rptags[4];

        rptags[0].ti_Tag = RPTAG_APen; rptags[0].ti_Data = apen;
        rptags[1].ti_Tag = RPTAG_BPen; rptags[1].ti_Data = bpen;
        rptags[2].ti_Tag = RPTAG_DrMd; rptags[2].ti_Data = drmd;
        rptags[3].ti_Tag = TAG_DONE;
        SetRPAttrsA(rp, rptags);
    }

    if (rp->Layer) UnlockLayer(rp->Layer);

    AROS_LIBFUNC_EXIT
} /* DrawImageState */
