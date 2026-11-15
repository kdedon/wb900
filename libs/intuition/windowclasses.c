/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.
    Copyright (C) 2001-2003, The MorphOS Development Team. All Rights Reserved.

    Classes for window decor stuff, like dragbar, close etc.
*/

/***********************************************************************************/

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/utility.h>
#include <proto/layers.h>

#include <intuition/classes.h>
#include <intuition/gadgetclass.h>
#include <intuition/cghooks.h>
#include <intuition/imageclass.h>
#include <intuition/extensions.h>
#include <aros/asmcall.h>

#include "gadgets.h"

#include "intuition_intern.h"
#include "inputhandler.h"
#include "inputhandler_support.h"
#include "inputhandler_actions.h"

#undef SDEBUG

/*

jDc: opaque resize code is NOT suitable for other apps than MUI, I have no intentions in fixing it so that
it would work with all apps out there. Use mui or die :)

The idea behind the trick is that we use a func that checks if apps reply to idcmps in some period
of time. As long as they are replying fast we can do another layer resize, send idcmp to app.
If they do not then we wait for a while and after some time, if one or more apps don't reply
we do the resize again (the timeout is also useful when we have some dead app with windows
on screen.

*/

#   define OPAQUESIZE 0

/***********************************************************************************/

#define WSIG_MOVE SIGF_INTUITION
#define WSIG_DIE  SIGF_ABORT

/***********************************************************************************/

/***********************************************************************************/

/* drawwindowframe is used when the user drags or resizes a window */

#define DWF_THICK_X 2
#define DWF_THICK_Y 2

/***********************************************************************************/

static void cliprectfill(struct Screen *scr, struct RastPort *rp,
                         WORD x1, WORD y1, WORD x2, WORD y2,
                         struct IntuitionBase *IntuitionBase)
{
    struct GfxBase *GfxBase = GetPrivIBase(IntuitionBase)->GfxBase;
    WORD scrx2 = scr->Width  - 1;
    WORD scry2 = scr->Height - 1;

    /* Check if inside at all */

    if (!((x1 > scrx2) || (x2 < 0) || (y1 > scry2) || (y2 < 0)))
    {
        if (x1 < 0) x1 = 0;
        if (y1 < 0) y1 = 0;
        if (x2 > scrx2) x2 = scrx2;
        if (y2 > scry2) y2 = scry2;

        /* paranoia */

        if ((x2 >= x1) && (y2 >= y1))
        {
            RectFill(rp, x1, y1, x2, y2);
        }
    }

}

/***********************************************************************************/

static void drawwindowframe(struct Screen *scr, struct RastPort *rp,
                            WORD x1, WORD y1, WORD x2, WORD y2,
                            struct IntuitionBase *IntuitionBase)
{
    /* this checks should not be necessary, but just to be sure */

    if (x2 < x1)
    {
        /* swap x2 and x1 */
        x2 ^= x1;
        x1 ^= x2;
        x2 ^= x1;
    }

    if (y2 < y1)
    {
        /* swap y2 and y1 */
        y2 ^= y1;
        y1 ^= y2;
        y2 ^= y1;
    }

    if (((x2 - x1) < (DWF_THICK_X * 2)) ||
        ((y2 - y1) < (DWF_THICK_Y * 2)))
    {
        cliprectfill(scr, rp, x1, y1, x2, y2, IntuitionBase);
    }
    else
    {
        cliprectfill(scr, rp, x1, y1, x2, y1 + DWF_THICK_Y - 1, IntuitionBase);
        cliprectfill(scr, rp, x2 - DWF_THICK_X + 1, y1 + DWF_THICK_Y, x2, y2, IntuitionBase);
        cliprectfill(scr, rp, x1, y2 - DWF_THICK_Y + 1, x2 - DWF_THICK_X, y2, IntuitionBase);
        cliprectfill(scr, rp, x1, y1 + DWF_THICK_Y, x1 + DWF_THICK_X - 1, y2 - DWF_THICK_Y, IntuitionBase);
    }
}

/***********************************************************************************/

/***********************************************************************************/

/* Draw the frame that shows where the dragged window will go. It is drawn
   in COMPLEMENT mode, so drawing it again erases it. */

static void dragbar_drawframe(struct dragbar_data *data, struct Window *w,
                              struct IntuitionBase *IntuitionBase)
{
    drawwindowframe(w->WScreen
                    , data->rp
                    , data->curleft
                    , data->curtop
                    , data->curleft + w->Width  - 1
                    , data->curtop  + w->Height - 1
                    , IntuitionBase
                    );
}

/***********************************************************************************/

/* Release the locks that are held while the user drags a window */

static void dragbar_releaselocks(struct dragbar_data *data, struct Window *w,
                                 struct IntuitionBase *IntuitionBase)
{
    struct LayersBase *LayersBase = GetPrivIBase(IntuitionBase)->LayersBase;

    if (data->drag_layerlock)
    {
        UnlockLayers(&w->WScreen->LayerInfo);
        data->drag_layerlock = FALSE;
    }

    if (data->drag_gadgetlock)
    {
        ReleaseSemaphore(&GetPrivIBase(IntuitionBase)->GadgetLock);
        data->drag_gadgetlock = FALSE;
    }

    if (data->drag_inputhandlerlock)
    {
        ReleaseSemaphore(&GetPrivIBase(IntuitionBase)->InputHandlerLock);
        data->drag_inputhandlerlock = FALSE;
    }

    if (data->drag_windowlock)
    {
        ReleaseSemaphore(&GetPrivIBase(IntuitionBase)->WindowLock);
        data->drag_windowlock = FALSE;
    }
}

/***********************************************************************************/

IPTR DragBarClass__GM_GOACTIVE(Class *cl, struct Gadget *g, struct gpInput *msg)
{
    struct IntuitionBase *IntuitionBase = (struct IntuitionBase *)cl->cl_UserData;
    struct GfxBase    *GfxBase = GetPrivIBase(IntuitionBase)->GfxBase;
    struct LayersBase *LayersBase = GetPrivIBase(IntuitionBase)->LayersBase;
    struct InputEvent *ie = msg->gpi_IEvent;
    IPTR               retval = GMR_NOREUSE;

    if (ie)
    {
        /* The gadget was activated via mouse input */
        struct dragbar_data *data;
        struct Window       *w;

        /* There is no point in rerendering ourseleves here, as this is done
           by a call to RefreshWindowFrame() in the intuition inputhandler
        */

        w = msg->gpi_GInfo->gi_Window;

        data = INST_DATA(cl, g);

        /* do NOT ObtainSemaphore here since this would lead to deadlocks!!! */
        /* when the semaphore can not be obtained we simply ignore gadget activation */

        /* in movehack the task that locks windowlock is the one that calls MoveWindow*/
        if (!(AttemptSemaphore(&GetPrivIBase(IntuitionBase)->WindowLock)))
        {
            goto fail;
        }
        data->drag_windowlock = TRUE;

        data->curleft = w->LeftEdge;
        data->curtop  = w->TopEdge;

        data->startleft = w->LeftEdge;
        data->starttop = w->TopEdge;

        data->mousex = w->WScreen->MouseX - data->curleft;
        data->mousey = w->WScreen->MouseY - data->curtop;

        data->drag_refreshed = TRUE;

        data->rp = CloneRastPort(&w->WScreen->RastPort);
        if (data->rp)
        {

            /* Lock all layers while the window is dragged.
             * Get the gadget lock first to avoid deadlocks
             * with ObtainGIRPort. */

            if (!(GetPrivIBase(IntuitionBase)->IControlPrefs.ic_Flags & ICF_OPAQUEMOVE))
            {
                ObtainSemaphore(&GetPrivIBase(IntuitionBase)->InputHandlerLock);
                data->drag_inputhandlerlock = TRUE;

                if (AttemptSemaphore(&GetPrivIBase(IntuitionBase)->GadgetLock))
                {
                    data->drag_gadgetlock = TRUE;
                }
                else
                {
                    goto fail;
                }
                LockLayers(&w->WScreen->LayerInfo);
                data->drag_layerlock = TRUE;
            } else {
                /* MOVEHACK ? */
                if (GetPrivIBase(IntuitionBase)->IControlPrefs.ic_Flags & ICF_PRIVILEDGEDREFRESH)
                {
                // FIXME!
                /* FIXME: Implemente MOVEHACK support for AROS (?) */
                }
            }

            SetDrMd(data->rp, COMPLEMENT);

            if (!(GetPrivIBase(IntuitionBase)->IControlPrefs.ic_Flags & ICF_OPAQUEMOVE))
                dragbar_drawframe(data, w, IntuitionBase);

            data->isrendered = TRUE;

            data->drag_canceled = FALSE;

            data->lasteventtime = EventTicks(ie);

            /* size mouse bounds such that mouse pointer cannot move if window cannot move, if offscreenlayers is turned off */
            if (!(GetPrivIBase(IntuitionBase)->IControlPrefs.ic_Flags & ICF_OFFSCREENLAYERS)) {
                struct IIHData *iihd = (struct IIHData *)GetPrivIBase(IntuitionBase)->InputHandler->is_Data;
                iihd->MouseBoundsActiveFlag = TRUE;
                iihd->MouseBoundsLeft = data->mousex;
                iihd->MouseBoundsRight = w->WScreen->Width - (w->Width - data->mousex);
                iihd->MouseBoundsTop = data->mousey;
                iihd->MouseBoundsBottom = w->WScreen->Height - (w->Height - data->mousey);
            }

            return GMR_MEACTIVE;
        }

fail:
        dragbar_releaselocks(data, w, IntuitionBase);
    }

    return retval;

}

/***********************************************************************************/

/***********************************************************************************/

IPTR DragBarClass__GM_HANDLEINPUT(Class *cl, struct Gadget *g, struct gpInput *msg)
{
    struct IntuitionBase *IntuitionBase = (struct IntuitionBase *)cl->cl_UserData;
    struct GfxBase    *GfxBase = GetPrivIBase(IntuitionBase)->GfxBase;
    struct LayersBase *LayersBase = GetPrivIBase(IntuitionBase)->LayersBase;
    struct GadgetInfo *gi = msg->gpi_GInfo;
    IPTR               retval = GMR_MEACTIVE;

    if (gi)
    {
        struct InputEvent   *ie = msg->gpi_IEvent;
        struct dragbar_data *data = INST_DATA(cl, g);
        struct Window       *w = msg->gpi_GInfo->gi_Window;

        switch (ie->ie_Class)
        {
        case IECLASS_RAWMOUSE:
            switch (ie->ie_Code)
            {
            case MENUDOWN:
                retval = GMR_NOREUSE;
                data->drag_canceled = TRUE;
                break;

            case SELECTUP:
                retval = GMR_NOREUSE;
                break;

            case IECODE_NOBUTTON:
                {
                    struct Screen       *scr = w->WScreen;
                    struct InputEvent    myie;
                    LONG                 new_left;
                    LONG                 new_top;

                    /* Can we move to the new position, or is window at edge of display ? */
                    new_left = scr->MouseX - data->mousex;
                    new_top  = scr->MouseY - data->mousey;

                    CopyMem(ie,&myie,sizeof (struct InputEvent));
                    myie.ie_Code = SELECTDOWN; //mouse_leftpress

                    if (!((GetPrivIBase(IntuitionBase)->IControlPrefs.ic_Flags & ICF_OFFSCREENLAYERS) && (w->WScreen->LayerInfo.Flags & LIFLG_SUPPORTS_OFFSCREEN_LAYERS)))
                    {
                        if (new_left < 0)
                        {
                            data->mousex += new_left;
                            new_left = 0;
                        }

                        if (new_top < 0)
                        {
                            data->mousey += new_top;
                            new_top = 0;
                        }

                        if (new_left + w->Width > scr->Width)
                        {
                            LONG correct_left;
                            correct_left = scr->Width - w->Width; /* align to screen border */
                            data->mousex += new_left - correct_left;
                            new_left = correct_left;
                        }

                        if (new_top + w->Height > scr->Height)
                        {
                            LONG correct_top;
                            correct_top = scr->Height - w->Height; /* align to screen border */
                            data->mousey += new_top - correct_top;
                            new_top = correct_top;
                        }
                    }

                    if (data->curleft != new_left || data->curtop != new_top)
                    {
                        SetDrMd(data->rp, COMPLEMENT);

                        if ((data->isrendered) && (!(GetPrivIBase(IntuitionBase)->IControlPrefs.ic_Flags & ICF_OPAQUEMOVE)))
                        {
                            /* Erase old frame */
                            dragbar_drawframe(data, w, IntuitionBase);
                        }

                        data->curleft = new_left;
                        data->curtop  = new_top;

                        if (GetPrivIBase(IntuitionBase)->IControlPrefs.ic_Flags & ICF_OPAQUEMOVE)
                        {
                            WORD newx = new_left - w->LeftEdge, newy = new_top - w->TopEdge;

                            if (newx || newy)
                            {
                                ULONG currenttime = EventTicks(ie);

                                if (currenttime > data->lasteventtime + 10) //10 delay should result in intuitick freq
                                {
                                    ih_fire_intuimessage(w,
                                                         IDCMP_CHANGEWINDOW,
                                                         CWCODE_MOVESIZE,
                                                         w,
                                                         IntuitionBase);
                                    data->lasteventtime = currenttime;
                                }

                                {
                                    LockLayers(&w->WScreen->LayerInfo);

                                    w->LeftEdge    += newx;
                                    w->TopEdge     += newy;

                                    if (BLAYER(w))
                                    {
                                        MoveSizeLayer(BLAYER(w), newx, newy , 0, 0);
                                    }

                                    MoveSizeLayer(WLAYER(w), newx, newy, 0, 0);

                                    MoveRequesterLayers(w, data->curleft, data->curtop, w->Width, w->Height, IntuitionBase);

                                    CheckLayers(w->WScreen, IntuitionBase);

                                    UnlockLayers(&w->WScreen->LayerInfo);
                                }
                            }
                        }
                        else
                        {
                                /* Rerender the window frame */
                            dragbar_drawframe(data, w, IntuitionBase);
                            data->isrendered = TRUE;
                        }

                    }

                    retval = GMR_MEACTIVE;

                    break;
                }

            default:
                retval = GMR_NOREUSE;
                if (!(GetPrivIBase(IntuitionBase)->IControlPrefs.ic_Flags & ICF_OPAQUEMOVE)) data->drag_canceled = TRUE;
                break;

            } /* switch (ie->ie_Code) */
            break;

    case IECLASS_TIMER:

            break;
        } /* switch (ie->ie_Class) */

    } /* if (gi) */

    return retval;
}

/***********************************************************************************/

IPTR DragBarClass__GM_GOINACTIVE(Class *cl, struct Gadget *g, struct gpGoInactive *msg)
{
    struct IntuitionBase *IntuitionBase = (struct IntuitionBase *)cl->cl_UserData;
    struct GfxBase      *GfxBase = GetPrivIBase(IntuitionBase)->GfxBase;
    struct dragbar_data *data;
    struct Window       *w;

    data = INST_DATA(cl, g);

    w = msg->gpgi_GInfo->gi_Window;

    /* Always clear last drawn frame */

    if (data->isrendered && data->rp && (!(GetPrivIBase(IntuitionBase)->IControlPrefs.ic_Flags & ICF_OPAQUEMOVE)))
    {

        SetDrMd(data->rp, COMPLEMENT);

        /* Erase old frame */
        dragbar_drawframe(data, w, IntuitionBase);

    }

    data->isrendered = FALSE;

    if (!data->drag_refreshed) CheckLayers(w->WScreen, IntuitionBase);

    Forbid();
    Permit();

    if (!data->drag_canceled)// && !(GetPrivIBase(IntuitionBase)->IControlPrefs.ic_Flags & ICF_OPAQUEMOVE))
    {
        MoveWindow(w
                   , data->curleft - w->LeftEdge    /* dx */
                   , data->curtop  - w->TopEdge /* dy */
                  );

    }

    if (data->drag_canceled && (GetPrivIBase(IntuitionBase)->IControlPrefs.ic_Flags & ICF_OPAQUEMOVE))
    {
        MoveWindow(w
                   , data->startleft - w->LeftEdge /* dx */
                   , data->starttop - w->TopEdge /* dy */
                  );
    }

    ih_fire_intuimessage(w,
                         IDCMP_CHANGEWINDOW,
                         CWCODE_MOVESIZE,
                         w,
                         IntuitionBase);

    data->drag_canceled = TRUE;

    dragbar_releaselocks(data, w, IntuitionBase);

    /* User through with drag operation. Unlock layers and free
    rastport clone
    */

    if (data->rp)
    {
        FreeRastPort(data->rp);
        data->rp = NULL;
    }

    /* shut off mouse bounds checking.  */
    struct IIHData *iihd = (struct IIHData *)GetPrivIBase(IntuitionBase)->InputHandler->is_Data;
    iihd->MouseBoundsActiveFlag = FALSE;

    return TRUE;

}

/***********************************************************************************/

IPTR DragBarClass__NOP(Class *cl, Object *o, Msg msg)
{
    return (IPTR)0;
}

/***********************************************************************************/

IPTR DragBarClass__GM_HITTEST(Class *cl, Object *o, Msg msg)
{
    return (IPTR)1;
}

/***********************************************************************************/

IPTR DragBarClass__OM_NEW(Class *cl, Object *o, Msg msg)
{
    struct Gadget *g = (struct Gadget *)DoSuperMethodA(cl, o, msg);
    if (g)
    {
        g->GadgetType |= GTYP_SYSGADGET | GTYP_WDRAGGING;
    }

    return (IPTR)g;
}

/***********************************************************************************/

/*********************
** The SizeButtonClass
*********************/

/***********************************************************************************/

/***********************************************************************************/

/* Draw the frame that shows the size the window will get. It is drawn in
   COMPLEMENT mode, so drawing it again erases it. */

static void sizebutton_drawframe(struct sizebutton_data *data, struct Window *w,
                                 struct IntuitionBase *IntuitionBase)
{
    drawwindowframe(w->WScreen
                    , data->rp
                    , data->left
                    , data->top
                    , data->left + data->width  - 1
                    , data->top  + data->height - 1
                    , IntuitionBase
                    );
}

/***********************************************************************************/

/* Release the locks that are held while the user resizes a window */

static void sizebutton_releaselocks(struct sizebutton_data *data, struct Window *w,
                                    struct IntuitionBase *IntuitionBase)
{
    struct LayersBase *LayersBase = GetPrivIBase(IntuitionBase)->LayersBase;

    if (data->drag_layerlock)
    {
        UnlockLayers(&w->WScreen->LayerInfo);
        data->drag_layerlock = FALSE;
    }

    if (data->drag_gadgetlock)
    {
        ReleaseSemaphore(&GetPrivIBase(IntuitionBase)->GadgetLock);
        data->drag_gadgetlock = FALSE;
    }

    if (data->drag_inputhandlerlock)
    {
        ReleaseSemaphore(&GetPrivIBase(IntuitionBase)->InputHandlerLock);
        data->drag_inputhandlerlock = FALSE;
    }

    if (data->drag_windowlock)
    {
        ReleaseSemaphore(&GetPrivIBase(IntuitionBase)->WindowLock);
        data->drag_windowlock = FALSE;
    }
}

/***********************************************************************************/

IPTR SizeButtonClass__GM_GOACTIVE(Class *cl, struct Gadget *g, struct gpInput *msg)
{
    struct IntuitionBase *IntuitionBase = (struct IntuitionBase *)cl->cl_UserData;
    struct GfxBase      *GfxBase = GetPrivIBase(IntuitionBase)->GfxBase;
    struct LayersBase   *LayersBase = GetPrivIBase(IntuitionBase)->LayersBase;
    struct InputEvent   *ie = msg->gpi_IEvent;
    IPTR                 retval = GMR_NOREUSE;

    if (ie)
    {
        /* The gadget was activated via mouse input */
        struct sizebutton_data  *data;
        struct Window           *w;

        /* There is no point in rerendering ourseleves her, as this
           is done by a call to RefreshWindowFrame() in the intuition inputhandler
        */

        w = msg->gpi_GInfo->gi_Window;

        data = INST_DATA(cl, g);

        /* do NOT ObtainSemaphore here since this would lead to deadlocks!!! */
        /* when the semaphore can not be obtained we simply ignore gadget activation */
        if (!(AttemptSemaphore(&GetPrivIBase(IntuitionBase)->WindowLock)))
        {
            goto fail;
        }
        data->drag_windowlock = TRUE;

        data->height = data->Height = w->Height;
        data->width  = data->Width = w->Width;
        data->left = data->LeftEdge = w->LeftEdge;
        data->top = data->TopEdge = w->TopEdge;

        data->mouseoffsetx = w->WScreen->MouseX;
        data->mouseoffsety = w->WScreen->MouseY;

        data->drag_refreshed = TRUE;
        data->drag_ticks = 2;

        data->rp = CloneRastPort(&w->WScreen->RastPort);
        if (data->rp)
        {
            /* Lock all layers while the window is resized.
             * Get the gadget lock first to avoid deadlocks
             * with ObtainGIRPort. */

            if (!OPAQUESIZE)
            {
                ObtainSemaphore(&GetPrivIBase(IntuitionBase)->InputHandlerLock);
                data->drag_inputhandlerlock = TRUE;

                if (AttemptSemaphore(&GetPrivIBase(IntuitionBase)->GadgetLock))
                {
                    data->drag_gadgetlock = TRUE;
                }
                else
                {
                    goto fail;
                }
                LockLayers(&w->WScreen->LayerInfo);
                data->drag_layerlock = TRUE;
            }

            SetDrMd(data->rp, COMPLEMENT);

            if(!OPAQUESIZE)
                sizebutton_drawframe(data, w, IntuitionBase);

            data->isrendered = TRUE;

            data->lasteventtime = EventTicks(ie);

            data->drag_canceled = FALSE;

            /* size mouse bounds such that mouse pointer cannot move if window cannot size, if offscreenlayers is turned off */
            if (!(GetPrivIBase(IntuitionBase)->IControlPrefs.ic_Flags & ICF_OFFSCREENLAYERS)) {
                struct IIHData *iihd = (struct IIHData *)GetPrivIBase(IntuitionBase)->InputHandler->is_Data;
                LONG mousex = data->mouseoffsetx - data->LeftEdge;
                LONG mousey = data->mouseoffsety - data->TopEdge;

                iihd->MouseBoundsActiveFlag = TRUE;
                iihd->MouseBoundsLeft = 0;
                iihd->MouseBoundsRight = w->WScreen->Width - (w->Width - mousex);
                iihd->MouseBoundsTop = 0;
                iihd->MouseBoundsBottom = w->WScreen->Height - (w->Height - mousey);
            }

            return GMR_MEACTIVE;
        }

fail:
        sizebutton_releaselocks(data, w, IntuitionBase);
    }

    return retval;

}

/***********************************************************************************/

IPTR SizeButtonClass__GM_HANDLEINPUT(Class *cl, struct Gadget *g, struct gpInput *msg)
{
    struct IntuitionBase *IntuitionBase = (struct IntuitionBase *)cl->cl_UserData;
    struct GfxBase      *GfxBase = GetPrivIBase(IntuitionBase)->GfxBase;
    struct GadgetInfo   *gi = msg->gpi_GInfo;
    IPTR                 retval = GMR_MEACTIVE;

    if (gi)
    {
        struct InputEvent       *ie = msg->gpi_IEvent;
        struct sizebutton_data  *data = INST_DATA(cl, g);
        struct Window           *w = msg->gpi_GInfo->gi_Window;

        switch (ie->ie_Class)
        {
        case IECLASS_RAWMOUSE:
            switch (ie->ie_Code)
            {
            case SELECTUP:
                retval = GMR_NOREUSE;
                break;

            case IECODE_NOBUTTON:
                {
                    struct Screen   *scr = w->WScreen;
                    LONG             new_width = 0;
                    LONG             new_height = 0;

                    /* Can we move to the new position, or is window at edge of display ? */
                    new_width   = data->Width + scr->MouseX - data->mouseoffsetx;
                    new_height  = data->Height + scr->MouseY - data->mouseoffsety;
                    if (new_width < 0)
                        new_width = 1;

                    if (w->MinWidth != 0 && new_width < (ULONG)w->MinWidth)
                        new_width = w->MinWidth;

                    if (w->MaxWidth != 0 && new_width > (ULONG)w->MaxWidth)
                        new_width = w->MaxWidth;

                    if (new_height < 0)
                        new_height = 1;

                    if (w->MinHeight != 0 && new_height < (ULONG)w->MinHeight)
                        new_height = w->MinHeight;

                    if (w->MaxHeight != 0 && new_height > (ULONG)w->MaxHeight)
                        new_height = w->MaxHeight;

                    if (data->height != new_height || data->width != new_width)
                    {
                        SetDrMd(data->rp, COMPLEMENT);

                        if (data->isrendered && !OPAQUESIZE)
                        {
                            /* Erase old frame */
                            sizebutton_drawframe(data, w, IntuitionBase);

                        }

                        data->width   = new_width;
                        data->height  = new_height;

                        /* Rerender the window frame */

                        data->drag_refreshed = FALSE;
                        data->drag_ticks = 2;

                        if (!OPAQUESIZE)
                            sizebutton_drawframe(data, w, IntuitionBase);

                        data->isrendered = TRUE;

                    }

                    retval = GMR_MEACTIVE;

                    break;
                }

            default:
                retval = GMR_NOREUSE;
                data->drag_canceled = TRUE;
                break;

            } /* switch (ie->ie_Code) */
            break;

        case IECLASS_TIMER:

            break;

        } /* switch (ie->ie_Class) */

    } /* if (gi) */

    return retval;
}

/***********************************************************************************/

IPTR SizeButtonClass__GM_GOINACTIVE(Class *cl, struct Gadget *g, struct gpGoInactive *msg)
{
    struct IntuitionBase    *IntuitionBase = (struct IntuitionBase *)cl->cl_UserData;
    struct GfxBase          *GfxBase = GetPrivIBase(IntuitionBase)->GfxBase;
    struct sizebutton_data  *data;
    struct Window           *w;

    data = INST_DATA(cl, g);
    w = msg->gpgi_GInfo->gi_Window;

    /* Allways clear last drawn frame */

    if (data->isrendered && data->rp)
    {

        SetDrMd(data->rp, COMPLEMENT);

        /* Erase old frame */
        if (!OPAQUESIZE)
            sizebutton_drawframe(data, w, IntuitionBase);

    }
    else
    {
        if (!OPAQUESIZE)
        {
            data->drag_canceled = TRUE;
        }
    }
    data->isrendered = FALSE;

    sizebutton_releaselocks(data, w, IntuitionBase);

    //jDc: workarounds refresh pb on GZZ window resize
    ((struct IIHData *)GetPrivIBase(IntuitionBase)->InputHandler->is_Data)->ActiveSysGadget->Flags &= ~GFLG_SELECTED;

    if (!data->drag_canceled || OPAQUESIZE)
    {
        if (OPAQUESIZE && data->drag_canceled)
        {
            DoMoveSizeWindow(w,data->LeftEdge,data->TopEdge,data->Width,data->Height,TRUE,IntuitionBase);
        }
        else
        {
            DoMoveSizeWindow(w,data->left,data->top,data->width,data->height,TRUE,IntuitionBase);
        }
        //ChangeWindowBox(w,data->left,data->top,data->width,data->height);
    }
    data->drag_canceled = TRUE;

    /* User through with drag operation. Unlock layers and free
       rastport clone
    */
    if (data->rp)
    {
        FreeRastPort(data->rp);
        data->rp = NULL;
    }

    /* shut off mouse bounds checking.  */
    ((struct IIHData *)GetPrivIBase(IntuitionBase)->InputHandler->is_Data)->MouseBoundsActiveFlag = FALSE;

    return TRUE;
}

/***********************************************************************************/

IPTR SizeButtonClass__OM_NEW(Class *cl, Object *o, Msg msg)
{
    struct Gadget *g = (struct Gadget *)DoSuperMethodA(cl, o, msg);
    if (g)
    {
        g->GadgetType |= GTYP_SYSGADGET | GTYP_SIZING;
    }

    return (IPTR)g;
}

/***********************************************************************************/
