/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: Code executed by the console.device task.
*/

#include <exec/lists.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/console.h>
#include <exec/io.h>
#include <exec/alerts.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <devices/input.h>
#include <devices/rawkeycodes.h>

#include "console_gcc.h"
#include "consoleif.h"

VOID con_inject(struct ConsoleBase *ConsoleDevice, struct ConUnit *cu,
    const UBYTE *data, LONG size)
{
    struct intConUnit *icu = ICU(cu);
    ObtainSemaphore(&ConsoleDevice->consoleTaskLock);
    if (data && size)
    {
        struct intPasteData *p =
            AllocMem(sizeof(struct intPasteData), MEMF_ANY);
        if (p)
        {
            if (size < 0)
                size = strlen(data);
            p->pasteBuffer = AllocMem(size, MEMF_ANY);
            if (p->pasteBuffer)
            {
                CopyMem((APTR) data, p->pasteBuffer, size);
                p->pasteBufferSize = size;
                p->unit = icu;
                AddTail((struct List *)&icu->pasteData, (struct Node *)p);
            }
            else
            {
                FreeMem(p, sizeof(struct intPasteData));
            }
        }
    }
    ReleaseSemaphore(&ConsoleDevice->consoleTaskLock);
    /* Wake up possible waiting CMD_READs */
    Signal(ConsoleDevice->consoleTask, SIGBREAKF_CTRL_D);
}

/* Protos */
static BOOL lockconunit(Object *unit, struct ConsoleBase *ConsoleDevice);
static void answer_read_request(struct IOStdReq *req,
    struct ConsoleBase *ConsoleDevice);

static VOID answer_requests(APTR unit, struct ConsoleBase *ConsoleDevice)
{
    struct IOStdReq *req, *nextreq;
    /* See if there are any queued io read requests that wants to be replied */
    ForeachNodeSafe(&ConsoleDevice->readRequests, req, nextreq)
    {
        if ((APTR) req->io_Unit == (APTR) unit)
        {
            /* Paranoia */
            if (0 != ICU(req->io_Unit)->numStoredChars)
            {
                Remove((struct Node *)req);
                answer_read_request(req, ConsoleDevice);
            }
        }
    }
}

/* RawDoFmt() output into a string */
static VOID putch(UBYTE c, UBYTE **p)
{
    *(*p)++ = c;
}

static BOOL report_raw_event(Object *unit, struct InputEvent *event,
    struct ConsoleBase *ConsoleDevice)
{
    struct intConUnit *icu = ICU(unit);
    UBYTE report[128], *p = report;
    LONG fields[8];
    LONG x, y;
    ULONG len;

    if (!CHECK_RAWEVENT(unit, event->ie_Class))
        return FALSE;

    /* The classic console.device reports mouse button transitions but not
     * IECODE_NOBUTTON movement events through this raw-event byte stream.
     * Consumers such as Ed read the mouse coordinates maintained by
     * Intuition when processing the press and release reports. */
    if (event->ie_Class == IECLASS_RAWMOUSE &&
        event->ie_Code == IECODE_NOBUTTON)
        return TRUE;

    if (event->ie_Class == IECLASS_RAWKEY)
    {
        x = ((ULONG)event->ie_Prev1DownCode << 8) |
            event->ie_Prev1DownQual;
        y = ((ULONG)event->ie_Prev2DownCode << 8) |
            event->ie_Prev2DownQual;
    }
    else
    {
        x = event->ie_X;
        y = event->ie_Y;
    }

    fields[0] = event->ie_Class;
    fields[1] = event->ie_SubClass;
    fields[2] = event->ie_Code;
    fields[3] = event->ie_Qualifier;
    fields[4] = x;
    fields[5] = y;
    fields[6] = event->ie_TimeStamp.tv_secs;
    fields[7] = event->ie_TimeStamp.tv_micro;

    /* Eight longs of at most 11 characters each fit the report */
    RawDoFmt("\x9b%ld;%ld;%ld;%ld;%ld;%ld;%ld;%ld|", (RAWARG)fields,
        (APTR)putch, &p);
    len = strlen(report);

    if (len <= CON_INPUTBUF_SIZE - icu->numStoredChars)
    {
        CopyMem(report, icu->inputBuf + icu->numStoredChars, len);
        icu->numStoredChars += len;
        answer_requests(unit, ConsoleDevice);
    }

    return TRUE;
}

static ULONG pasteData(struct intConUnit *icu,
    struct ConsoleBase *ConsoleDevice)
{
    /* Check if we have data to paste to the input buffer */
    struct intPasteData *pd =
        (struct intPasteData *)GetHead(&icu->pasteData);
    if (pd)
    {
        ULONG tocopy =
            MIN(pd->pasteBufferSize - icu->pasteBufferPos,
            CON_INPUTBUF_SIZE - icu->numStoredChars);

        CopyMem(pd->pasteBuffer + icu->pasteBufferPos,
            icu->inputBuf + icu->numStoredChars, tocopy);
        icu->numStoredChars += tocopy;
        icu->pasteBufferPos += tocopy;

        if (icu->pasteBufferPos >= pd->pasteBufferSize)
        {
            FreeMem(pd->pasteBuffer, pd->pasteBufferSize);
            Remove((struct Node *)pd);
            FreeMem(pd, sizeof(struct intPasteData));
            icu->pasteBufferPos = 0;
        }

        answer_requests((APTR) icu, ConsoleDevice);
        return tocopy;
    }
    return 0;
}

#define MAPRAWKEY_BUFSIZE 80

/* A key press, or a window's close: characters for the unit's reader */
static VOID handle_key(Object *unit, struct InputEvent *ie,
    struct ConsoleBase *ConsoleDevice)
{
    UBYTE inputBuf[MAPRAWKEY_BUFSIZE + 1];
    LONG actual;
    ULONG tocopy;
    struct InputEvent e;

    /* Mouse wheel support */
    if (ie->ie_Code == RAWKEY_NM_WHEEL_UP ||
        ie->ie_Code == RAWKEY_NM_WHEEL_DOWN)
    {
        e.ie_EventAddress =
            (ie->ie_Code == RAWKEY_NM_WHEEL_UP) ? (APTR) 1 : (APTR) 2;
        e.ie_Class = IECLASS_GADGETDOWN;
        Console_HandleGadgets(unit, &e);
        e.ie_Class = IECLASS_GADGETUP;
        Console_HandleGadgets(unit, &e);
        return;
    }

    /* Convert it to ANSI chars */

    if (ie->ie_Class == IECLASS_CLOSEWINDOW)
    {
        /* this is a hack. It would actually be up to the
         * console.device user (CON: handler for example) to
         * activate CLOSEWINDOW raw events (SET RAW EVENTS cmd)
         * and then look out for this in the input stream
         * (CMD_READ) */
        inputBuf[0] = 28;   /* CTRL-\ */
        actual = 1;
    }
    else
    {
        actual = RawKeyConvert(ie, inputBuf, MAPRAWKEY_BUFSIZE,
            &CU(unit)->cu_KeyMapStruct);

        if (ie->ie_Qualifier == IEQUALIFIER_RCOMMAND && actual == 1)
        {
            switch (inputBuf[0])
            {
            case 'c':
                Console_Copy(unit);
                actual = 0;
                break;
            case 'v':
                Console_Paste(unit);
                /* We have likely put something into the input buffer */
                if (ICU(unit)->numStoredChars)
                    answer_requests(unit, ConsoleDevice);
                actual = 0;
                break;
            }
        }
    }

    if (actual > 0)
    {
        /* Let the console update input-related visual state (for
         * example, dropping a text selection) before the character is
         * handed to its reader. */
        Console_HandleGadgets(unit, ie);

        /* Copy received characters to the console unit input buffer. If
         * the buffer is full, then console input will be lost */
        tocopy = MIN(actual, CON_INPUTBUF_SIZE - ICU(unit)->numStoredChars);

        CopyMem(inputBuf, ICU(unit)->inputBuf + ICU(unit)->numStoredChars,
            tocopy);

        ICU(unit)->numStoredChars += tocopy;

        answer_requests(unit, ConsoleDevice);
    }
}

VOID consoleTaskEntry(VOID)
{
    struct ConsoleBase *ConsoleDevice =
        (struct ConsoleBase *)FindTask(NULL)->tc_UserData;
    BOOL success = FALSE;
    ULONG waitsigs, wakeupsig, commandsig, inputsig;
    BYTE sig;

    /* CD input handler puts data into this port */
    struct MsgPort *inputport;


    /* Used for input.device */
    struct MsgPort *inputmp;

    /* The command port signals the task from now on */
    sig = AllocSignal(-1);
    ConsoleDevice->commandPort.mp_SigBit = sig;
    ConsoleDevice->commandPort.mp_SigTask = FindTask(NULL);
    ConsoleDevice->commandPort.mp_Flags = PA_SIGNAL;

    /* Add the CDInputHandler to input.device's list of input handlers */
    inputmp = CreateMsgPort();
    if (sig >= 0 && inputmp)
    {
        struct IOStdReq *inputio;
        inputio =
            (struct IOStdReq *)CreateIORequest(inputmp,
            sizeof(struct IOStdReq));
        if (inputio)
        {
            /* Open the input.device */
            if (!OpenDevice("input.device", -1, (struct IORequest *)inputio,
                    0UL))
            {
                /* Initialize the inputhandler itself */
                ConsoleDevice->inputHandler = initCDIH(ConsoleDevice);
                if (ConsoleDevice->inputHandler)
                {
                    inputio->io_Data = ConsoleDevice->inputHandler;
                    inputio->io_Command = IND_ADDHANDLER;

                    DoIO((struct IORequest *)inputio);
                    success = TRUE;
                }
                CloseDevice((struct IORequest *)inputio);

            }

            DeleteIORequest((struct IORequest *)inputio);
        }

        DeleteMsgPort(inputmp);
    }

    /* if not successful, throw an alert */
    if (!success)
    {
        Alert(AT_DeadEnd | AN_ConsoleDev | AG_OpenDev | AO_Unknown);
    }

    /* Get console.device input handler's port */
    inputport = ConsoleDevice->consIHData.inputPort;

    inputsig = 1L << inputport->mp_SigBit;
    commandsig = 1L << sig;

    waitsigs = inputsig | commandsig | SIGBREAKF_CTRL_D;

    /* Requests sent before the port had its signal */
    Signal(FindTask(NULL), commandsig);

    for (;;)
    {
        wakeupsig = Wait(waitsigs);

        ObtainSemaphore(&ConsoleDevice->consoleTaskLock);

        if (wakeupsig & SIGBREAKF_CTRL_D)
        {
            struct IOStdReq *req;
            BOOL active = TRUE;
            /* console.device pasted data? */
            while (active)
            {
                active = FALSE;
                ForeachNode(&ConsoleDevice->readRequests, req)
                {
                    if (pasteData(ICU(req->io_Unit), ConsoleDevice))
                    {
                        active = TRUE;
                        break;
                    }
                }
            }
        }

        if (wakeupsig & inputsig)
        {
            /* A message from the console device input handler */
            struct cdihMessage *cdihmsg;

            while ((cdihmsg = (struct cdihMessage *)GetMsg(inputport)))
            {
                /* Check that the ConUnit has not been disposed,
                   while the message was passed
                 */
                if (lockconunit(cdihmsg->unit, ConsoleDevice))
                {
                    BOOL rawEvent = report_raw_event(cdihmsg->unit,
                        &cdihmsg->ie, ConsoleDevice);

                    switch (cdihmsg->ie.ie_Class)
                    {
                    case IECLASS_CLOSEWINDOW:
                    case IECLASS_RAWKEY:
                        if (!rawEvent)
                            handle_key(cdihmsg->unit, &cdihmsg->ie,
                                ConsoleDevice);
                        break;

                    case IECLASS_GADGETDOWN:
                    case IECLASS_GADGETUP:
                    case IECLASS_TIMER:
                    case IECLASS_RAWMOUSE:
                    case IECLASS_ACTIVEWINDOW:
                    case IECLASS_INACTIVEWINDOW:
                        Console_HandleGadgets(cdihmsg->unit,
                            &(cdihmsg->ie));
                        break;

                    case IECLASS_REFRESHWINDOW:
                    case IECLASS_SIZEWINDOW:
                        /* Intentionally empty (Begin|End)Refresh()
                         * pair, as we are required to call them to keep
                         * Intuition informed, but we don't want to be
                         * restricted to rendering just in the damaged
                         * area (WFLG_NOCAREREFRESH is another
                         * possibility, but it's not our window). */
                        BeginRefresh(WINDOW(cdihmsg->unit));
                        EndRefresh(WINDOW(cdihmsg->unit), TRUE);

                        /* Refresh window contents */
                        Console_NewWindowSize(cdihmsg->unit);
                        break;
                    } /* switch(cdihmsg->ie.ie_Class) */

                    /*
                     * CloseDevice() needs the unit-list lock exclusively
                     * before removing the unit.  Keep our shared lock until
                     * all accesses through cdihmsg->unit are complete.
                     */
                    ReleaseSemaphore(&ConsoleDevice->unitListLock);
                } /* if (lockconunit(cdihmsg->unit, ConsoleDevice)) */

                /* The input handler sent this asynchronously (it must never
                   wait for us, see cdihandler()); we own the message. */
                FreeMem(cdihmsg, sizeof(struct cdihMessage));
            } /* while ((cdihmsg = (struct cdihMessage *)GetMsg(inputport))) */
        } /* if (wakeupsig & inputsig) */

        if (wakeupsig & commandsig)
        {
            /* We got a command from the outside. Investigate it */
            struct IOStdReq *req;

            while ((req =
                    (struct IOStdReq *)GetMsg(&ConsoleDevice->commandPort)))
            {
                /* Only CMD_READ comes here */
                pasteData(ICU(req->io_Unit), ConsoleDevice);

                if (0 != ICU(req->io_Unit)->numStoredChars)
                {
                    answer_read_request(req, ConsoleDevice);
                }
                else
                {
                    /* Not enough bytes in the buffer to fill the request,
                     * put it on hold. ioReq already removed from the
                     * queue with GetMsg() */
                    AddTail((struct List *)&ConsoleDevice->readRequests,
                        (struct Node *)req);
                }
            } /* while ((req = (struct IOStdReq *)GetMsg(ConsoleDevice->commandPort))) */

        } /* if (wakeupsig & commandsig) */

        ReleaseSemaphore(&ConsoleDevice->consoleTaskLock);
    } /* forever */
}

/********** lockconunit()  *******************************/

/*
 * Checks that the supplied unit is still in the console list.  If found,
 * the shared unit-list lock remains held so the object cannot be removed or
 * disposed until the caller has finished using it.
 */
static BOOL lockconunit(Object *unit, struct ConsoleBase *ConsoleDevice)
{
    Object *o, *ostate;
    BOOL found = FALSE;

    ObtainSemaphoreShared(&ConsoleDevice->unitListLock);

    ostate = (Object *) ConsoleDevice->unitList.mlh_Head;
    while ((o = NextObject(&ostate)) && (!found))
    {
        if (o == unit)
        {
            found = TRUE;
        }
    }

    if (!found)
        ReleaseSemaphore(&ConsoleDevice->unitListLock);

    return found;
}

/******** answer_read_request() ***************************/

static void answer_read_request(struct IOStdReq *req,
    struct ConsoleBase *ConsoleDevice)
{
    Object *unit;

    /* This function assumes that there are at least one character
       available in the unitsinput buffer
     */

    unit = (Object *) req->io_Unit;

    req->io_Actual = MIN(ICU(unit)->numStoredChars, req->io_Length);

    /* Copy characters from the buffer into the request */
    CopyMem((APTR) ICU(unit)->inputBuf, req->io_Data, req->io_Actual);

    ICU(unit)->numStoredChars -= req->io_Actual;

    /* Move the rest of the bytes to the start of the buffer */
    if (ICU(unit)->numStoredChars)
        memmove(ICU(unit)->inputBuf, ICU(unit)->inputBuf + req->io_Actual,
            ICU(unit)->numStoredChars);

    req->io_Error = 0;

    /* The caller removes req from the read list when it was there */
    ReplyMsg((struct Message *)req);
}
