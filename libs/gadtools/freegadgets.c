/*
    Copyright (C) 1995-2007, The AROS Development Team. All rights reserved.

    Desc:
*/
#include <proto/exec.h>
#include "gadtools_intern.h"

/*********************************************************************

    NAME */
#include <proto/gadtools.h>
#include <intuition/intuition.h>

        AROS_LH1(VOID, FreeGadgets,

/*  SYNOPSIS */
        AROS_LHA(struct Gadget *, glist, A0),

/*  LOCATION */
        struct Library *, GadToolsBase, 6, GadTools)

/*  FUNCTION
        Frees all gadtools gadgets in a linked list of gadgets.

    INPUTS
        glist - pointer to the first gadget to be freed, may be NULL.

    RESULT

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO
        CreateGadgetA()

    INTERNALS

    HISTORY

***************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct Gadget       *lastgad = NULL, *nextgad = NULL;


    if (!glist)
        return;

    for (; glist; glist = nextgad)
    {
        nextgad = glist->NextGadget;
        if (glist->GadgetType & GTYP_GADTOOLS)
        {
            if ((glist->GadgetType & GTYP_GTYPEMASK) == GTYP_CUSTOMGADGET)
            {

                /* must check this, because arrowclass uses GA_LabelImage! */
                if ((glist->Flags & GFLG_LABELMASK) == GFLG_LABELITEXT)
                {
                    freeitext(GTB(GadToolsBase), glist->GadgetText);
                }
                DisposeObject(glist);
                
            } /* if ((glist->GadgetType & GTYP_GTYPEMASK) == GTYP_CUSTOMGADGET) */
            else
            {
                struct GT_ContextGadget *contextgad = (struct GT_ContextGadget *)glist;
                struct GT_GenericGadget *genericgad = (struct GT_GenericGadget *)glist;
                
                if ( (contextgad->magic  == CONTEXT_MAGIC) &&
                     (contextgad->magic2 == CONTEXT_MAGIC2) )
                {
                    /* This is a GadTools Context Gadget */
                    
                    if (contextgad->gtmsg_used)
                    {
                        /* App is calling FreeGadgets() while still holding unreplied GT Messages.
                           Delay killing of context gad because the gtmsg is embedded in it.
                           Kill will happen in GT_PostFilterImsg() */


                        contextgad->killme = TRUE;
                    }
                    else
                    {
                        FreeMem(glist, sizeof(struct GT_ContextGadget));
                    }
                }
                else if ( (genericgad->magic  == GENERIC_MAGIC) &&
                          (genericgad->magic2 == GENERIC_MAGIC2) )
                {
                    /* This is a GadTools Generic Kind Gadget */
                    
                    freeitext(GTB(GadToolsBase), ((struct GT_GenericGadget *)glist)->itext);
                    FreeMem(glist, sizeof(struct GT_GenericGadget));
                }
                
            } /* if ((glist->GadgetType & GTYP_GTYPEMASK) == GTYP_CUSTOMGADGET) else ... */
            
        } /* if (glist->GadgetType & GTYP_GADTOOLS) */
        else
        {
            if (lastgad != NULL)
                lastgad->NextGadget = glist;
            lastgad = glist;
        }
        
    } /* for (; glist; glist = nextgad) */

    if (lastgad != NULL)
        lastgad->NextGadget = NULL;


    AROS_LIBFUNC_EXIT
} /* FreeGadgets */
