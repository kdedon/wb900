/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: Graphics function LoadView()
*/

#include <graphics/view.h>

#include "graphics_intern.h"

/*****************************************************************************

    NAME */
#include <proto/graphics.h>

        AROS_LH1(void, LoadView,

/*  SYNOPSIS */
        AROS_LHA(struct View *, view, A1),

/*  LOCATION */
        struct GfxBase *, GfxBase, 37, Graphics)

/*  FUNCTION
        Display a new view

    INPUTS
        view - pointer to the View structure which contains the pointer to the
               constructed coprocessor instructions list, or NULL

    RESULT
        None.

    NOTES
        The first ViewPort that is not hidden is shown: its bitmap when
        that is a display's, or one in RAM of the display's mode and size,
        which takes the display's planes from the bitmap that has them and
        gives it its own, each keeping its pixels; another bitmap of the
        display's format is copied in.  A display is taken from the
        console with its bitmap (AllocBitMap() with BMATags_DisplayID),
        and LoadView(NULL), or a View that shows no display, gives it back
        once that bitmap is freed.

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

    HISTORY


******************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct ViewPort *vp;
    struct BitMap *bm = NULL;
    struct GfxDriver *gd;

    ObtainSemaphore(GfxBase->ActiViewCprSemaphore);

    GfxBase->ActiView = view;

    for (vp = view ? view->ViewPort : NULL; vp; vp = vp->Next)
        if (!(vp->Modes & VP_HIDE))
            break;
    if (vp)
        bm = gfx_ViewPortDisplay(vp, GfxBase);

    gd = PrivGBase(GfxBase)->display;
    if (PrivGBase(GfxBase)->shown && !bm)
        gd->gd_Show(gd, NULL);
    if (bm)
        gd->gd_Show(gd, bm);
    PrivGBase(GfxBase)->shown = bm;
    /* graphics' own bitmap for the display's planes goes once not shown */
    if (PrivGBase(GfxBase)->spare && PrivGBase(GfxBase)->spare != bm) {
        FreeBitMap(PrivGBase(GfxBase)->spare);
        PrivGBase(GfxBase)->spare = NULL;
    }

    ReleaseSemaphore(GfxBase->ActiViewCprSemaphore);

    AROS_LIBFUNC_EXIT
} /* LoadView */
