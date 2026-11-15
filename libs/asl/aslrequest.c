/*
    Copyright (C) 1995-2015, The AROS Development Team. All rights reserved.

    Desc:
*/
#include <proto/dos.h>
#include <proto/utility.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <intuition/intuition.h>
#include "asl_intern.h"

struct LayoutData *AllocCommon(ULONG, struct IntReq *, APTR, struct AslBase_intern *);
VOID FreeCommon(struct LayoutData *, struct AslBase_intern *);
BOOL HandleEvents(struct LayoutData *, struct AslReqInfo *, struct AslBase_intern *);

/*****************************************************************************

    NAME */
#include <proto/asl.h>
#include <utility/tagitem.h>

        AROS_LH2(BOOL, AslRequest,

/*  SYNOPSIS */
        AROS_LHA(APTR            , requester, A0),
        AROS_LHA(struct TagItem *, tagList, A1),

/*  LOCATION */
        struct Library *, AslBase, 10, Asl)

/*  FUNCTION

    INPUTS

    RESULT

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

*****************************************************************************/
{

    struct ReqNode      *reqnode;
    struct IntReq       *intreq;
    struct AslReqInfo   *reqinfo;
    struct LayoutData   *ld;
    struct Window       *win;

    BOOL success = FALSE;

    if (!requester)
    {
        SetIoErr(ERROR_NO_FREE_STORE);
        return (FALSE);
    }

    /* Find the correspondent internal structure */
    reqnode = FindReqNode(requester, ASLB(AslBase));
    if (!reqnode)
    {
        return (FALSE);
    }

    intreq  = reqnode->rn_IntReq;
    reqinfo = &(ASLB(AslBase)->ReqInfo[intreq->ir_ReqType]);

/*  stegerg: this must be done when the Requester is terminated
**           before setting the new output variables in the Requester struct.
**
**    StripRequester(requester, intreq->ir_ReqType, ASLB(AslBase));
*/

    /* Parse new tags if supplied */
    if (tagList)
    {
        struct ParseTagArgs pta;

        ParseCommonTags(intreq, tagList, ASLB(AslBase));

        /* Parse requester specific tags */
        pta.pta_IntReq  = intreq;
        pta.pta_Req     = reqnode->rn_Req;
        pta.pta_Tags    = tagList;

        CallHookPkt(&(reqinfo->ParseTagsHook), &pta, ASLB(AslBase));
    }

    /* Now do the layout stuff */

    /* Do some allocations common for all requesters */
    ld = AllocCommon(reqinfo->UserDataSize, intreq, requester, ASLB(AslBase));
    if (ld)
    {

        /* Tell the specific requester to initialize */

        ld->ld_Command = LDCMD_INIT;

        if (CallHookPkt( &(reqinfo->GadgetryHook), ld, ASLB(AslBase)))
        {
            struct NewWindow nw;
            struct TagItem   wintags[] =
            {
                {WA_CustomScreen        , (IPTR)ld->ld_Screen       }, /* stegerg: requesters should not use WA_PubScreen */
                {WA_AutoAdjust          , TRUE                      },
                {WA_NewLookMenus        , TRUE                      },
                {WA_BackFill            , (IPTR)LAYERS_NOBACKFILL   },
                {TAG_DONE                                           }
            };
            ULONG           idcmp;
            BOOL            privateidcmp;

            memset(&nw, 0L, sizeof (struct NewWindow));

            /* Size */
            nw.Width    = intreq->ir_Width;
            nw.Height   = intreq->ir_Height;

            if (nw.Width < ld->ld_MinWidth) nw.Width = ld->ld_MinWidth;
            if (nw.Height < ld->ld_MinHeight) nw.Height = ld->ld_MinHeight;

            /* Position */
            if (intreq->ir_LeftEdge >= 0)
            {
                nw.LeftEdge = intreq->ir_LeftEdge;
            } else {
                nw.LeftEdge = (ld->ld_Screen->ViewPort.DWidth - (nw.Width + ld->ld_WBorLeft + ld->ld_WBorRight)) / 2
                              - ld->ld_Screen->LeftEdge;
            }
            if (intreq->ir_TopEdge >= 0)
            {
                nw.TopEdge = intreq->ir_TopEdge;
            } else {
                nw.TopEdge = (ld->ld_Screen->ViewPort.DHeight - (nw.Height + ld->ld_WBorTop + ld->ld_WBorBottom)) / 2
                             - ld->ld_Screen->TopEdge;
            }

            nw.Title = intreq->ir_TitleText;
            if (!nw.Title)
            {
                nw.Title = GetString(intreq->ir_TitleID);
            }

/*          nw.FirstGadget      = ld->ld_GList;
*/
            nw.Flags    =  WFLG_DRAGBAR    | WFLG_DEPTHGADGET   | WFLG_CLOSEGADGET    |
                           WFLG_SIZEGADGET | WFLG_SIZEBBOTTOM   | WFLG_SIMPLE_REFRESH |
                           WFLG_NOCAREREFRESH;

            if (!(intreq->ir_Flags & IF_OPENINACTIVE))
            {
                nw.Flags |= WFLG_ACTIVATE;
            }

            idcmp = IDCMP_CLOSEWINDOW | IDCMP_GADGETUP      |
                    IDCMP_NEWSIZE     | IDCMP_REFRESHWINDOW | IDCMP_GADGETDOWN |
                    IDCMP_MENUPICK    | IDCMP_RAWKEY        | IDCMP_VANILLAKEY |
                    IDCMP_MOUSEBUTTONS;

            privateidcmp = ((intreq->ir_Flags & IF_PRIVATEIDCMP) || (!intreq->ir_Window));
            if (privateidcmp)
            {
                nw.IDCMPFlags = idcmp;
            }

            win = OpenWindowTagList(&nw, wintags);
            if (win)
            {
                struct Window *oldwinptr = (struct Window *) -1;
                struct Process *Self;

                if (!privateidcmp)
                {
                    win->UserPort = intreq->ir_Window->UserPort;
                    ModifyIDCMP(win, idcmp);
                }

                ld->ld_Window = win;

                ObtainSemaphore( &(ASLB(AslBase)->ReqListSem) );
                reqnode->rn_ReqWindow = win;
                ReleaseSemaphore(&(ASLB(AslBase)->ReqListSem));

                /* Constraint the window minsize */

                WindowLimits
                (
                    win,
                    ld->ld_MinWidth + win->BorderLeft + win->BorderRight,
                    ld->ld_MinHeight + win->BorderTop + win->BorderBottom,
                    ~0,
                    ~0
                );

                SetFont(win->RPort, ld->ld_Font);

                /* Set process windowpointer to catch possible requesters
                    to correct screen & relative to requester window. - Piru
                    */
                Self = (struct Process *) FindTask(NULL);
                if (Self->pr_Task.tc_Node.ln_Type == NT_PROCESS &&
                    Self->pr_WindowPtr != (struct Window *) -1)
                {
                    oldwinptr = Self->pr_WindowPtr;
                    Self->pr_WindowPtr = win;
                }

                /* Layout the requester */
                ld->ld_Command = LDCMD_LAYOUT;
                if (CallHookPkt(&(reqinfo->GadgetryHook), ld, ASLB(AslBase)))
                {

                    if (ld->ld_GList)
                    {
                        AddGList(win, ld->ld_GList, -1, -1, NULL);
                        RefreshGList(ld->ld_GList, win, NULL, -1);

                    }

                    if (ld->ld_Menu) SetMenuStrip(win, ld->ld_Menu);

                    ld->ld_Command = LDCMD_WINDOWOPENED;
                    CallHookPkt(&(reqinfo->GadgetryHook), ld, ASLB(AslBase));

                    if (intreq->ir_Flags & IF_POPTOFRONT)
                    {
                        struct Screen   *frontscr;
                        ULONG            ilock;

                        ilock = LockIBase(0);
                        frontscr = IntuitionBase->FirstScreen;
                        UnlockIBase(ilock);

                        if (frontscr != win->WScreen)
                        {
                            ScreenToFront(win->WScreen);
                            intreq->ir_Flags |= IF_POPPEDTOFRONT;
                        }
                    }

                    /* Wait for the user to do something */
                    success = HandleEvents(ld, reqinfo, ASLB(AslBase));

                    /* Finished with event handling, clean up requester
                    specific stuff */

                    ld->ld_Command = LDCMD_CLEANUP;
                    CallHookPkt(&(reqinfo->GadgetryHook), ld, ASLB(AslBase));

                } /* if (CallHookPkt(&(reqinfo->GadgetryHook), ld, ASLB(AslBase))) */

                if (oldwinptr != (struct Window *) -1)
                {
                    Self->pr_WindowPtr = oldwinptr;
                }

                /* win is closed in FreeCommon */

            } /* if (win) */

        } /* if (CallHookPkt( &(reqinfo->GadgetryHook), ld, ASLB(AslBase))) */

        ObtainSemaphore( &(ASLB(AslBase)->ReqListSem) );
        reqnode->rn_ReqWindow = NULL;
        ReleaseSemaphore(&(ASLB(AslBase)->ReqListSem));

        FreeCommon(ld, ASLB(AslBase));

    } /* if (ld) */

    return success;

} /* AslRequest */
