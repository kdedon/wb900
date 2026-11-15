/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.
    Copyright (C) 2001-2003, The MorphOS Development Team. All Rights Reserved.
*/

#include <dos/dos.h>
#include <dos/dosextens.h>

#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/scrdecorclass.h>
#include <intuition/cghooks.h>
#include <intuition/icclass.h>
#include <intuition/extensions.h>

#include <graphics/gfxbase.h>
#include <graphics/gfxmacros.h>

#include <utility/tagitem.h>
#include <utility/hooks.h>

#include <clib/macros.h>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/utility.h>

#include <proto/alib.h>

#include "intuition_intern.h"
#include "gadgets.h"
#include "decorsupport.h"

/**************************************************************************************************/

#define DEFSIZE_WIDTH  14
#define DEFSIZE_HEIGHT 14

#define HSPACING 3
#define VSPACING 3
/* Ralph Schmidt
 * heuristics for smaller arrows used in apps
 * like filer
 */
#define HSPACING_MIDDLE 2
#define VSPACING_MIDDLE 2
#define HSPACING_SMALL 1
#define VSPACING_SMALL 1

#define DRI(dri) ((struct DrawInfo *)(dri))

/**************************************************************************************************/

/**************************************************************************************************/

IPTR ScrDecorClass__OM_NEW(Class *cl, Object *obj, struct opSet *msg)
{
    return decor_om_new(cl, obj, msg, SDA_UserBuffer);
}

/**************************************************************************************************/

IPTR ScrDecorClass__OM_GET(Class *cl, Object *obj, struct opGet *msg)
{
    return decor_om_get(cl, obj, msg, SDA_UserBuffer, SDA_TrueColorOnly);
}

/**************************************************************************************************/

IPTR ScrDecorClass__SDM_GETDEFSIZE_SYSIMAGE(Class *cl, Object *obj, struct sdpGetDefSizeSysImage *msg)
{
    ULONG def_low_width = DEFSIZE_WIDTH, def_low_height = DEFSIZE_HEIGHT;
    ULONG def_med_width = DEFSIZE_WIDTH, def_med_height = DEFSIZE_HEIGHT;
    ULONG def_high_width = DEFSIZE_WIDTH, def_high_height = DEFSIZE_HEIGHT;

    switch(msg->sdp_Which)
    {
        case SDEPTHIMAGE:
            def_low_width = def_med_width = def_high_width = DEFSIZE_WIDTH;
            def_low_height = def_med_height = def_high_height = DEFSIZE_HEIGHT;
            break;

        default:
            return FALSE;
    }

    switch(msg->sdp_SysiSize)
    {
        case SYSISIZE_LOWRES:
            *msg->sdp_Width = def_low_width;
            *msg->sdp_Height = def_low_height;
            break;

        case SYSISIZE_MEDRES:
            *msg->sdp_Width = def_med_width;
            *msg->sdp_Height = def_med_height;
            break;

        case SYSISIZE_HIRES:
        default:
            *msg->sdp_Width = def_high_width;
            *msg->sdp_Height = def_high_height;
            break;
    }

    return TRUE;
}

/**************************************************************************************************/

IPTR ScrDecorClass__SDM_DRAW_SYSIMAGE(Class *cl, Object *obj, struct sdpDrawSysImage *msg)
{
    struct IntuitionBase        *IntuitionBase = (struct IntuitionBase *)cl->cl_UserData;
    struct GfxBase              *GfxBase = GetPrivIBase(IntuitionBase)->GfxBase;
    struct RastPort             *rp = msg->sdp_RPort;
    UWORD                       *pens = DRI(msg->sdp_Dri)->dri_Pens;
    LONG                        state = msg->sdp_State;
    LONG                        left = msg->sdp_X;
    LONG                        top = msg->sdp_Y;
    LONG                        width = msg->sdp_Width;
    LONG                        height = msg->sdp_Height;
    LONG                        right = left + width - 1;
    LONG                        bottom = top + height - 1;
    UWORD                       frameh, framev;

    getintframesizes(IntuitionBase, &frameh, &framev);

    SetDrMd(rp, JAM1);

    switch(msg->sdp_Which)
    {
        case SDEPTHIMAGE:
        {
            UWORD  bg;

            renderimageframe(rp, DEPTHIMAGE, state, pens,
                             left, top, width, height, IntuitionBase);
            left++;
            top++;
            right--;
            bottom--;
            width -= (frameh << 1);
            height -= (framev << 1);

            bg = pens[BACKGROUNDPEN];

            /* Clear background into correct color */
            SetAPen(rp, bg);
            RectFill(rp, left, top, right, bottom);

            /*
             * render an image of two partially
             * overlapping window frames.
             */

            drawdepthwindows(rp, pens, state, TRUE, left, top, width, height, IntuitionBase);
            break;
        }

        default:
            return FALSE;
    }

    return TRUE;
}

/**************************************************************************************************/

static void findtitlearea(struct Screen *scr, LONG *left, LONG *right)
{
    struct Gadget *g;

    *left = 0;
    *right = scr->Width - 1;

    for (g = scr->FirstGadget; g; g = g->NextGadget)
    {
        if (!(g->Flags & GFLG_RELRIGHT))
        {
            if (g->LeftEdge + g->Width > *left)
                *left = g->LeftEdge + g->Width;
        }
        else
        {
            if (g->LeftEdge + scr->Width - 1 - 1 < *right)
                *right = g->LeftEdge + scr->Width - 1 - 1;
        }
    }

}

/**************************************************************************************************/

/**************************************************************************************************/

IPTR ScrDecorClass__SDM_DRAW_SCREENBAR(Class *cl, Object *obj, struct sdpDrawScreenBar *msg)
{
    struct IntuitionBase *IntuitionBase = (struct IntuitionBase *)cl->cl_UserData;
    struct GfxBase       *GfxBase = GetPrivIBase(IntuitionBase)->GfxBase;
    struct RastPort      *rp = msg->sdp_RPort;
    UWORD                *pens = DRI(msg->sdp_Dri)->dri_Pens;
    LONG                  right, left;
    BOOL                  beeping = FALSE;
    BOOL                  atomicTitle = FALSE;
    TEXT                  titleLine[128];
    ULONG                 titleChars = 0;
    ULONG                 len;
    LONG                  titleWidth;

    beeping = msg->sdp_Screen->Flags & BEEPING;

    findtitlearea(msg->sdp_Screen, &left, &right);

    titleWidth = right - msg->sdp_Screen->BarHBorder;
    if (titleWidth >= 8)
    {
        titleChars = titleWidth / 8;
        if (titleChars > sizeof(titleLine))
            titleChars = sizeof(titleLine);
    }

    if ((msg->sdp_Flags & SDF_DSB_TITLEONLY) && rp->Font &&
        !rp->Font->tf_CharKern && !rp->Font->tf_CharSpace &&
        rp->Font->tf_XSize == 8 && rp->TxSpacing == 0 &&
        !(rp->AlgoStyle & (FSF_BOLD | FSF_ITALIC | FSF_UNDERLINED)))
    {
        if (titleChars)
        {
            memset(titleLine, ' ', titleChars);
            if (msg->sdp_Screen->Title)
            {
                len = 0;
                while ((len < titleChars) && msg->sdp_Screen->Title[len])
                    len++;
                memcpy(titleLine, msg->sdp_Screen->Title, len);
            }
            atomicTitle = TRUE;
        }
    }

    SetDrMd(rp, JAM1);

    if (!atomicTitle)
    {
        SetAPen(rp, pens[beeping ? BARDETAILPEN : BARBLOCKPEN]);
        RectFill(rp, left + 1, 0, right - 1,
                 msg->sdp_Screen->BarHeight - 1);
    }

    SetAPen(rp, pens[beeping ? BARDETAILPEN : BARTRIMPEN]);
    RectFill(rp, 0, msg->sdp_Screen->BarHeight, msg->sdp_Screen->Width - 1, msg->sdp_Screen->BarHeight);

    if (msg->sdp_Screen->Title || atomicTitle)
    {
        UWORD ypad, xunused;

        getintframesizes(IntuitionBase, &xunused, &ypad);

        SetAPen(rp, pens[beeping ? BARBLOCKPEN: BARDETAILPEN]);
        SetBPen(rp, pens[beeping ? BARDETAILPEN : BARBLOCKPEN]);

        if (atomicTitle)
            SetDrMd(rp, JAM2);

        Move(rp, msg->sdp_Screen->BarHBorder, ypad + rp->TxBaseline + ((msg->sdp_Screen->BarHeight - rp->TxHeight) >> 1));

        if (atomicTitle)
            Text(rp, titleLine, titleChars);
        else
        {
            for (len = 0; (len < titleChars) && msg->sdp_Screen->Title[len]; len++)
                ;
            Text(rp, msg->sdp_Screen->Title, len);
        }

    }
    return TRUE;
}

/**************************************************************************************************/

IPTR ScrDecorClass__SDM_LAYOUT_SCREENGADGETS(Class *cl, Object *obj, struct sdpLayoutScreenGadgets *msg)
{

    /*
     * Nothing to do here any more.
     * Proper default positioning is done by OpenScreen(). Use this method
     * only if you actually want to change something.
     */

    return TRUE;
}

IPTR ScrDecorClass__SDM_INITSCREEN(Class *cl, Object *obj, struct sdpInitScreen *msg)
{

    return TRUE;
}

IPTR ScrDecorClass__SDM_EXITSCREEN(Class *cl, Object *obj, struct sdpExitScreen *msg)
{

    return TRUE;
}

/**************************************************************************************************/
