/*
    Copyright (C) 1995-2013, The AROS Development Team. All rights reserved.
    Copyright (C) 2001-2003, The MorphOS Development Team. All Rights Reserved.
*/

#include <intuition/gadgetclass.h>
#include <intuition/intuitionbase.h>
#include <intuition/windecorclass.h>
#include <graphics/rpattr.h>

#include <proto/layers.h>
#include <proto/graphics.h>
#include <proto/layers.h>

#include "inputhandler_actions.h"
#include "intuition_intern.h"

/*****************************************************************************

    NAME */
#include <proto/intuition.h>

        AROS_LH1(void, RefreshWindowFrame,

/*  SYNOPSIS */
        AROS_LHA(struct Window *, window, A0),

/*  LOCATION */
        struct IntuitionBase *, IntuitionBase, 76, Intuition)

/*  FUNCTION
        Redraw window borders.

    INPUTS
        window - pointer to a window whose borders should be redrawn

    RESULT

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    

    int_refreshwindowframe(window, 0, 0, IntuitionBase);

    return;

    AROS_LIBFUNC_EXIT
} /* RefreshWindowFrame */

/* dwbflags: more WDM_DRAW_WINBORDER flags */
VOID int_RefreshWindowFrame(struct Window *window,
                            LONG mustbe, LONG mustnotbe, ULONG dwbflags,
                            struct IntuitionBase *IntuitionBase)
{
    /* Draw a frame around the window */
    struct LayersBase *LayersBase = GetPrivIBase(IntuitionBase)->LayersBase;
    struct RastPort *rp = window->BorderRPort;
    struct DrawInfo *dri;
    struct Region   *old_clipregion;

    WORD             old_scroll_x, old_scroll_y;

    if (!(window->Flags & WFLG_BORDERLESS))
    {
        dri = GetScreenDrawInfo(window->WScreen);
        if (dri)
        {
            LOCK_REFRESH(window->WScreen);
            LOCKGADGET(IntuitionBase)
            if ((rp->Layer==NULL) ||
                    ((!(window->Flags & WFLG_GIMMEZEROZERO)) && (rp->Layer != window->RPort->Layer)))
            {

            }

            LockLayer(0,rp->Layer);

            old_scroll_x = rp->Layer->Scroll_X;
            old_scroll_y = rp->Layer->Scroll_Y;

            rp->Layer->Scroll_X = 0;
            rp->Layer->Scroll_Y = 0;

            old_clipregion = InstallClipRegion(rp->Layer, NULL);

            {
                struct wdpDrawWinBorder  msg;

                msg.MethodID            = WDM_DRAW_WINBORDER;
                msg.wdp_TrueColor       = (((struct IntScreen *)window->WScreen)->DInfo.dri_Flags & DRIF_DIRECTCOLOR) ? TRUE : FALSE;
                msg.wdp_Window          = window;
                msg.wdp_RPort           = rp;
                msg.wdp_Flags           = ((mustbe == REFRESHGAD_TOPBORDER) ? WDF_DWB_TOP_ONLY : 0) | dwbflags;
                msg.wdp_Dri             = dri;
                msg.wdp_UserBuffer      = ((struct IntWindow *)window)->DecorUserBuffer;
                DoMethodA(((struct IntScreen *)(window->WScreen))->WinDecorObj, (Msg)&msg);
            }

            /* Emm: RefreshWindowFrame() is documented to refresh *all* the gadgets,
             * but when a window is activated/deactivated, only border gadgets
             * are refreshed. */
            /* Refresh rel gadgets first, since wizard.library (burn in hell!) seems
             * to rely on that. */
            int_refreshglist(window->FirstGadget,
                             window,
                             NULL,
                             -1,
                             mustbe | REFRESHGAD_REL,
                             mustnotbe,
                             IntuitionBase);
            int_refreshglist(window->FirstGadget,
                             window,
                             NULL,
                             -1,
                             mustbe,
                             mustnotbe | REFRESHGAD_REL,
                             IntuitionBase);

            InstallClipRegion(rp->Layer,old_clipregion);

            rp->Layer->Scroll_X = old_scroll_x;
            rp->Layer->Scroll_Y = old_scroll_y;

            UnlockLayer(rp->Layer);
            UNLOCKGADGET(IntuitionBase)

            UNLOCK_REFRESH(window->WScreen);

            FreeScreenDrawInfo(window->WScreen, (struct DrawInfo *)dri);

        } /* if (dri) */

    } /* if (!(win->Flags & WFLG_BORDERLESS)) */
}
