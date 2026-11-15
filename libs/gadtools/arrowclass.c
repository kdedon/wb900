/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Internal GadTools arrow class.
*/
 

#include <proto/exec.h>
#include <exec/libraries.h>
#include <exec/memory.h>
#include <proto/dos.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/gadgetclass.h>
#include <intuition/imageclass.h>
#include <intuition/intuition.h>
#include <intuition/cghooks.h>
#include <graphics/rastport.h>
#include <graphics/text.h>
#include <utility/tagitem.h>
#include <devices/inputevent.h>
#include <proto/alib.h>
#include <proto/utility.h>



#include "gadtools_intern.h"

/**********************************************************************************************/

#define GadToolsBase ((struct GadToolsBase_intern *)cl->cl_UserData)

/**********************************************************************************************/

Object *GTArrow__OM_NEW(Class * cl, Object * o, struct opSet *msg)
{
     struct DrawInfo    *dri = (struct DrawInfo *)GetTagData(GA_DrawInfo, (IPTR) NULL, msg->ops_AttrList);
     Object             *frame = NULL, *arrowimage = NULL;
    struct TagItem      itags[] =
    {
         {SYSIA_Which     , 0                   },
         {SYSIA_DrawInfo  , 0                   },
         {IA_Left         , 0                   },
         {IA_Top          , 0                   },
         {IA_Width        , 0                   },
         {IA_Height       , 0                   },
         {SYSIA_WithBorder, FALSE               },
         {SYSIA_Style     , SYSISTYLE_GADTOOLS  },
         {TAG_DONE                              }
    };
    struct TagItem      atags[] =
    {
         {GA_LabelImage , 0UL                   },
         {GA_Image      , 0UL                   },
         {TAG_MORE                              }
    };
    WORD                arrowtype;
 
    
    itags[4].ti_Data = GetTagData(GA_Width, 0, msg->ops_AttrList);
    itags[5].ti_Data = GetTagData(GA_Height, 0, msg->ops_AttrList);

    
    frame = gt_newframe(GadToolsBase, itags[4].ti_Data, itags[5].ti_Data,
                        ((IPTR)dri->dri_Resolution.X << 16) + dri->dri_Resolution.Y,
                        FRAME_BUTTON, TAG_DONE);
    if (!frame)
        return NULL;


        
    itags[0].ti_Data = arrowtype = GetTagData(GTA_Arrow_Type, LEFTIMAGE, msg->ops_AttrList);
    itags[1].ti_Data = (IPTR)dri;
    
    arrowimage = NewObjectA(NULL, SYSICLASS, itags);
    if (!arrowimage)
        goto failure;
    
    #define IM(o) ((struct Image *)o)
        
    atags[0].ti_Data = (IPTR)arrowimage;
    atags[1].ti_Data = (IPTR)frame;
    atags[2].ti_Data = (IPTR)msg->ops_AttrList;
    
    o = (Object *)DoSuperMethod(cl, o, OM_NEW, (IPTR) atags, (IPTR) NULL);
    if (o)
    {
        struct ArrowData *data = INST_DATA(cl, o);
        
        data->gadgetkind = GetTagData(GTA_GadgetKind, 0, msg->ops_AttrList);
        data->arrowtype = arrowtype;
        data->scroller = (Object *)GetTagData(GTA_Arrow_Scroller, (IPTR) NULL,  msg->ops_AttrList);
        if (!data->scroller)
            goto failure;
            
        data->arrowimage = arrowimage;
        data->frame      = frame;
        
    }
    return o;
    
failure:
    if (frame)
        DisposeObject(frame);
    if (arrowimage)
        DisposeObject(arrowimage);
    if (o)
        CoerceMethod(cl, o, OM_DISPOSE);
    
    return NULL;
    
}

/**********************************************************************************************/

IPTR GTArrow__OM_GET(Class * cl, Object * o, struct opGet *msg)
{
    struct ArrowData    *data = INST_DATA(cl, o);
    IPTR                retval;
    
    switch(msg->opg_AttrID)
    {
        case GTA_GadgetKind:
            *(msg->opg_Storage) = data->gadgetkind;
            retval = 1UL;
            break;

        case GTA_ChildGadgetKind:
            *(msg->opg_Storage) = _ARROW_KIND;
            retval = 1UL;
            break;

        case GTA_Arrow_Type:
            *(msg->opg_Storage) = data->arrowtype;
            retval = 1UL;
            break;

        case GTA_Arrow_Scroller:
            *(msg->opg_Storage) = (IPTR)data->scroller;
            retval = 1UL;
            break;

        default:
            retval = DoSuperMethodA(cl, o, (Msg)msg);
            break;
    }
    
    return retval;
}

/**********************************************************************************************/

IPTR GTArrow__OM_DISPOSE(Class * cl, Object * o, Msg msg)
{
    struct ArrowData *data = INST_DATA(cl, o);

    if (data->frame) DisposeObject(data->frame);
    if (data->arrowimage) DisposeObject(data->arrowimage);

    return DoSuperMethodA(cl, o, msg);
}

/**********************************************************************************************/
