/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.
 
    Internal GadTools text class (NUMERIC_KIND and TEXT_KIND) .
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

#define TEXTF_CLIPPED   (1 << 0)
#define TEXTF_BORDER    (1 << 1)
#define TEXTF_COPYTEXT  (1 << 2)

/**********************************************************************************************/

STATIC IPTR text_set(Class * cl, Object * o, struct opSet * msg)
{
    IPTR                retval = 0UL;
    struct TagItem      *tag, *tstate;
    struct TextData     *data = INST_DATA(cl, o);
    
    
    tstate = msg->ops_AttrList;
    
    while ((tag = NextTagItem(&tstate)))
    {
        IPTR tidata = tag->ti_Data;
        
        switch (tag->ti_Tag)
        {
            case GTA_GadgetKind:
                data->gadgetkind = (WORD)tidata;
                break;

            case GTNM_Number:
                data->toprint = tidata;
                if (data->dispfunc)
                {
                    struct Gadget *g;
                    
                    g = data->parentgadget ? data->parentgadget : (struct Gadget *)o;
                    data->toprint = data->dispfunc(g, (WORD)data->toprint);
                }
                retval = 1UL;
                break;
            
            case GTTX_Text:
                /* If the user has GT_SetGadgetAttrs() us to a different text,
                ** then don't copy it anymore
                */
                if (msg->MethodID != OM_NEW)
                {
                    if (data->flags & TEXTF_COPYTEXT)
                    {
                        FreeVec((APTR)data->toprint);
                        data->flags &= ~TEXTF_COPYTEXT;
                    }
                    data->toprint = tidata;
                }
                retval = 1UL;
                break;
            
            case GTTX_Border:   /* [I]  */
            case GTNM_Border:   /* [I]  */
                if (tidata)
                    data->flags |= TEXTF_BORDER;
                    
                break;
                

            /*case GTTX_FrontPen:  [IS] */
            case GTNM_FrontPen: /* [IS] */
                data->frontpen = (UBYTE)tidata;
                retval = 1UL;
                break;
                
            /* case GTTX_BackPen: [IS]  */
            case GTNM_BackPen:  /* [IS] */
                data->backpen = (UBYTE)tidata;
                retval = 1UL;
                break;

            /* case GTTX_Justification:  [I] */
            case GTNM_Justification:    /* [I] */
                data->justification = (UBYTE)tidata;
                break;
                
            case GTNM_Format:   /* [I]  */
            case GTA_Text_Format:
                data->format = (STRPTR)tidata;
                break;
                
            /* case GTTX_Clipped:        [I]    */
            case GTNM_Clipped:
                if (tidata)
                    data->flags |= TEXTF_CLIPPED;
                break;
                
            case GTNM_MaxNumberLen:     /* [I]  */
                data->maxnumberlength = tidata;
                break;
            
        } /* switch() */
    
    } /* while (iterate taglist) */
    
    /* Redraw the gadget, if an attribute was changed and if this is the
       objects' base-class. */
    if ((retval) && (OCLASS(o) == cl)) {
        if (gt_render(GadToolsBase, o, msg->ops_GInfo, GREDRAW_REDRAW))
            retval = FALSE;
    }

    return retval;
}

/**********************************************************************************************/

IPTR GTText__OM_GET(Class * cl, Object * o, struct opGet *msg)
{
    struct TextData     *data = INST_DATA(cl, o);
    IPTR                retval;
    
    switch (msg->opg_AttrID)
    {
        case GTA_GadgetKind:
        case GTA_ChildGadgetKind:
            *(msg->opg_Storage) = data->gadgetkind;
            retval = 1UL;
            break;

        case GTTX_Text:
        case GTNM_Number:
            *(msg->opg_Storage) = data->toprint;
            retval = 1UL;
            break;

        default:
            retval = DoSuperMethodA(cl, o, (Msg)msg);
            break;
    }
    
    return retval;
}

/**********************************************************************************************/

IPTR GTText__OM_NEW(Class * cl, Object * o, struct opSet *msg)
{

    o = (Object *) DoSuperMethodA(cl, o, (Msg)msg);
    if (o)
    {
        struct TextData *data = INST_DATA(cl, o);
        struct TextAttr *tattr, def_tattr;
        
        /* Set some defaults */
        data->format    = "%ld";
        data->flags     = 0;
        data->frontpen  = TEXTPEN;
        data->backpen   = BACKGROUNDPEN;
        data->toprint   = (IPTR) NULL;
        data->font      = NULL;
        data->maxnumberlength = 0; /* This means "no limit" */
        data->dispfunc = (APTR)GetTagData(GTA_Text_DispFunc, (IPTR) NULL, msg->ops_AttrList);
        data->labelplace = GetTagData(GA_LabelPlace, GV_LabelPlace_Left, msg->ops_AttrList);
        data->parentgadget = (struct Gadget *)GetTagData(GTA_Text_ParentGadget, 0, msg->ops_AttrList);
        
        /* Open font to use for gadget */
        
        /* We will *ALWAYS* have a valid DrawInfo struct */
        data->dri = (struct DrawInfo *)GetTagData(GA_DrawInfo, (IPTR) NULL, msg->ops_AttrList);

        def_tattr.ta_Name  = data->dri->dri_Font->tf_Message.mn_Node.ln_Name;
        def_tattr.ta_YSize = data->dri->dri_Font->tf_YSize;
        def_tattr.ta_Style = 0;
        def_tattr.ta_Flags = 0;

        tattr = (struct TextAttr *)GetTagData(GA_TextAttr, (IPTR)&def_tattr, msg->ops_AttrList);

        data->font = OpenFont(tattr);
        if (!data->font)
           goto error;
        
        
        if (GetTagData(GTTX_CopyText, (IPTR)FALSE, msg->ops_AttrList))
        {
            STRPTR text;
            
            text = (STRPTR)GetTagData(GTTX_Text, (IPTR)"Text MUST be passed with OM_NEW", msg->ops_AttrList);
            if (text)
            {
                /* Allocate copy buffer for the text */
                data->toprint = (IPTR)AllocVec(strlen(text) + 1, MEMF_ANY);
                if (data->toprint)
                {
                    data->flags |= TEXTF_COPYTEXT;
                    strcpy((STRPTR)data->toprint, text);
                }
                else
                {
                    goto error;
                }
            } else {
                /* If text==NULL we have nothing to copy */
                data->toprint = 0;
            }
        } else {
            STRPTR text;
            
            if ((text = (STRPTR)GetTagData(GTTX_Text, (IPTR) NULL, msg->ops_AttrList)))
            {
                data->toprint = (IPTR)text;
            }
        }

        text_set(cl, o, msg);
        
        if (data->flags & TEXTF_BORDER)
        {
            data->frame = gt_newframe(GadToolsBase,
                                      GetTagData(GA_Width, 0, msg->ops_AttrList),
                                      GetTagData(GA_Height, 0, msg->ops_AttrList),
                                      ((IPTR)data->dri->dri_Resolution.X << 16) + data->dri->dri_Resolution.Y,
                                      FRAME_BUTTON, IA_Recessed);
        }
        
    }
    return (IPTR)o;
    
error:
    CoerceMethod(cl, o, OM_DISPOSE);
    return (IPTR) NULL;
}

/**********************************************************************************************/

#define HBORDER 2
#define VBORDER 2

AROS_UFH2 (void, puttostr,
    AROS_UFHA(UBYTE, chr, D0),
    AROS_UFHA(STRPTR *,strPtrPtr,A3)
)
{
    AROS_USERFUNC_INIT
    *(*strPtrPtr)= chr;
    (*strPtrPtr) ++;
    AROS_USERFUNC_EXIT
}

/**********************************************************************************************/

/* Apart from GM_RENDER, so that textbuf is off the stack while the frame and label draw */
static void rendertext(Class *cl, struct TextData *data, struct Gadget *g,
    struct gpRender *msg)
{
    UWORD               *pens = msg->gpr_GInfo->gi_DrInfo->dri_Pens;
    UBYTE               textbuf[256], *str;
    WORD                left, left2, top, width, height, numchars, tlength;
    struct TextFont     *oldfont;
    struct RastPort     *rp = msg->gpr_RPort;

    left   = g->LeftEdge;
    top    = g->TopEdge;
    width  = g->Width;
    height = g->Height;

    /* preserve font */
    oldfont = rp->Font;
    SetFont(rp, data->font);

    str = textbuf;
    if (data->gadgetkind == TEXT_KIND)
    {
        strncpy(str, (char *)data->toprint, sizeof(textbuf));
        textbuf[sizeof(textbuf) - 1] = '\0';
    }
    else /* NUMERIC_KIND or label of SLIDER_KIND */
    {
        ULONG value = data->toprint;
        RawDoFmt(data->format, (RAWARG)&value, (APTR)puttostr, &str);
    }

    numchars = strlen(textbuf);

    if (data->flags & TEXTF_BORDER)
    {
        left += VBORDER + 1;
        top  += HBORDER + 1;
        width  = g->Width -  (VBORDER + 1) * 2;
        height = g->Height - (HBORDER + 1) * 2;
    }

    if (data->flags & TEXTF_CLIPPED)
    {
        struct TextExtent te;

        /* See how many chars fits into the display area */
        numchars = TextFit(rp, textbuf, numchars, &te, NULL, 1, width, g->Height);
    }

    tlength = TextLength(rp, textbuf, numchars);

    left2 = left;
    switch (data->justification)
    {
        case GTJ_LEFT:
            break;
        case GTJ_RIGHT:
            left2 += (width - tlength);
            break;
        case GTJ_CENTER:
            left2 += ((width - tlength) / 2);
            break;
    }

    /* Render text */
    SetABPenDrMd(rp, pens[data->backpen], pens[data->backpen], JAM2);
    RectFill(rp, left, top, left + width - 1, top + height - 1);
    SetAPen(rp, pens[data->frontpen]);

    Move(rp, left2,
         top + ((height - (WORD)data->font->tf_YSize) / 2) + data->font->tf_Baseline);
    Text(rp, textbuf, numchars);

    SetFont(rp, oldfont);
}

IPTR GTText__GM_RENDER(Class *cl, struct Gadget *g, struct gpRender *msg)
{
    struct TextData     *data = INST_DATA(cl, g);

    if (msg->gpr_Redraw == GREDRAW_REDRAW)
    {
        if (data->frame)
        {
            DrawImageState(msg->gpr_RPort,
                    (struct Image *)data->frame,
                    g->LeftEdge,
                    g->TopEdge,
                    IDS_NORMAL,
                    msg->gpr_GInfo->gi_DrInfo);
        }
        
        renderlabel(GadToolsBase, g, msg->gpr_RPort, data->labelplace);
    }
    
    if (data->toprint || (data->gadgetkind != TEXT_KIND))
        rendertext(cl, data, g, msg);
    
    return 0;
}

/**********************************************************************************************/

IPTR GTText__OM_DISPOSE(Class *cl, Object *o, Msg msg)
{
    struct TextData *data = INST_DATA(cl, o);
    
    if (data->flags & TEXTF_COPYTEXT) FreeVec((APTR)data->toprint);
    if (data->font) CloseFont(data->font);
    if (data->frame) DisposeObject(data->frame);
    
    return DoSuperMethodA(cl, o, msg);

}

/**********************************************************************************************/

IPTR GTText__OM_SET(Class *cl, Object *o, struct opSet *msg)
{
    IPTR retval;
    
    retval = DoSuperMethodA(cl, o, (Msg)msg);
    retval += text_set(cl, o, msg);

    /* If we have been subclassed, OM_UPDATE should not cause a GM_RENDER
     * because it would circumvent the subclass from fully overriding it.
     * The check of cl == OCLASS(o) should fail if we have been
     * subclassed, and we have gotten here via DoSuperMethodA().
     */
    if ( retval && ( msg->MethodID == OM_UPDATE ) && ( cl == OCLASS(o) ) )
    {
        struct GadgetInfo *gi = msg->ops_GInfo;
        if (gi)
        {
            gt_render(GadToolsBase, o, gi, GREDRAW_REDRAW);
        } /* if */
    } /* if */

    return retval;
}

/**********************************************************************************************/

IPTR GTText__GM_GOACTIVE(Class *cl, Object *o, Msg msg)
{
    return (IPTR)GMR_NOREUSE;
}

/**********************************************************************************************/
