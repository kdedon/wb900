/*
    Copyright (C) 1995-2001, The AROS Development Team. All rights reserved.

    Desc: Commodities input handler
*/

/*  INTERNALS

  * Currently, there is no protection against stupid users. If you, for
    instance, install a broker and attach a custom object to it, and this
    custom object's function is to Route the messages to the broker, there
    will be a deadlock, as the message list will never get empty.
        I have no GOOD solution for this, and therefore nothing to prevent
    this type of deadlock is implemented. One way could be to have a counter
    (in every message) that says how many times a message has been routed
    and if this counter reaches MAGICAL_VALUE the message is Disposed of.
        The above should then perhaps be accompanied by a new COMMAND,
    CXCMD_CYCLE that tells (at least Exchange) that something isn't quite
    right.

  * CX_TYPEFILTER isn't documented anywhere (and it's obsolete) meaning
    that the action done in case of a typefilter is not known to be the
    correct one.

    HISTORY

    ??.08.97   SDuvan Implemented
    16.08.99   Working version
*/

/***************************************************************************/

#include "cxintern.h"
#include <proto/exec.h>
#include <proto/commodities.h>
#include <exec/lists.h>
#include <exec/memory.h>
#include <aros/asmcall.h>

static void ProduceEvent(CxMsg *, struct CommoditiesBase *CxBase);
static void DebugFunc(CxMsg *, CxObj *, struct CommoditiesBase *CxBase);
static void TransFunc(CxMsg *, CxObj *, struct CommoditiesBase *CxBase);
static void SendFunc(CxMsg *, CxObj *, struct CommoditiesBase *CxBase);

/* input.device's handler, at priority 53, ahead of intuition's */
AROS_UFH2(struct InputEvent *, cxIHandler,
    AROS_UFHA(struct InputEvent *     , events , A0),
    AROS_UFHA(struct CommoditiesBase *, CxBase , A1))
{
    AROS_USERFUNC_INIT

    CxObj *co;
    CxMsg *tempMsg, *msg;
    struct Node *node, *succ;

    if (events == NULL)
    {
        return NULL;
    }

    /* Skip the semaphore while there is no broker */
    if (IsListEmpty(&CxBase->cx_BrokerList) &&
        IsListEmpty(&CxBase->cx_GeneratedInputEvents))
    {
        return events;
    }

    ObtainSemaphore(&CxBase->cx_SignalSemaphore);

    /* Take care of the processed input events */
    ForeachNodeSafe(&CxBase->cx_GeneratedInputEvents, node, succ)
    {
        struct GeneratedInputEvent *tempEvent;

        tempEvent = (struct GeneratedInputEvent *)(((UBYTE *)node) - offsetof(struct GeneratedInputEvent, node));
        FreeCxStructure(tempEvent, CX_INPUTEVENT, (struct Library *)CxBase);
    }

    CxBase->cx_IEvents = NULL;
    CxBase->cx_EventExtra = &CxBase->cx_IEvents;
    NEWLIST(&CxBase->cx_GeneratedInputEvents);

    /* The last broker has gone: the events pass unchanged */
    if (IsListEmpty(&CxBase->cx_BrokerList))
    {
        ReleaseSemaphore(&CxBase->cx_SignalSemaphore);

        return events;
    }

    /* Free all the replied messages */
    while ((tempMsg = (CxMsg *)GetMsg(&CxBase->cx_MsgPort)) != NULL)
    {
        FreeCxStructure(tempMsg, CX_MESSAGE, (struct Library *)CxBase);
    }

    /* Route all messages to the first broker */

    co = (CxObj *)GetHead(&CxBase->cx_BrokerList);

    ForeachNode(&CxBase->cx_MessageList, tempMsg)
    {
        ROUTECxMsg(tempMsg, co);
    }

    AddIEvents(events); /* Add the new events (incapsulated in commodtity
                           messages) to the message list. */

/***************************************************************************/

    /* Process the new events */

    while ((msg = (CxMsg *)GetHead(&CxBase->cx_MessageList)) != NULL)
    {
        co = msg->cxm_Routing;

        while (co == NULL && msg->cxm_Level != 0)
        {
            msg->cxm_Level--;
            co = msg->cxm_retObj[msg->cxm_Level];
            co = (CxObj *)GetSucc(&co->co_Node);
        }

        /* If there are no more objects that shall process the event, we
           link it in to the list of ready input events */

        if (co == NULL)
        {
            ProduceEvent(msg, CxBase);
            continue;
        }

        /* Route the message to the next object */

        ROUTECxMsg(msg, (CxObj *)GetSucc(&co->co_Node));

        if (!(co->co_Flags & COF_ACTIVE))
        {
            continue;
        }

        switch (CXOBJType(co))
        {
        case CX_INVALID:
            break;

        case CX_FILTER:

            if ((co->co_Error & COERR_BADFILTER))
            {
                break;
            }

            if (msg->cxm_Type == CXM_IEVENT)
            {
                if (MatchIX(msg->cxm_Data, co->co_Ext.co_FilterIX) != 0)
                {
                    DivertCxMsg(msg, co, co);
                }
            }
            break;

        case CX_TYPEFILTER:
            if ((msg->cxm_Type & co->co_Ext.co_TypeFilter) != 0)
            {
                DivertCxMsg(msg, co, co);
            }
            break;

        case CX_SEND:
            SendFunc(msg, co, CxBase);
            break;

        case CX_SIGNAL:
            Signal(co->co_Ext.co_SignalExt->sixt_Task,
                   1L << co->co_Ext.co_SignalExt->sixt_SigBit);
            break;

        case CX_TRANSLATE:
            TransFunc(msg, co, CxBase);
            break;

        case CX_BROKER:
            DivertCxMsg(msg, co, co);
            break;

        case CX_DEBUG:
            DebugFunc(msg, co, CxBase);
            break;

        case CX_CUSTOM:
            msg->cxm_ID = co->co_Ext.co_CustomExt->cext_ID;

            /* Action shouldn't be NULL, but well, sometimes it is...
             */
            if (co->co_Ext.co_CustomExt->cext_Action)
            {
                (*(VOID (*)())co->co_Ext.co_CustomExt->cext_Action)(msg, co);
            }
            break;

        case CX_ZERO:
            ProduceEvent(msg, CxBase);
            break;
        }
    }

    ReleaseSemaphore(&CxBase->cx_SignalSemaphore);

    return CxBase->cx_IEvents;

    AROS_USERFUNC_EXIT
}

static void ProduceEvent(CxMsg *msg, struct CommoditiesBase *CxBase)
{
    struct GeneratedInputEvent *temp;

    if ((temp = (struct GeneratedInputEvent *)AllocCxStructure(CX_INPUTEVENT, 0,
               (struct Library *)CxBase)) != NULL)
    {
        CopyInputEvent(msg->cxm_Data, &temp->ie, CxBase);

        /* Put the input event last in the ready list and update bookkeeping */
        temp->ie.ie_NextEvent = NULL;

        *(CxBase->cx_EventExtra) = &temp->ie;
        CxBase->cx_EventExtra = &temp->ie.ie_NextEvent;

        AddTail((struct List *)&CxBase->cx_GeneratedInputEvents,
                (struct Node *)&temp->node);
    }

    DisposeCxMsg(msg);
}

static void SendFunc(CxMsg *msg, CxObj *co, struct CommoditiesBase *CxBase)
{
    CxMsg  *tempMsg;
    struct InputEvent *saveIE;  /* To save the InputEvent pointer
                                   from being destroyed by CopyMem() */

    if (co->co_Ext.co_SendExt->sext_MsgPort == NULL)
    {
        return;
    }

    tempMsg = (CxMsg *)AllocCxStructure(CX_MESSAGE, CXM_DOUBLE,
                                        (struct Library *)CxBase);

    if (tempMsg == NULL)
    {
        return;
    }

    saveIE = tempMsg->cxm_Data;
    CopyMem(msg, tempMsg, sizeof(CxMsg));
    tempMsg->cxm_Data = saveIE;

    CopyInputEvent(msg->cxm_Data, tempMsg->cxm_Data, CxBase);

    tempMsg->cxm_ID = co->co_Ext.co_SendExt->sext_ID;

    PutMsg(co->co_Ext.co_SendExt->sext_MsgPort, (struct Message *)tempMsg);
}

static void TransFunc(CxMsg *msg, CxObj *co, struct CommoditiesBase *CxBase)
{
    struct  InputEvent *event;
    CxMsg              *msg2, *prev = NULL;

    if (co->co_Ext.co_IE != NULL)
    {
        event = co->co_Ext.co_IE;

        do
        {
            struct InputEvent *saveIE;  /* To save the InputEvent pointer
                                           from being destroyed by CopyMem() */

            if ((msg2 = (CxMsg *)AllocCxStructure(CX_MESSAGE, CXM_DOUBLE,
                                (struct Library *)CxBase)) == NULL)
            {
                break;
            }

            saveIE = msg2->cxm_Data;
            CopyMem(msg, msg2, sizeof(CxMsg));
            msg2->cxm_Data = saveIE;

            /* Don't care about errors for now */
            CopyInputEvent(event, msg2->cxm_Data, CxBase);

            /* Next in line, in the order of the translation's events */
            Insert(&CxBase->cx_MessageList, (struct Node *)msg2,
                   (struct Node *)prev);
            prev = msg2;

        } while ((event = event->ie_NextEvent) != NULL);
    }

    DisposeCxMsg(msg);
}

static void DebugFunc(CxMsg *msg, CxObj *co, struct CommoditiesBase *CxBase)
{

    if (msg->cxm_Type != CXM_IEVENT)
    {
        return;
    }

    xKPrintf("dump IE: %08lx\tClass %02x\tCode %04x\tQualifier %04x"
             "\nEventAddress 0x%08lx\n",
             msg->cxm_Data, (int)msg->cxm_Data->ie_Class, msg->cxm_Data->ie_Code,
             msg->cxm_Data->ie_Qualifier, msg->cxm_Data->ie_EventAddress);
}

BOOL CopyInputEvent(struct InputEvent *from, struct InputEvent *to,
                           struct CommoditiesBase *CxBase)
{
    CopyMem(from, to, sizeof(struct InputEvent));

    if (from->ie_Class == IECLASS_NEWPOINTERPOS)
    {
        switch (from->ie_SubClass)
        {
        case IESUBCLASS_PIXEL :
            if ((to->ie_EventAddress = AllocVec(sizeof(struct IEPointerPixel),
                                                MEMF_ANY)) == NULL)
            {
                return FALSE;
            }

            CopyMem(from->ie_EventAddress, to->ie_EventAddress, sizeof(struct IEPointerPixel));
            break;

        case IESUBCLASS_TABLET :
            if ((to->ie_EventAddress = AllocVec(sizeof(struct IEPointerTablet),
                                                MEMF_ANY)) == NULL)
            {
                return FALSE;
            }

            CopyMem(from->ie_EventAddress, to->ie_EventAddress, sizeof(struct IEPointerTablet));
            break;

        case IESUBCLASS_NEWTABLET :
            if ((to->ie_EventAddress = AllocVec(sizeof(struct IENewTablet),
                                                MEMF_ANY)) == NULL)
            {
                return FALSE;
            }

            CopyMem(from->ie_EventAddress, to->ie_EventAddress, sizeof(struct IENewTablet));
            break;

        default :
            break;
        }
    }

    return TRUE;
}

