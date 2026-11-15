/*
    Copyright (C) 1995-2015, The AROS Development Team. All rights reserved.

    Desc: Input device
*/

#include <devices/inputevent.h>
#include <devices/input.h>
#include <devices/newstyle.h>
#include <proto/exec.h>
#include <proto/input.h>
#include <exec/memory.h>
#include <exec/errors.h>
#include <exec/resident.h>

#include "input_intern.h"

static char idname[] = INPUTNAME;
static char idid[] = "input 41.4 (14.11.2026)";

extern APTR InputFuncs[];

static const UWORD SupportedCommands[] =
{
    CMD_RESET,
    CMD_STOP,
    CMD_START,
    IND_ADDHANDLER,
    IND_REMHANDLER,
    IND_WRITEEVENT,
    IND_ADDEVENT,
    IND_SETTHRESH,
    IND_SETPERIOD,
    NSCMD_DEVICEQUERY,
    0
};

static struct Library *idinit(struct Library *lib, APTR seg, APTR sysbase)
{
    struct inputbase *InputDevice = (struct inputbase *)lib;

    lib->lib_Node.ln_Name = idname;
    lib->lib_IdString = (APTR)idid;
    lib->lib_Version = 41;
    lib->lib_Revision = 4;
    InputDevice->SegList = seg;

    NewList((struct List *)&(InputDevice->HandlerList));

    /*
       These defaults are in terms of 50 Hz ticks. The real VBlank frequency
       does not affect them.
     */
    InputDevice->KeyRepeatThreshold.tv_secs =
        DEFAULT_KEY_REPEAT_THRESHOLD / 50;
    InputDevice->KeyRepeatThreshold.tv_micro =
        (DEFAULT_KEY_REPEAT_THRESHOLD % 50) * 1000000L / 50;

    InputDevice->KeyRepeatInterval.tv_secs =
        DEFAULT_KEY_REPEAT_INTERVAL / 50;
    InputDevice->KeyRepeatInterval.tv_micro =
        (DEFAULT_KEY_REPEAT_INTERVAL % 50) * 1000000L / 50;

    /* Requests queue here until the task takes the port's signal. */
    InputDevice->CommandPort.mp_Node.ln_Type = NT_MSGPORT;
    InputDevice->CommandPort.mp_Flags = PA_IGNORE;
    NewList(&InputDevice->CommandPort.mp_MsgList);

    Forbid();
    InputDevice->InputTask = CreateTask(idname, IDTASK_PRIORITY,
        (TASKPROC)ProcessEvents, IDTASK_STACKSIZE);
    if (InputDevice->InputTask)
        InputDevice->InputTask->tc_UserData = InputDevice;
    Permit();

    if (InputDevice->InputTask)
        return lib;

    FreeMem((char *)lib - lib->lib_NegSize,
        lib->lib_NegSize + lib->lib_PosSize);
    return NULL;
}

void idopen(struct Library *lib, struct IORequest *ioreq, ULONG unitnum,
    ULONG flags)
{
    /* Too many AOS programs do not initialize mn_Length. AOS input.device
       does not care. */
    if (ioreq->io_Message.mn_Length < sizeof(struct IOStdReq))
    {
        ioreq->io_Error = IOERR_OPENFAIL;
        return;
    }
    ioreq->io_Unit = NULL;
    lib->lib_OpenCnt++;
}

APTR idclose(struct Library *lib, struct IORequest *ioreq)
{
    lib->lib_OpenCnt--;
    return NULL;
}

/* The task and its handlers outlive the opens. */
APTR idexpunge(struct Library *lib)
{
    return NULL;
}

APTR idnull(void)
{
    return NULL;
}

#define ioStd(x)  ((struct IOStdReq *)x)


AROS_LH1(void, beginio,
    AROS_LHA(struct IOStdReq *, ioreq, A1),
    struct inputbase *, InputDevice, 5, Input)
{
    AROS_LIBFUNC_INIT

    LONG error = 0;
    BOOL done_quick = TRUE;

    /* WaitIO will look into this */
    ioreq->io_Message.mn_Node.ln_Type = NT_MESSAGE;

    switch (ioreq->io_Command)
    {
    case NSCMD_DEVICEQUERY:
        if (ioStd(ioreq)->io_Length < sizeof(struct NSDeviceQueryResult))
        {
            error = IOERR_BADLENGTH;
        }
        else
        {
            struct NSDeviceQueryResult *d;

            d = (struct NSDeviceQueryResult *)ioStd(ioreq)->io_Data;

            d->DevQueryFormat = 0;
            d->SizeAvailable = sizeof(struct NSDeviceQueryResult);
            d->DeviceType = NSDEVTYPE_INPUT;
            d->DeviceSubType = 0;
            d->SupportedCommands = (UWORD *) SupportedCommands;

            ioStd(ioreq)->io_Actual = sizeof(struct NSDeviceQueryResult);
        }
        break;

    case IND_ADDHANDLER:
    case IND_REMHANDLER:
    case IND_WRITEEVENT:
    case IND_ADDEVENT:
    case IND_SETTHRESH:
    case IND_SETPERIOD:
    case CMD_RESET:
    case CMD_STOP:
    case CMD_START:
        done_quick = FALSE;
        break;

    default:
        error = IOERR_NOCMD;
        break;
    }

    if (!done_quick)
    {
        /* Mark IO request to be done non-quick */
        ioreq->io_Flags &= ~IOF_QUICK;
        /* Send to input device task */
        PutMsg(&InputDevice->CommandPort, &ioreq->io_Message);
    }
    else
    {

        /* If the quick bit is not set but the IO request was done quick,
         * reply the message to tell we're through
         */
        ioreq->io_Error = error;
        if (!(ioreq->io_Flags & IOF_QUICK))
            ReplyMsg(&ioreq->io_Message);
    }

    AROS_LIBFUNC_EXIT
}


AROS_LH1(LONG, abortio,
    AROS_LHA(struct IORequest *, ioreq, A1),
    struct inputbase *, InputDevice, 6, Input)
{
    AROS_LIBFUNC_INIT

    struct Node *n;
    LONG ret = -1;

    /* Only a request the task has not taken yet can be aborted. */
    Forbid();
    for (n = InputDevice->CommandPort.mp_MsgList.lh_Head; n->ln_Succ;
        n = n->ln_Succ)
    {
        if (n == &ioreq->io_Message.mn_Node)
        {
            Remove(n);
            ioreq->io_Error = IOERR_ABORTED;
            ReplyMsg(&ioreq->io_Message);
            ret = 0;
            break;
        }
    }
    Permit();

    return ret;

    AROS_LIBFUNC_EXIT
}

static struct {
    ULONG it_Size;
    APTR it_Funcs, it_Struct, it_Init;
} idinittab = {
    sizeof(struct inputbase), (APTR)InputFuncs, NULL, (APTR)idinit
};

struct Resident idtag = {
    RTC_MATCHWORD, &idtag, (APTR)(&idtag + 1), RTF_AUTOINIT, 41,
    NT_DEVICE, 30, idname, idid, (APTR)&idinittab
};
