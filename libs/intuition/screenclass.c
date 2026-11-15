/*
    Copyright (C) 2013-2026, The AROS Development Team. All rights reserved.
*/

#include <graphics/sprite.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <proto/alib.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/utility.h>

#include "intuition_intern.h"
#include "intuition_customize.h"

/*i***************************************************************************

    NAME
        --background_screenclass--

    LOCATION
        screenclass

    NOTES
        In AROS screens are BOOPSI objects. It is possible to modify certain
        properties of the screen by using the SetAttrs() and GetAttr()
        functions. screenclass by itself is private to the system and does not
        have a public ID. The user can't create objects of this class
        manually. Screens are created and destroyed as usual, using
        the OpenScreen() and CloseScreen() functions.

        This class is fully compatible with MorphOS starting from v2.x.

*****************************************************************************/

IPTR ScreenClass__OM_DISPOSE(Class *cl, Object *o, Msg msg)
{
    struct IntuitionBase *IntuitionBase = (struct IntuitionBase *)cl->cl_UserData;
    struct IntScreen *screen = INST_DATA(cl, o);

    /* Free decoration objects */
    DisposeObject(screen->WinDecorObj);
    DisposeObject(screen->MenuDecorObj);
    DisposeObject(screen->ScrDecorObj);

    /* Free decoration buffer */
    FreeMem((APTR)screen->DecorUserBuffer, screen->DecorUserBufferSize);

    if ((screen->Decorator != ((struct IntIntuitionBase *)(IntuitionBase))->Decorator) &&
        (screen->Decorator != NULL))
    {
        struct NewDecorator *nd = screen->Decorator;

        ObtainSemaphore(&((struct IntIntuitionBase *)(IntuitionBase))->ScrDecorSem);

        nd->nd_cnt--;
        if (nd->nd_IntPattern == NULL)
        {
            /*
             * If no pattern is defined, we assume it was old default decorator
             * which fell out of use. Unload it.
             */
            int_UnloadDecorator(nd, IntuitionBase);
        }

        ReleaseSemaphore(&((struct IntIntuitionBase *)(IntuitionBase))->ScrDecorSem);
    }

    return DoSuperMethodA(cl, o, msg);
}

/*i***************************************************************************

    NAME
        SA_Left

    SYNOPSIS
        [ISG], LONG

    LOCATION
        screenclass

    FUNCTION
        Position of left edge of the screen relative to top-left physical
        display corner.

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

*****************************************************************************/

/*i***************************************************************************

    NAME
        SA_Top

    SYNOPSIS
        [ISG], LONG

    LOCATION
        screenclass

    FUNCTION
        Position of top edge of the screen relative to top-left physical
        display corner.

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

*****************************************************************************/
/*i***************************************************************************

    NAME
        SA_Width

    SYNOPSIS
        [I.G], LONG

    LOCATION
        screenclass

    FUNCTION
        Width of the screen (not display) in pixels.

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

*****************************************************************************/

/*i***************************************************************************

    NAME
        SA_Height

    SYNOPSIS
        [I.G], LONG

    LOCATION
        screenclass

    FUNCTION
        Height of the screen (not display) in pixels.

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

*****************************************************************************/

/*i***************************************************************************

    NAME
        SA_Depth

    SYNOPSIS
        [I.G], LONG

    LOCATION
        screenclass

    FUNCTION
        Depth of the screen.

    NOTES
        This attribute returns the real depth; however, old structure fields,
        for example screen->RastPort.BitMap->bm_Depth, in case of direct-color
        screens will contain 8 for purposes of backwards compatibility.

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

*****************************************************************************/

/*i***************************************************************************

    NAME
        SA_PubName

    SYNOPSIS
        [I.G], STRPTR

    LOCATION
        screenclass

    FUNCTION
        Public screen name (system name, not human-readable title).

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

*****************************************************************************/

/*i***************************************************************************

    NAME
        SA_DisplayID

    SYNOPSIS
        [I.G], LONG

    LOCATION
        screenclass

    FUNCTION
        Display mode ID of the screen.

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

*****************************************************************************/

/*i***************************************************************************

    NAME
        SA_Behind

    SYNOPSIS
        [I.G], LONG

    LOCATION
        screenclass

    FUNCTION
        Supplying this attribute during screen creation (to OpenScreen() or
        OpenScreenTagList() functions) causes this screen to be created and
        opened behind all other screens.

        Querying this attribute returns its initial value.

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

*****************************************************************************/

/*i***************************************************************************

    NAME
        SA_ShowPointer

    SYNOPSIS
        [ISG], BOOL

    LOCATION
        screenclass

    FUNCTION
        Setting this attribute to FALSE makes mouse pointer invisible on your
        custom screen. The default value is TRUE. Setting this attribute on
        public screens is ignored.

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

*****************************************************************************/

IPTR ScreenClass__OM_GET(Class *cl, Object *o, struct opGet *msg)
{
    struct IntuitionBase *IntuitionBase = (struct IntuitionBase *)cl->cl_UserData;
    struct IntScreen *screen = INST_DATA(cl, o);

    switch (msg->opg_AttrID)
    {
    case SA_Left:
        *msg->opg_Storage = screen->Screen.LeftEdge;
        break;

    case SA_Top:
        *msg->opg_Storage = screen->Screen.TopEdge;
        break;

    case SA_Width:
        *msg->opg_Storage = screen->Screen.Width;
        break;

    case SA_Height:
        *msg->opg_Storage = screen->Screen.Height;
        break;

    case SA_Depth:
        *msg->opg_Storage = screen->realdepth;
        break;

    case SA_PubName:
        *msg->opg_Storage = (IPTR)screen->pubScrNode->psn_Node.ln_Name;
        break;

    case SA_DisplayID:
        *msg->opg_Storage = screen->ModeID;
        break;

    case SA_Behind:
        *msg->opg_Storage = (screen->Screen.Flags & SCREENBEHIND) ? TRUE : FALSE;
        break;

    default:
        /*
         * The attributes numbered from SA_Dummy + 100 up lie apart from the
         * ones above. They have a switch of their own, so that each of the
         * two covers a dense range of numbers.
         */
        switch (msg->opg_AttrID)
        {
        case SA_Displayed:
            /* The display shows the frontmost screen */
            *msg->opg_Storage = (&screen->Screen == IntuitionBase->FirstScreen);
            break;
        case SA_ShowPointer:
            *msg->opg_Storage = screen->ShowPointer;
            break;

        case SA_DisplayWidth:
            *msg->opg_Storage = screen->Screen.ViewPort.DWidth;
            break;

        case SA_DisplayHeight:
            *msg->opg_Storage = screen->Screen.ViewPort.DHeight;
            break;

        case SA_PixelFormat:
            *msg->opg_Storage = -1;
            break;
        case SA_ScreenbarTextYPos:
            *msg->opg_Storage = screen->Screen.BarVBorder + screen->Screen.BarLayer->rp->TxBaseline;
            break;

        case SA_ScreenbarTextPen:
            *msg->opg_Storage = screen->Pens[BARDETAILPEN];
            break;

        case SA_ScreenbarTextColor:
            *msg->opg_Storage = 0xFFFFFFFF;
            if (screen->ScrDecorObj) {
                GetAttr(SA_ScreenbarTextColor, screen->ScrDecorObj, msg->opg_Storage);
                break;
            }
            break;

        case SA_ScreenbarTextFont:
            if (screen->ScrDecorObj) {
                *msg->opg_Storage = (IPTR)screen->DInfo.dri_Font;
                break;
            }
            *msg->opg_Storage = (IPTR)screen->Screen.BarLayer->rp->Font;
            break;

        case SA_ScreenbarSignal:    /* Reserved in AROS */
            *msg->opg_Storage = 0;
            break;

        default:
            return FALSE;
        }
        break;
    }

    return TRUE;
}

IPTR ScreenClass__OM_SET(Class *cl, Object *o, struct opSet *msg)
{
    struct IntuitionBase *IntuitionBase = (struct IntuitionBase *)cl->cl_UserData;
    struct Library *UtilityBase = GetPrivIBase(IntuitionBase)->UtilityBase;
    struct IntScreen *screen = INST_DATA(cl, o);
    struct TagItem *tstate = msg->ops_AttrList;
    struct TagItem *tag;
    BOOL showpointer = screen->ShowPointer;
    BOOL movescreen = FALSE;

    while((tag = NextTagItem(&tstate)))
    {
        switch (tag->ti_Tag)
        {
        case SA_Left:
            if (screen->SpecialFlags & SF_Draggable)
            {
                screen->Screen.LeftEdge = tag->ti_Data;
                movescreen = TRUE;
            }
            break;

        case SA_Top:
            if (screen->SpecialFlags & SF_Draggable)
            {
                screen->Screen.TopEdge = tag->ti_Data;
                movescreen = TRUE;
            }
            break;

        case SA_ShowPointer:
            showpointer = tag->ti_Data;
            break;

        }
    }

    if (movescreen)
    {
        /*
         * Move the screen to (0, 0) relative of current position.
         * LeftEdge/TopEdge have been already updated.
         */
        ScreenPosition(&screen->Screen, SPOS_RELATIVE, 0, 0, 0, 0);
    }

    if (showpointer != screen->ShowPointer)
    {
        screen->ShowPointer = showpointer;
        /* TODO: Actually implement this */
    }

    return 0;
}
