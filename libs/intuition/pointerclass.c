/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.
    Copyright (C) 2001-2003, The MorphOS Development Team. All Rights Reserved.
*/

#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/pointerclass.h>
#include <graphics/sprite.h>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/utility.h>

#include "intuition_intern.h"

#include <aros/asmcall.h>

/***********************************************************************************/

/* Empty sprite */
const UWORD posctldata[] =
{
    0x0000,0x0000, /* posctl                 */
    0x0000,0x0000, /* Empty pixels, one line */
    0x0000,0x0000  /* Reserved               */
};

/***********************************************************************************/

IPTR PointerClass__OM_NEW(Class *cl, Object *o, struct opSet *msg)
{
    struct IntuitionBase *IntuitionBase = (struct IntuitionBase *)cl->cl_UserData;
    struct GfxBase *GfxBase = GetPrivIBase(IntuitionBase)->GfxBase;
    struct Library *UtilityBase = GetPrivIBase(IntuitionBase)->UtilityBase;

    if (cl)
    {
        struct TagItem spritetags[] =
        {
            {SPRITEA_Width        , 0   },
            {SPRITEA_OutputHeight , 0   },
            {SPRITEA_OldDataFormat, TRUE},
            {TAG_DONE               }
        };
        struct ExtSprite *sprite;
        struct TagItem *tags = msg->ops_AttrList;
        struct BitMap *bitmap = (struct BitMap *)GetTagData(POINTERA_BitMap, (IPTR)NULL, tags);

        /* Hack: we accept and pass on to AllocSpriteDataA() these attributes in order to
                 provide code reuse. Applications should not rely on it, this is private! */
        BOOL oldbitmap = GetTagData(SPRITEA_OldDataFormat, FALSE, tags);
        IPTR width = GetTagData(SPRITEA_Width, 16, tags);
        IPTR height = GetTagData(SPRITEA_OutputHeight, 0, tags);

        //ULONG xResolution = GetTagData(POINTERA_XResolution, POINTERXRESN_DEFAULT, tags);
        //ULONG yResolution = GetTagData(POINTERA_YResolution, POINTERYRESN_DEFAULT, tags);

        if (bitmap)
        {
            if (oldbitmap) {
                spritetags[0].ti_Data = width;
                spritetags[1].ti_Data = height;
            } else {
                spritetags[0].ti_Data = GetTagData(POINTERA_WordWidth, ((GetBitMapAttr(bitmap, BMA_WIDTH) + 15) & ~15)>>4, tags) * 16;
                spritetags[1].ti_Tag = TAG_DONE;
            }
        }
        else
        {

            spritetags[0].ti_Data = 16;
            spritetags[1].ti_Data = 1;
            bitmap = (struct BitMap *)posctldata;
        }

        sprite = AllocSpriteDataA(bitmap, spritetags);

        if (sprite)
        {
            struct SharedPointer *shared;

            shared = CreateSharedPointer(sprite,
                                         GetTagData(POINTERA_XOffset, 0, tags),
                                         GetTagData(POINTERA_YOffset, 0, tags),
                                         IntuitionBase);

            if (shared)
            {
                o = (Object *)DoSuperMethodA(cl, o, (Msg)msg);

                if (o)
                {
                    struct PointerData *data = INST_DATA(cl, o);

                    data->shared_pointer = shared;
                    //data->xRes = xResolution;
                    //data->yRes = yResolution;

                }
                else
                {

                    ReleaseSharedPointer(shared, IntuitionBase);
                }
                return (IPTR)o;
            }
            else
            {

                FreeSpriteData(sprite);
            }
        }
    }
    return (IPTR)NULL;
}

/***********************************************************************************/

IPTR PointerClass__OM_GET(Class *cl, Object *o, struct opGet *msg)
{
    struct PointerData *data = INST_DATA(cl, o);

    switch (msg->opg_AttrID)
    {
    case POINTERA_SharedPointer:

        *msg->opg_Storage = (IPTR)data->shared_pointer;
        break;

    case POINTERA_XOffset:

        *msg->opg_Storage = data->shared_pointer->xoffset;
        break;

    case POINTERA_YOffset:

        *msg->opg_Storage = data->shared_pointer->yoffset;
        break;

    default:
        return DoSuperMethodA(cl, o, (Msg)msg);
    }

    return (IPTR)1;
}

/***********************************************************************************/

IPTR PointerClass__OM_DISPOSE(Class *cl, Object *o, Msg msg)
{
    struct IntuitionBase *IntuitionBase = (struct IntuitionBase *)cl->cl_UserData;
    struct PointerData *data = INST_DATA(cl, o);

    ReleaseSharedPointer(data->shared_pointer, IntuitionBase);

    return DoSuperMethodA(cl, o, msg);
}

/***********************************************************************************/
