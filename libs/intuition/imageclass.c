/*
    Copyright (C) 1995-2020, The AROS Development Team. All rights reserved.
    Copyright (C) 2001-2003, The MorphOS Development Team. All Rights Reserved.
*/

#include <exec/types.h>

#include <dos/dos.h>
#include <dos/dosextens.h>

#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/imageclass.h>
#include <intuition/cghooks.h>
#include <intuition/icclass.h>

#include <graphics/gfxbase.h>
#include <graphics/gfxmacros.h>

#include <utility/tagitem.h>
#include <utility/hooks.h>

#include <clib/macros.h>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/utility.h>

#include "intuition_intern.h"
#include <aros/asmcall.h>
#include <proto/alib.h>

/***********************************************************************************/

#define DEBUG_HIT(x)    ;

/***********************************************************************************/

IPTR _om_set(Class *cl, struct Image *im, struct TagItem *tags)
{
    struct IntuitionBase *IntuitionBase = (struct IntuitionBase *)cl->cl_UserData;
    struct Library *UtilityBase = GetPrivIBase(IntuitionBase)->UtilityBase;
    struct TagItem *tstate = tags;
    struct TagItem *tag;
    IPTR   tidata;
    BOOL   unsupported = FALSE;

    while ((tag = NextTagItem(&tstate)))
    {
        tidata = tag->ti_Data;

        switch (tag->ti_Tag)
        {
        case IA_Left:
            im->LeftEdge = (WORD) tidata;
            break;

        case IA_Top:
            im->TopEdge = (WORD) tidata;
            break;

        case IA_Width:
            im->Width = (WORD) tidata;
            break;

        case IA_Height:
            im->Height = (WORD) tidata;
            break;

        case IA_FGPen:
            im->PlanePick = (WORD) tidata;
            break;

        case IA_BGPen:
            im->PlaneOnOff = (WORD) tidata;
            break;

        case IA_Data:
            im->ImageData = (UWORD *) tidata;
            break;

        default:
            unsupported = TRUE;
            break;

        } /* switch (Tag) */

    } /* while (Tag) */

    /*
     * If all attributes were supported and there is no retval yet,
     * set retval to 1.
     */
    if (!unsupported)
        return (IPTR)1;
    else
        return (IPTR)0;
}

IPTR ImageClass__OM_NEW(Class *cl, Object *o, struct opSet *msg)
{
    struct Image *im;

    if (cl)
    {
        im = (struct Image *)DoSuperMethodA(cl, o, (Msg)msg);

        if(im)
        {
            /*
             * This is how Intuition knows an image is a boopsi
             * object!
             */
            /*
             * The instance object is contains cleared memory!
             * SetMem ((void *)retval, 0, (cl->cl_InstOffset + cl->cl_InstSize));
             */
            im->Width = 80;
            im->Height = 40;
            im->Depth = CUSTOMIMAGEDEPTH;

            _om_set(cl, im, msg->ops_AttrList);

            return (IPTR)im;
        }
        else
            return (IPTR)NULL;
    }
    else
        return (IPTR)NULL;
}

IPTR ImageClass__OM_SET(Class *cl, struct Image *im, struct opSet *msg)
{

    /*
     * Because we are a direct subclass of rootclass
     * which has no settable/gettable attributes we
     * we will NOT pass this method to our superclass!
     */
    return _om_set(cl, im, msg->ops_AttrList);
}

IPTR ImageClass__OM_GET(Class *cl, struct Image *im, struct opGet *msg)
{

    switch (msg->opg_AttrID)
    {
    case IA_Left:
        *msg->opg_Storage = (IPTR) im->LeftEdge;
        break;

    case IA_Top:
        *msg->opg_Storage = (IPTR) im->TopEdge;
        break;

    case IA_Width:
        *msg->opg_Storage = (IPTR) im->Width;
        break;

    case IA_Height:
        *msg->opg_Storage = (IPTR) im->Height;
        break;

    case IA_FGPen:
        *msg->opg_Storage = (IPTR) im->PlanePick;
        break;

    case IA_BGPen:
        *msg->opg_Storage = (IPTR) im->PlaneOnOff;
        break;

    case IA_Data:
        *msg->opg_Storage = (IPTR) im->ImageData;
        break;

    default:
        return (IPTR)0;
    } /* switch */

    /*
     * Because we are a direct subclass of rootclass
     * which has no settable/gettable attributes we
     * we will NOT pass this method to our superclass!
     */
    return (IPTR)1;
}

IPTR ImageClass__IM_ERASE(Class *cl, struct Image *im, struct impErase *msg)
{
    /*
     * Both erase methods are documented as handled the same
     * at this level, so we will fall thru...
     */
    struct IntuitionBase *IntuitionBase = (struct IntuitionBase *)cl->cl_UserData;
    struct GfxBase *GfxBase = GetPrivIBase(IntuitionBase)->GfxBase;
    WORD left, top, width, height;

    left   = im->LeftEdge + msg->imp_Offset.X;
    top    = im->TopEdge + msg->imp_Offset.Y;
    width  = im->Width - 1;
    height = im->Height - 1;

    if (msg->imp_RPort)
    {
        EraseRect(msg->imp_RPort,
                  left, top,
                  left + width, top + height
        );
    }

    return (IPTR)0;
}

IPTR ImageClass__IM_HITTEST(Class *cl, struct Image *im, struct impHitTest *imp)
{
    int hit;

    /*
     * Loosing my sanity, better check that I do not
     * have my X/Y mixed up here. :)
     */
    hit = (imp->imp_Point.X >= im->LeftEdge && imp->imp_Point.X < im->LeftEdge + im->Width)
          && (imp->imp_Point.Y >= im->TopEdge  && imp->imp_Point.Y < im->TopEdge + im->Height)
    ;

    return (IPTR)hit;
}

/***********************************************************************************/
