/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: Console.device
*/

/*****************************************************************************/

#include <proto/exec.h>
#include <proto/console.h>
#include <proto/intuition.h>
#include <proto/keymap.h>

#include <exec/resident.h>
#include <exec/errors.h>
#include <exec/memory.h>
#include <exec/alerts.h>
#include <devices/inputevent.h>
#include <devices/conunit.h>
#include <devices/newstyle.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <intuition/classusr.h>
#include <graphics/rastport.h>
#include <aros/libcall.h>

#include "console_gcc.h"
#include "consoleif.h"

/*****************************************************************************/

static char cdname[] = CONSOLENAME;
static char cdid[] = "console 41.11 (15.11.2026)";

extern APTR ConsoleFuncs[];

static const UWORD SupportedCommands[] = {
    CMD_READ,
    CMD_WRITE,
    NSCMD_DEVICEQUERY,
    0
};

/*****************************************************************************/

static VOID closelibs(struct ConsoleBase *ConsoleDevice)
{
    if (ConsoleDevice->cb_KeymapBase)
        CloseLibrary(ConsoleDevice->cb_KeymapBase);
    if (ConsoleDevice->cb_IntuitionBase)
        CloseLibrary(ConsoleDevice->cb_IntuitionBase);
    if (ConsoleDevice->cb_UtilityBase)
        CloseLibrary(ConsoleDevice->cb_UtilityBase);
}

static struct Library *cdinit(struct ConsoleBase *ConsoleDevice, APTR seg,
    APTR sysbase)
{
    struct Library *lib = &ConsoleDevice->device.dd_Library;

    lib->lib_Node.ln_Name = cdname;
    lib->lib_IdString = (APTR)cdid;
    lib->lib_Version = 41;
    lib->lib_Revision = 11;
    ConsoleDevice->seglist = seg;

    /* The device base starts out cleared, so on failure just the
       libraries that have been opened are closed */
    if (!(ConsoleDevice->cb_UtilityBase =
            OpenLibrary("utility.library", 0)) ||
        !(ConsoleDevice->cb_IntuitionBase =
            OpenLibrary("intuition.library", 39)) ||
        !(ConsoleDevice->cb_KeymapBase =
            OpenLibrary("keymap.library", 0)))
        goto fail;

    NEWLIST(&ConsoleDevice->unitList);
    NEWLIST(&ConsoleDevice->sniphooks);
    NEWLIST(&ConsoleDevice->readRequests);
    InitSemaphore(&ConsoleDevice->unitListLock);
    InitSemaphore(&ConsoleDevice->consoleTaskLock);
    InitSemaphore(&ConsoleDevice->copyBufferLock);

    /* Create the console classes */
    CONSOLECLASSPTR = makeConsoleClass(ConsoleDevice);
    STDCONCLASSPTR = makeStdConClass(ConsoleDevice);
    CHARMAPCLASSPTR = makeCharMapConClass(ConsoleDevice);
    SNIPMAPCLASSPTR = makeSnipMapConClass(ConsoleDevice);

    if (!CONSOLECLASSPTR || !STDCONCLASSPTR || !CHARMAPCLASSPTR
        || !SNIPMAPCLASSPTR)
        Alert(AT_DeadEnd | AN_ConsoleDev | AG_NoMemory);

    ConsoleDevice->commandPort.mp_Node.ln_Type = NT_MSGPORT;
    ConsoleDevice->commandPort.mp_Flags = PA_IGNORE;
    NEWLIST(&ConsoleDevice->commandPort.mp_MsgList);

    /* Create the console.device task. */
    Forbid();
    ConsoleDevice->consoleTask = CreateTask(cdname, COTASK_PRIORITY,
        (TASKPROC)consoleTaskEntry, COTASK_STACKSIZE);
    if (ConsoleDevice->consoleTask)
        ConsoleDevice->consoleTask->tc_UserData = ConsoleDevice;
    Permit();

    if (ConsoleDevice->consoleTask)
        return lib;

fail:
    closelibs(ConsoleDevice);
    FreeMem((char *)lib - lib->lib_NegSize,
        lib->lib_NegSize + lib->lib_PosSize);
    return NULL;
}

/*****************************************************************************/

void cdopen(struct ConsoleBase *ConsoleDevice,
    struct IOStdReq *ioreq, ULONG unitnum, ULONG flags)
{
    BOOL success = FALSE;

    if (((LONG) unitnum) == CONU_LIBRARY)
        /* unitnum is ULONG while CONU_LIBRARY is -1 :-(   */
    {
        ioreq->io_Device = (struct Device *)ConsoleDevice;

        /* Set io_Unit so that CloseDevice knows this is a CONU_LIBRARY unit.
         * WB1.3 Setmap sets io_Unit to CONU_LIBRARY (-1) before closing
         * console.device */
        ioreq->io_Unit = (struct Unit *)CONU_LIBRARY;
        success = TRUE;
    }
    else
    {
        Class *classptr;
        struct TagItem conunit_tags[2];

        /* AOS programs don't always initialize mn_Length. */
        if (ioreq->io_Message.mn_Length < sizeof(struct IOStdReq))
            goto open_fail;

        conunit_tags[0].ti_Tag = A_Console_Window;
        conunit_tags[0].ti_Data = (IPTR) ioreq->io_Data; /* Window */
        conunit_tags[1].ti_Tag = TAG_DONE;

        /* Select class of which to create console object */
        switch (unitnum)
        {
        case CONU_STANDARD:
            classptr = STDCONCLASSPTR;
            break;

        case CONU_CHARMAP:
            classptr = CHARMAPCLASSPTR;
            break;

        case CONU_SNIPMAP:
            classptr = SNIPMAPCLASSPTR;
            break;

        default:
            goto open_fail;
        }

        /* Create console object */
        ioreq->io_Unit =
            (struct Unit *)NewObjectA(classptr, NULL, conunit_tags);
        if (ioreq->io_Unit)
        {
            struct opAddTail add_msg;
            success = TRUE;

            /* Add the newly created unit to console's list of units */
            ObtainSemaphore(&ConsoleDevice->unitListLock);

            add_msg.MethodID = OM_ADDTAIL;
            add_msg.opat_List = (struct List *)&ConsoleDevice->unitList;
            DoMethodA((Object *) ioreq->io_Unit, (Msg) &add_msg);

            ReleaseSemaphore(&ConsoleDevice->unitListLock);
        } /* if (console unit created) */
    } /* if (not CONU_LIBRARY) */

    if (success)
    {
        ConsoleDevice->device.dd_Library.lib_OpenCnt++;
        return;
    }

  open_fail:

    ioreq->io_Error = IOERR_OPENFAIL;
}

/*****************************************************************************/

APTR cdclose(struct ConsoleBase *ConsoleDevice, struct IORequest *ioreq)
{
    if (ioreq->io_Unit && ioreq->io_Unit != (struct Unit *)CONU_LIBRARY)
    {
        ULONG mid = OM_REMOVE;

        /* Remove the consoe from the console list */
        ObtainSemaphore(&ConsoleDevice->unitListLock);
        DoMethodA((Object *) ioreq->io_Unit, (Msg) &mid);
        ReleaseSemaphore(&ConsoleDevice->unitListLock);

        DisposeObject((Object *) ioreq->io_Unit);
    }
    ConsoleDevice->device.dd_Library.lib_OpenCnt--;

    return NULL;
}

/* The task, its input handler and the classes outlive the opens. */
APTR cdexpunge(struct ConsoleBase *ConsoleDevice)
{
    return NULL;
}

APTR cdnull(VOID)
{
    return NULL;
}

/*****************************************************************************/

AROS_LH1(void, beginio,
    AROS_LHA(struct IOStdReq *, ioreq, A1),
    struct ConsoleBase *, ConsoleDevice, 5, Console)
{
    AROS_LIBFUNC_INIT

    LONG error = 0;
    BOOL done_quick = TRUE;

    /* WaitIO will look into this */
    ioreq->io_Message.mn_Node.ln_Type = NT_MESSAGE;

    switch (ioreq->io_Command)
    {
    case NSCMD_DEVICEQUERY:
        if (ioreq->io_Length < sizeof(struct NSDeviceQueryResult))
        {
            error = IOERR_BADLENGTH;
        }
        else
        {
            struct NSDeviceQueryResult *d;

            d = (struct NSDeviceQueryResult *)ioreq->io_Data;

            d->DevQueryFormat = 0;
            d->SizeAvailable = sizeof(struct NSDeviceQueryResult);
            d->DeviceType = NSDEVTYPE_CONSOLE;
            d->DeviceSubType = 0;
            d->SupportedCommands = (UWORD *) SupportedCommands;

            ioreq->io_Actual = sizeof(struct NSDeviceQueryResult);
        }
        break;

    case CMD_WRITE:
        {
            ULONG towrite;

            if (ioreq->io_Length == -1)
            {
                towrite = strlen((STRPTR) ioreq->io_Data);
            }
            else
            {
                towrite = ioreq->io_Length;
            }

            ioreq->io_Actual =
                writeToConsole((struct ConUnit *)ioreq->io_Unit,
                ioreq->io_Data, towrite, ConsoleDevice);

            break;
        }

    case CMD_READ:
        done_quick = FALSE;

        break;

    case CD_ASKKEYMAP:
    case CD_SETKEYMAP:
    case CD_ASKDEFAULTKEYMAP:
    case CD_SETDEFAULTKEYMAP:
        if (ioreq->io_Length < sizeof(struct KeyMap))
            error = IOERR_BADLENGTH;
        else if (ioreq->io_Command == CD_SETDEFAULTKEYMAP)
            SetKeyMapDefault((struct KeyMap *)ioreq->io_Data);
        else if (ioreq->io_Command == CD_ASKDEFAULTKEYMAP ||
            (ioreq->io_Command == CD_ASKKEYMAP &&
                ioreq->io_Unit == (struct Unit *)CONU_LIBRARY))
            /* The library unit has no keymap of its own */
            CopyMem(AskKeyMapDefault(), ioreq->io_Data,
                sizeof(struct KeyMap));
        else if (ioreq->io_Unit == (struct Unit *)CONU_LIBRARY)
            /* CD_SETKEYMAP */
            error = IOERR_NOCMD;
        else
        {
            /* Read or replace the keymap of a console unit */
            APTR from = &((struct ConUnit *)ioreq->io_Unit)->cu_KeyMapStruct;
            APTR to = ioreq->io_Data;

            if (ioreq->io_Command == CD_ASKKEYMAP)
                ObtainSemaphoreShared(&ConsoleDevice->unitListLock);
            else
            {
                to = from;
                from = ioreq->io_Data;
                ObtainSemaphore(&ConsoleDevice->unitListLock);
            }
            CopyMem(from, to, sizeof(struct KeyMap));
            ReleaseSemaphore(&ConsoleDevice->unitListLock);
        }
        break;

    default:
        error = IOERR_NOCMD;
        break;
    } /* switch (ioreq->io_Command) */

    if (!done_quick)
    {
        /* Mark IO request to be done non-quick */
        ioreq->io_Flags &= ~IOF_QUICK;
        /* Send to the console task */
        PutMsg(&ConsoleDevice->commandPort, &ioreq->io_Message);
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

/*****************************************************************************/

AROS_LH1(LONG, abortio,
    AROS_LHA(struct IORequest *, ioreq, A1),
    struct ConsoleBase *, ConsoleDevice, 6, Console)
{
    AROS_LIBFUNC_INIT

    LONG ret = -1;

    ObtainSemaphore(&ConsoleDevice->consoleTaskLock);

    /* The ioreq can either be in the ConsoleDevice->commandPort MsgPort,
       or be in the ConsoleDevice->readRequests List, or be already done.

       In the first two cases ln_Type will be NT_MESSAGE (= it can be
       aborted), in the last case ln_Type will be NT_REPLYMSG (cannot
       abort, because already done)

       The consoleTaskLock Semaphore hopefully makes sure that there are no
       other/"in-between" cases.

     */

    if (ioreq->io_Message.mn_Node.ln_Type != NT_REPLYMSG)
    {
        ioreq->io_Error = IOERR_ABORTED;
        Remove(&ioreq->io_Message.mn_Node);
        ReplyMsg(&ioreq->io_Message);

        ret = 0;
    }

    ReleaseSemaphore(&ConsoleDevice->consoleTaskLock);

    return ret;

    AROS_LIBFUNC_EXIT
}

/*****************************************************************************/

static struct {
    ULONG it_Size;
    APTR it_Funcs, it_Struct, it_Init;
} cdinittab = {
    sizeof(struct ConsoleBase), (APTR)ConsoleFuncs, NULL, (APTR)cdinit
};

struct Resident cdtag = {
    RTC_MATCHWORD, &cdtag, (APTR)(&cdtag + 1), RTF_AUTOINIT, 41,
    NT_DEVICE, 4, cdname, cdid, (APTR)&cdinittab
};
