/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: Graphics function ChangeVPBitMap()
*/

#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <graphics/view.h>

#include "graphics_intern.h"
#include "gfxfuncsupport.h"

/*****************************************************************************

    NAME */
#include <proto/graphics.h>

        AROS_LH3(void, ChangeVPBitMap,

/*  SYNOPSIS */
        AROS_LHA(struct ViewPort *, vp, A0),
        AROS_LHA(struct BitMap *, bm, A1),
        AROS_LHA(struct DBufInfo *, db, A2),

/*  LOCATION */
        struct GfxBase *, GfxBase, 157, Graphics)

/*  FUNCTION

    INPUTS
        vp - pointer to a viewport
        bm - pointer to a BitMap structure. This BitMap structure must be of
             the same layout as the one attached to the viewport
             (same depth, alignment and BytesPerRow)
        db - pointer to a DBufInfo

    RESULT

    NOTES
        There is no display start to move: a bitmap in RAM trades planes
        with the display's, as in LoadView(), at once, and both of db's
        messages are replied then.

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

    HISTORY


******************************************************************************/
{
    AROS_LIBFUNC_INIT

    /* Insert a new bitmap and rebuild the viewport */
    vp->RasInfo->BitMap = bm;
    MakeVPort(GfxBase->ActiView, vp);

    /* If it is on display, show the new bitmap */
    ObtainSemaphore(GfxBase->ActiViewCprSemaphore);
    if(gfx_ViewPortShown(vp, GfxBase))
        LoadView(GfxBase->ActiView);
    ReleaseSemaphore(GfxBase->ActiViewCprSemaphore);

    if(db) {
        /* As on the Amiga: shown first, then safe to draw into the old one */
        if(db->dbi_DispMessage.mn_ReplyPort)
            ReplyMsg(&db->dbi_DispMessage);
        if(db->dbi_SafeMessage.mn_ReplyPort)
            ReplyMsg(&db->dbi_SafeMessage);
    }

    AROS_LIBFUNC_EXIT
} /* ChangeVPBitMap */
