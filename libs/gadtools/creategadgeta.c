/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: gadtools.library function CreateGadgetA()
*/
#include <proto/exec.h>
#include <exec/memory.h>
#include <proto/intuition.h>
#include <intuition/gadgetclass.h>
#include <proto/utility.h>
#include "gadtools_intern.h"

/* gt_makefuncs[] is indexed by the gadget kind */
typedef char gt_kindcheck[GENERIC_KIND == 0 && BUTTON_KIND == 1 && CHECKBOX_KIND == 2 && INTEGER_KIND == 3 &&
               LISTVIEW_KIND == 4 && MX_KIND == 5 && NUMBER_KIND == 6 && CYCLE_KIND == 7 &&
               PALETTE_KIND == 8 && SCROLLER_KIND == 9 && SLIDER_KIND == 11 && STRING_KIND == 12 &&
               TEXT_KIND == 13 && NUM_KINDS == 14 ? 1 : -1];

/* The function that creates the gadget(s) of each kind */
static gt_makefunc * const gt_makefuncs[NUM_KINDS] =
{
    makegeneric,    /* GENERIC_KIND  */
    makebutton,     /* BUTTON_KIND   */
    makecheckbox,   /* CHECKBOX_KIND */
    makestring,     /* INTEGER_KIND  */
    makelistview,   /* LISTVIEW_KIND */
    makemx,         /* MX_KIND       */
    maketext,       /* NUMBER_KIND   */
    makecycle,      /* CYCLE_KIND    */
    makepalette,    /* PALETTE_KIND  */
    makescroller,   /* SCROLLER_KIND */
    NULL,           /* 10            */
    makeslider,     /* SLIDER_KIND   */
    makestring,     /* STRING_KIND   */
    maketext        /* TEXT_KIND     */
};

/*********************************************************************

    NAME */
#include <proto/gadtools.h>
#include <exec/types.h>
#include <intuition/intuition.h>
#include <utility/tagitem.h>
#include <libraries/gadtools.h>

        AROS_LH4(struct Gadget *, CreateGadgetA,

/*  SYNOPSIS */
        AROS_LHA(ULONG, kind, D0),
        AROS_LHA(struct Gadget *, previous, A0),
        AROS_LHA(struct NewGadget *, ng, A1),
        AROS_LHA(struct TagItem *, taglist, A2),

/*  LOCATION */
        struct Library *, GadToolsBase, 5, GadTools)

/*  FUNCTION
        Creates a gadtools gadget.

    INPUTS

        kind -     Kind of gadget. See <libraries/gadtools.h> for a list of
                   all possible kinds.
        previous - Pointer to the previous gadget in gadget-list. Create the
                   first "gadget" with CreateContext(). This may be NULL, in
                   which case CreateGadgetA() fails.
        ng -       Pointer to struct NewGadget. See <libraries/gadtools.h>.
        taglist -  Additional tags. See <libraries/gadtools.h>.

    RESULT
        A pointer to a gadget or NULL to indicate an error.

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO
        CreateContext(), FreeGadgets(), <libraries/gadtools.h>

    INTERNALS

    HISTORY

***************************************************************************/
{
    AROS_LIBFUNC_INIT

    int LV_HeightCorrection = 0;

    struct Gadget       *gad = NULL;
    struct TagItem      stdgadtags[] =
    {
        {GA_Left        , 0L                    },
        {GA_Top         , 0L                    },
        {GA_Width       , 0L                    },
        {GA_Height      , 0L                    },
        {GA_IntuiText   , (IPTR)NULL            },
        {GA_LabelPlace  , (IPTR)GV_LabelPlace_In},
        {GA_Previous    , (IPTR)NULL            },
        {GA_ID          , 0L                    },
        {GA_DrawInfo    , (IPTR)NULL            },
        {GA_UserData    , (IPTR)NULL            },
        {TAG_DONE                               }
    };


    if (previous == NULL || ng == NULL || ng->ng_VisualInfo == NULL)
        return (NULL);




    /* Emm: The makeXXX() functions expect this, and with 'normal' object creation,
     * this is always true. However, with Triton previous->NextGadget is trashed
     * after a window resize. What Triton does is somewhat questionable, but
     * setting this pointer doesn't hurt, so...
     */

    previous->NextGadget = NULL;

    /* Set the default label place according to the gadget type. */

    if (kind == LISTVIEW_KIND)
        stdgadtags[TAG_LabelPlace].ti_Data = GV_LabelPlace_Above;
    else if (kind != BUTTON_KIND)
        stdgadtags[TAG_LabelPlace].ti_Data = GV_LabelPlace_Left;
    
    stdgadtags[TAG_Left].ti_Data = ng->ng_LeftEdge;
    stdgadtags[TAG_Top].ti_Data = ng->ng_TopEdge;
    stdgadtags[TAG_Width].ti_Data = ng->ng_Width;
    stdgadtags[TAG_Height].ti_Data = ng->ng_Height;

    if (ng->ng_GadgetText)
    {
        ULONG old_ng_flags = ng->ng_Flags;
        
        if (kind == BUTTON_KIND) ng->ng_Flags &= ~NG_HIGHLABEL;
        stdgadtags[TAG_IText].ti_Data = (IPTR)makeitext(GTB(GadToolsBase), ng, taglist);
        if (kind == BUTTON_KIND) ng->ng_Flags = old_ng_flags;
        
        if (!stdgadtags[TAG_IText].ti_Data)
        {
            return (NULL);
        }
    } else {
        stdgadtags[TAG_IText].ti_Tag = TAG_IGNORE;
    }

    stdgadtags[TAG_Previous].ti_Data = (IPTR)previous;
    stdgadtags[TAG_ID].ti_Data = ng->ng_GadgetID;
    stdgadtags[TAG_DrawInfo].ti_Data = (IPTR)(VI(ng->ng_VisualInfo)->vi_dri);
    stdgadtags[TAG_UserData].ti_Data = (IPTR)ng->ng_UserData;

    /* Calculate label placement.*/
    if ((ng->ng_Flags & PLACETEXT_LEFT))
        stdgadtags[TAG_LabelPlace].ti_Data = GV_LabelPlace_Left;
    else if ((ng->ng_Flags & PLACETEXT_RIGHT))
        stdgadtags[TAG_LabelPlace].ti_Data = GV_LabelPlace_Right;
    else if ((ng->ng_Flags & PLACETEXT_ABOVE))
        stdgadtags[TAG_LabelPlace].ti_Data = GV_LabelPlace_Above;
    else if ((ng->ng_Flags & PLACETEXT_BELOW))
        stdgadtags[TAG_LabelPlace].ti_Data = GV_LabelPlace_Below;
    else if ((ng->ng_Flags & PLACETEXT_IN ))
    	stdgadtags[TAG_LabelPlace].ti_Data = GV_LabelPlace_In;

    if (kind == LISTVIEW_KIND)
    {
        /* correction values are based on a visual comparison between AmigaOS and ApolloOS */
        LV_HeightCorrection = (ng->ng_TextAttr) ? ((ng->ng_TextAttr)->ta_YSize)-4 : 7;
        stdgadtags[TAG_Height].ti_Data = stdgadtags[TAG_Height].ti_Data - LV_HeightCorrection;
    }

    if ((kind < NUM_KINDS) && gt_makefuncs[kind])
    {
        gad = gt_makefuncs[kind](GTB(GadToolsBase),
                                 stdgadtags,
                                 VI(ng->ng_VisualInfo),
                                 ng->ng_TextAttr,
                                 taglist,
                                 kind);
    }

    if (gad)
    {

        /*
         * Scan through all childgadgets and the return the last one, so we return
         * a ptr with no NextGadget which would collide with our triton fix above
         */
        while (gad->NextGadget)
        {
            gad = gad->NextGadget;
        }
    }
    else
    {
        previous->NextGadget = NULL;
        FreeVec((APTR)stdgadtags[TAG_IText].ti_Data);
    }

    
    return (gad);
    AROS_LIBFUNC_EXIT
    
} /* CreateGadgetA */
