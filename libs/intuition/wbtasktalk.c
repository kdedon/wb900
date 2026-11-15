/*
    Copyright (C) 1995-2020, The AROS Development Team. All rights reserved.
    Copyright (C) 2001-2003, The MorphOS Development Team. All Rights Reserved.

    Tell Workbench task that it shall open/close its windows.
*/

/******************************************************************************/

#include <proto/exec.h>
#include <proto/intuition.h>
#include "intuition_intern.h"

/******************************************************************************/

ULONG TellWBTaskToOpenWindows(struct IntuitionBase *IntuitionBase)
{

    if
    (
           GetPrivIBase(IntuitionBase)->WorkBenchMP != NULL
        && GetPrivIBase(IntuitionBase)->WorkBenchMP->mp_SigTask != FindTask(NULL)
    )
    {
        struct IntuiMessage *imsg;

        if ((imsg = AllocIntuiMessage(NULL)))
        {
            /* Setup our message. */
            imsg->ExecMessage.mn_ReplyPort = GetPrivIBase(IntuitionBase)->IntuiReplyPort;
            imsg->Class                    = IDCMP_WBENCHMESSAGE;
            imsg->Code                     = WBENCHOPEN;

            /* Sends it to the handler asynchron */
            PutMsg( GetPrivIBase(IntuitionBase)->WorkBenchMP,
                    &imsg->ExecMessage);

            return(TRUE);
        }
        else
        {

        }
    }
    else
    {

    }
    return(FALSE);
}

/******************************************************************************/

ULONG TellWBTaskToCloseWindows(struct IntuitionBase *IntuitionBase)
{
    struct Library *UtilityBase = GetPrivIBase(IntuitionBase)->UtilityBase;

    if
    (
           GetPrivIBase(IntuitionBase)->WorkBenchMP != NULL
        && GetPrivIBase(IntuitionBase)->WorkBenchMP->mp_SigTask != FindTask(NULL)
    )
    {
        struct MsgPort      replymp;
        struct IntuiMessage imsg;

        /* Setup our reply port. By doing this manually, we can use SIGB_SINGLE
         * and thus avoid allocating a signal (which may fail). */
        SetMem( &replymp, 0, sizeof( replymp ) );

        replymp.mp_Node.ln_Type = NT_MSGPORT;
        replymp.mp_Flags        = PA_SIGNAL;
        replymp.mp_SigBit       = SIGB_SINGLE;
        replymp.mp_SigTask      = FindTask( NULL );
        NewList((struct List *) &replymp.mp_MsgList );

        /* Setup our message */
        imsg.ExecMessage.mn_ReplyPort = &replymp;
        imsg.Class = IDCMP_WBENCHMESSAGE;
        imsg.Code  = WBENCHCLOSE;

        SetSignal(0, SIGF_SINGLE);

        /* Sends it to the handler and wait for the reply */
        PutMsg( GetPrivIBase(IntuitionBase)->WorkBenchMP,
                (struct Message *) &imsg );
        WaitPort( &replymp );

        /* After leaving this block imsg and repymp will be automagically freed,
         * so we don't have to deallocate them ourselves. */

        return(TRUE);
    }
    else
    {

    }
    return(FALSE);
}

/******************************************************************************/
