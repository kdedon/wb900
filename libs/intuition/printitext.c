/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.
    Copyright (C) 2001-2003, The MorphOS Development Team. All Rights Reserved.
*/

#include <proto/graphics.h>
#include "intuition_intern.h"

/*****************************************************************************

    NAME */
#include <graphics/rastport.h>
#include <graphics/rpattr.h>
#include <intuition/intuition.h>
#include <proto/intuition.h>

        AROS_LH4(void, PrintIText,

/*  SYNOPSIS */
        AROS_LHA(struct RastPort  *, rp, A0),
        AROS_LHA(struct IntuiText *, iText, A1),
        AROS_LHA(WORD              , leftOffset, D0),
        AROS_LHA(WORD              , topOffset, D1),

/*  LOCATION */
        struct IntuitionBase *, IntuitionBase, 36, Intuition)

/*  FUNCTION
        Render an IntuiText in the specified RastPort with the
        specified offset.

    INPUTS
        rp - Draw into this RastPort
        iText - Render this text
        leftOffset, topOffset - Starting-Point. All coordinates in the
            IntuiText structures are relative to this point.

    RESULT
        None.

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    int_PrintIText(rp, iText, leftOffset, topOffset, FALSE, IntuitionBase);

    AROS_LIBFUNC_EXIT

} /* PrintIText */

void int_PrintIText(struct RastPort * rp, struct IntuiText * iText,
                    LONG leftOffset, LONG topOffset, BOOL ignore_attributes,
                    struct IntuitionBase *IntuitionBase)
{

    struct GfxBase  *GfxBase = GetPrivIBase(IntuitionBase)->GfxBase;
    IPTR             apen;
    IPTR             bpen;
    IPTR             drmd;
    UBYTE            style;
    struct TextFont *font;
    struct TextFont *newfont = NULL;

    /* Store important variables of the RastPort */
    {
        struct TagItem rptags[4];

        rptags[0].ti_Tag = RPTAG_APen; rptags[0].ti_Data = (IPTR)&apen;
        rptags[1].ti_Tag = RPTAG_BPen; rptags[1].ti_Data = (IPTR)&bpen;
        rptags[2].ti_Tag = RPTAG_DrMd; rptags[2].ti_Data = (IPTR)&drmd;
        rptags[3].ti_Tag = TAG_DONE;
        GetRPAttrsA(rp, rptags);
    }

    font  = rp->Font;
    style = rp->AlgoStyle;

    /* For all borders... */
    for ( ; iText; iText = iText->NextText)
    {
        if (!ignore_attributes)
        {
            /* Change RastPort to the colors/mode specified */
            SetABPenDrMd (rp, iText->FrontPen, iText->BackPen, iText->DrawMode);
        }

        if (iText->ITextFont)
        {
            newfont = OpenFont (iText->ITextFont);

            if (newfont)
                SetFont (rp, newfont);

            SetSoftStyle(rp, iText->ITextFont->ta_Style, AskSoftStyle(rp));
        }

        /* Move to initial position */
        Move (rp
              , iText->LeftEdge + leftOffset
              , iText->TopEdge + topOffset + rp->Font->tf_Baseline
             );
        Text (rp, iText->IText, iText->IText ? strlen (iText->IText) : 0);

        if (iText->ITextFont)
        {
            if (newfont)
            {
                SetFont (rp, font);
                CloseFont (newfont);
            }
        }
    }

    /* Restore RastPort */
    {
        struct TagItem rptags[4];

        rptags[0].ti_Tag = RPTAG_APen; rptags[0].ti_Data = apen;
        rptags[1].ti_Tag = RPTAG_BPen; rptags[1].ti_Data = bpen;
        rptags[2].ti_Tag = RPTAG_DrMd; rptags[2].ti_Data = drmd;
        rptags[3].ti_Tag = TAG_DONE;
        SetRPAttrsA(rp, rptags);
    }
    SetFont      (rp, font);
    SetSoftStyle (rp, style, AskSoftStyle(rp));

}
