/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: Free the memory occupied by a BitMap.
*/

#include <exec/memory.h>
#include <proto/exec.h>

#include "graphics_intern.h"
#include "gfxfuncsupport.h"

/*****************************************************************************

    NAME */
#include <graphics/gfx.h>
#include <proto/graphics.h>

        AROS_LH1(void, FreeBitMap,

/*  SYNOPSIS */
        AROS_LHA(struct BitMap *, bm, A0),

/*  LOCATION */
        struct GfxBase *, GfxBase, 154, Graphics)

/*  FUNCTION
        Returns the memory occupied by the BitMap to the system.

    INPUTS
        bm - The result of AllocBitMap(). Must be non-NULL.

    RESULT
        None.

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO
        AllocBitMap(), AllocRaster(), FreeRaster()

    INTERNALS

    HISTORY

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    if(!bm) return;

    /* Every bitmap from AllocBitMap() is a struct GfxBitMap */
    {
        struct GfxBitMap *gbm = GFXBM(bm);
        UWORD plane, planes = gbm->gb_Driver->gd_Format == GDF_PLANAR ? bm->Depth : 1;

        /* A display's bitmap leaves the display and goes back to its driver */
        if(gbm->gb_Flags & GBF_DISPLAY) {
            ObtainSemaphore(GfxBase->ActiViewCprSemaphore);
            if(PrivGBase(GfxBase)->shown == bm) {
                gbm->gb_Driver->gd_Show(gbm->gb_Driver, NULL);
                PrivGBase(GfxBase)->shown = NULL;
            }
            gbm->gb_Driver->gd_FreeDisplay(gbm->gb_Driver, bm);
            PrivGBase(GfxBase)->vram = NULL;
            ReleaseSemaphore(GfxBase->ActiViewCprSemaphore);
            FreeVec(gbm);
            return;
        }

        if(gbm->gb_Flags & GBF_OWNPLANES) {
            for(plane = 0; plane < planes; plane++)
                FreeMem(bm->Planes[plane], gbm->gb_PlaneSize);
        }

        FreeVec(gbm);
    }

    AROS_LIBFUNC_EXIT
} /* FreeBitMap */
