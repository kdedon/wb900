/*
    Copyright (C) 1995-2014, The AROS Development Team. All rights reserved.

    Desc:
*/

#include <proto/exec.h>

#include <exec/lists.h>
#include <exec/interrupts.h>
#include <exec/alerts.h>
#include <exec/memory.h>
#include <devices/inputevent.h>
#include <devices/input.h>
#include <devices/timer.h>
#include <devices/keyboard.h>
#include <devices/gameport.h>

#include "input_intern.h"

#define SEND_INPUT_REQUEST(io, ie, cmd) \
    io->io_Command = cmd;                       \
    io->io_Data = (APTR)ie;                     \
    io->io_Length = sizeof (struct InputEvent); \
    SendIO((struct IORequest *)io)


#define SEND_KBD_REQUEST(kbdio, kbdie) \
    SEND_INPUT_REQUEST(kbdio, kbdie, KBD_READEVENT)
#define SEND_GPD_REQUEST(gpdio, gpdie) \
    SEND_INPUT_REQUEST(gpdio, gpdie, GPD_READEVENT)

#define SEND_TIMER_REQUEST(timerio)                     \
        timerio->tr_node.io_Command = TR_ADDREQUEST;    \
        timerio->tr_time.tv_secs = 0;                   \
        timerio->tr_time.tv_micro = 100000;             \
        SendIO((struct IORequest *)timerio)

#define SEND_KEYTIMER_REQUEST(timerio,time)             \
        timerio->tr_node.io_Command = TR_ADDREQUEST;    \
        timerio->tr_time.tv_secs = time.tv_secs;        \
        timerio->tr_time.tv_micro = time.tv_micro;      \
        SendIO((struct IORequest *)timerio)

#define ABORT_KEYTIMER_REQUEST \
        if (!CheckIO(&keytimerio->tr_node)) AbortIO(&keytimerio->tr_node); \
        WaitIO(&keytimerio->tr_node); \
        SetSignal(0, keytimersig);

#define KEY_QUALIFIERS (IEQUALIFIER_LSHIFT     | IEQUALIFIER_RSHIFT   | \
                        IEQUALIFIER_CAPSLOCK   | IEQUALIFIER_CONTROL  | \
                        IEQUALIFIER_RALT       | IEQUALIFIER_LALT     | \
                        IEQUALIFIER_LCOMMAND   | IEQUALIFIER_RCOMMAND | \
                        IEQUALIFIER_NUMERICPAD /* | IEQUALIFIER_REPEAT */)

#define MOUSE_QUALIFIERS (IEQUALIFIER_LEFTBUTTON | IEQUALIFIER_RBUTTON | \
                          IEQUALIFIER_MIDBUTTON)


/**********************
**  ForwardEvents()  **
**********************/

/* Forwards a chain of events to the inputhandlers */
VOID ForwardQueuedEvents(struct inputbase *InputDevice)
{
    struct InputEvent *ie_chain;
    struct Interrupt *ihiterator;

    ie_chain = GetEventsFromQueue(InputDevice);
    if (ie_chain)
    {
        for (ihiterator = (struct Interrupt *)InputDevice->HandlerList.mlh_Head;
            ihiterator->is_Node.ln_Succ;
            ihiterator = (struct Interrupt *)ihiterator->is_Node.ln_Succ)
        {
            ie_chain = (*(struct InputEvent *(*)())ihiterator->is_Code)
                (ie_chain, ihiterator->is_Data);
        }
    }

    return;
}


/***********************************
** Input device task entry point  **
***********************************/
void ProcessEvents(VOID)
{
    struct inputbase *InputDevice;
    ULONG commandsig, kbdsig, wakeupsigs;
    ULONG gpdsig, timersig, keytimersig;
    struct MsgPort *timermp, *keytimermp;
    struct timerequest *timerio, *keytimerio;

    struct MsgPort *kbdmp, *gpdmp;
    struct IOStdReq *kbdio, *gpdio;
    struct InputEvent *kbdie, *gpdie, keyrepeatie, moveie;
    struct Device *TimerBase;
    BYTE sigbit;

    struct GamePortTrigger mouseTrigger = {
        GPTF_DOWNKEYS | GPTF_UPKEYS,
        0,                      /* We don't really care about time triggers */
        0,                      /* Report any mouse change */
        0
    };

    BYTE controllerType = GPCT_MOUSE;
    BYTE keyrepeat_state = 0;

    InputDevice = (struct inputbase *)FindTask(NULL)->tc_UserData;

    /************** Take the command port *******************/

    sigbit = AllocSignal(-1);
    if (sigbit == -1)
        Alert(AT_DeadEnd | AG_NoSignal | AN_Unknown);
    commandsig = 1L << sigbit;

    Forbid();
    InputDevice->CommandPort.mp_SigBit = sigbit;
    InputDevice->CommandPort.mp_SigTask = FindTask(NULL);
    InputDevice->CommandPort.mp_Flags = PA_SIGNAL;
    Permit();

    /* Requests sent before the port had a signal */
    SetSignal(commandsig, commandsig);

    /************** Open timer.device *******************/

    timermp = CreateMsgPort();
    keytimermp = CreateMsgPort();

    if (!timermp || !keytimermp)
        Alert(AT_DeadEnd | AG_NoMemory | AN_Unknown);

    timerio =
        (struct timerequest *)CreateIORequest(timermp,
        sizeof(struct timerequest));
    keytimerio =
        (struct timerequest *)CreateIORequest(keytimermp,
        sizeof(struct timerequest));
    if (!timerio || !keytimerio)
        Alert(AT_DeadEnd | AG_NoMemory | AN_Unknown);

    if (0 != OpenDevice(TIMERNAME, UNIT_VBLANK, (struct IORequest *)timerio,
            0))
        Alert(AT_DeadEnd | AG_OpenDev | AN_Unknown);

    TimerBase = timerio->tr_node.io_Device;

    *keytimerio = *timerio;
    keytimerio->tr_node.io_Message.mn_ReplyPort = keytimermp;

    /************** Open keyboard.device *******************/
    kbdmp = CreateMsgPort();
    if (!kbdmp)
        Alert(AT_DeadEnd | AG_NoMemory | AN_Unknown);

    kbdio =
        (struct IOStdReq *)CreateIORequest(kbdmp, sizeof(struct IOStdReq));
    if (!kbdio)
        Alert(AT_DeadEnd | AG_NoMemory | AN_Unknown);

    if (0 != OpenDevice(KEYBOARDNAME, 0, (struct IORequest *)kbdio, 0))
        Alert(AT_DeadEnd | AG_OpenDev | AN_Unknown);

    kbdie = AllocMem(sizeof(struct InputEvent), MEMF_PUBLIC | MEMF_CLEAR);
    if (!kbdie)
        Alert(AT_DeadEnd | AG_NoMemory | AN_Unknown);


    /************** Open gameport.device *******************/
    gpdmp = CreateMsgPort();
    if (!gpdmp)
        Alert(AT_DeadEnd | AG_NoMemory | AN_Unknown);

    gpdio =
        (struct IOStdReq *)CreateIORequest(gpdmp, sizeof(struct IOStdReq));
    if (!gpdio)
        Alert(AT_DeadEnd | AG_NoMemory | AN_Unknown);

    gpdie = AllocMem(sizeof(struct InputEvent), MEMF_PUBLIC | MEMF_CLEAR);
    if (!gpdie)
        Alert(AT_DeadEnd | AG_NoMemory | AN_Unknown);

    /* gameport.device exists only with the HR card: run without a mouse */
    gpdsig = 0;
    if (0 == OpenDevice(GAMEPORTNAME, 0, (struct IORequest *)gpdio, 0))
    {
        /* Set the controller type */
        gpdio->io_Command = GPD_SETCTYPE;
        gpdio->io_Data = (APTR) &controllerType;
        gpdio->io_Length = sizeof(BYTE);
        DoIO((struct IORequest *)gpdio);

        /* Set the gameport trigger */
        gpdio->io_Command = GPD_SETTRIGGER;
        gpdio->io_Data = &mouseTrigger;
        gpdio->io_Length = sizeof(struct GamePortTrigger);
        DoIO((struct IORequest *)gpdio);

        SEND_GPD_REQUEST(gpdio, gpdie);
        gpdsig = 1L << gpdmp->mp_SigBit;
    }

    /* Send an initial request to the keyboard device */
    SEND_KBD_REQUEST(kbdio, kbdie);

    /* ...and to timer.device */
    SEND_TIMER_REQUEST(timerio);

    kbdsig = 1L << kbdmp->mp_SigBit;
    timersig = 1L << timermp->mp_SigBit;
    keytimersig = 1L << keytimermp->mp_SigBit;

    for (;;)
    {
        wakeupsigs = Wait(commandsig |
            (InputDevice->Stopped ? 0 :
                (kbdsig | gpdsig | timersig | keytimersig)));

        if (wakeupsigs & commandsig)
        {
            struct IOStdReq *ioreq;

            /* Get all commands from the port */
            while ((ioreq =
                    (struct IOStdReq *)GetMsg(&InputDevice->CommandPort)))
            {

                switch (ioreq->io_Command)
                {
                case CMD_STOP:
                    InputDevice->Stopped = TRUE;
                    break;

                case CMD_START:
                case CMD_RESET:
                    InputDevice->Stopped = FALSE;
                    break;

                case IND_ADDHANDLER:
                    Enqueue((struct List *)&(InputDevice->HandlerList),
                        (struct Node *)ioreq->io_Data);
                    break;

                case IND_REMHANDLER:
                    Remove((struct Node *)ioreq->io_Data);
                    break;

                case IND_ADDEVENT:
                    {
                        /*
                         * IND_ADDEVENT command allows client to send multiple
                         * RAWKEY or RAWMOUSE events to the input.device. All
                         * other classes will be ignored.
                         */
                        struct InputEvent *ie =
                            (struct InputEvent *)ioreq->io_Data;
                        ULONG ie_cnt =
                            ioreq->io_Length / sizeof(struct InputEvent);

                        /* Update the current qualifier */
                        InputDevice->ActQualifier = ie->ie_Qualifier;

                        /* For each event... */
                        for (; ie_cnt; ie_cnt--, ie++)
                        {
                            /* Of class RAWMOUSE or RAWKEY... */
                            if (ie->ie_Class == IECLASS_RAWMOUSE
                                || ie->ie_Class == IECLASS_RAWKEY)
                            {
                                ie->ie_NextEvent = NULL;

                                if (ie->ie_Class == IECLASS_RAWKEY)
                                {
                                    if (!IsQualifierKey(ie->ie_Code))
                                    {
                                        if (keyrepeat_state > 0)
                                        {
                                            ABORT_KEYTIMER_REQUEST;
                                            keyrepeat_state = 0;
                                        }

                                        if (!(ie->ie_Code & IECODE_UP_PREFIX))
                                        {
                                            if (IsRepeatableKey(ie->
                                                    ie_Code))
                                            {
                                                keyrepeatie = *ie;

                                                SEND_KEYTIMER_REQUEST
                                                    (keytimerio,
                                                    InputDevice->
                                                    KeyRepeatThreshold);
                                                keyrepeat_state = 1;

                                            }
                                        }
                                    }
                                }

                                /* If the event's qualifier differs from the
                                   current one, fire the events */
                                if (InputDevice->ActQualifier !=
                                    ie->ie_Qualifier)
                                {
                                    UWORD q = ie->ie_Qualifier;
                                    ForwardQueuedEvents(InputDevice);

                                    /* And set new qualifier */
                                    InputDevice->ActQualifier = q;
                                }

                                /* Set the timestamp */
                                GetSysTime(&(ie->ie_TimeStamp));

                                /* and enqueue */
                                AddEQTail(ie, InputDevice);
                            }
                        }
                        /* In case some events are still in the queue, fire
                           them all up */
                        ForwardQueuedEvents(InputDevice);
                    }
                    break;

                case IND_WRITEEVENT:
                    {
                        struct InputEvent *ie;

                        ie = (struct InputEvent *)ioreq->io_Data;

                        ie->ie_NextEvent = NULL;
                        /* Add a timestamp to the event */
                        GetSysTime(&(ie->ie_TimeStamp));

                        /* Add event to queue */
                        AddEQTail((struct InputEvent *)ioreq->io_Data,
                            InputDevice);

                        /* Forward event (and possible others in the queue) */
                        ForwardQueuedEvents(InputDevice);
                    } break;

                case IND_SETTHRESH:
                    InputDevice->KeyRepeatThreshold.tv_secs =
                        ((struct timerequest *)ioreq)->tr_time.tv_secs;
                    InputDevice->KeyRepeatThreshold.tv_micro =
                        ((struct timerequest *)ioreq)->tr_time.tv_micro;
                    break;

                case IND_SETPERIOD:
                    InputDevice->KeyRepeatInterval.tv_secs =
                        ((struct timerequest *)ioreq)->tr_time.tv_secs;
                    InputDevice->KeyRepeatInterval.tv_micro =
                        ((struct timerequest *)ioreq)->tr_time.tv_micro;
                    break;

                }

                ReplyMsg((struct Message *)ioreq);
            }
        }

        if (!InputDevice->Stopped && (wakeupsigs & timersig))
        {
            struct InputEvent timer_ie;

            GetMsg(timermp);

            timer_ie.ie_NextEvent = NULL;
            timer_ie.ie_Class = IECLASS_TIMER;
            timer_ie.ie_SubClass = 0;
            timer_ie.ie_Code = 0;
            timer_ie.ie_Qualifier = InputDevice->ActQualifier;
            timer_ie.ie_position.ie_addr = 0;

            /* Add a timestamp to the event */
            GetSysTime(&(timer_ie.ie_TimeStamp));

            AddEQTail(&timer_ie, InputDevice);
            ForwardQueuedEvents(InputDevice);

            SEND_TIMER_REQUEST(timerio);
        }

        if (!InputDevice->Stopped && (wakeupsigs & keytimersig))
        {
            struct InputEvent ie;

            GetMsg(keytimermp);

            keyrepeat_state = 2;

            /* InputHandlers can change inputevents, so send a clone */
            ie = keyrepeatie;
            ie.ie_NextEvent = NULL;     /* !! */
            ie.ie_Qualifier |= IEQUALIFIER_REPEAT;
            GetSysTime(&ie.ie_TimeStamp);

            AddEQTail(&ie, InputDevice);

            /* Forward event (and possible others in the queue) */
            ForwardQueuedEvents(InputDevice);

            SEND_KEYTIMER_REQUEST(keytimerio,
                InputDevice->KeyRepeatInterval);
        }

        /* The keyboard's own repeats are dropped: repeat is timed here */
        if (!InputDevice->Stopped && (wakeupsigs & kbdsig)
            && GetMsg(kbdmp))
        {
            if (kbdio->io_Error == 0
                && !(kbdie->ie_Qualifier & IEQUALIFIER_REPEAT))
            {
                InputDevice->ActQualifier &= ~KEY_QUALIFIERS;
                InputDevice->ActQualifier |=
                    (kbdie->ie_Qualifier & KEY_QUALIFIERS);

                kbdie->ie_Qualifier &= ~MOUSE_QUALIFIERS;
                kbdie->ie_Qualifier |=
                    (InputDevice->ActQualifier & MOUSE_QUALIFIERS);

                /* keyboard.device stamps the tick count */
                kbdie->ie_NextEvent = NULL;
                GetSysTime(&kbdie->ie_TimeStamp);

                /* Add event to queue */
                AddEQTail(kbdie, InputDevice);

                if (!IsQualifierKey(kbdie->ie_Code))
                {
                    if (keyrepeat_state > 0)
                    {
                        ABORT_KEYTIMER_REQUEST;
                        keyrepeat_state = 0;
                    }

                    if (!(kbdie->ie_Code & IECODE_UP_PREFIX))
                    {
                        if (IsRepeatableKey(kbdie->ie_Code))
                        {
                            keyrepeatie = *kbdie;

                            SEND_KEYTIMER_REQUEST(keytimerio,
                                InputDevice->KeyRepeatThreshold);
                            keyrepeat_state = 1;

                        }
                    }
                }

                /* Forward event (and possible others in the queue) */
                ForwardQueuedEvents(InputDevice);
            }

            /* Wait for some more events */
            SEND_KBD_REQUEST(kbdio, kbdie);
        }

        if (!InputDevice->Stopped && (wakeupsigs & gpdsig)
            && GetMsg(gpdmp))
        {
            if (gpdio->io_Error == 0)
            {
                InputDevice->ActQualifier &= ~MOUSE_QUALIFIERS;
                InputDevice->ActQualifier |=
                    (gpdie->ie_Qualifier & MOUSE_QUALIFIERS);

                gpdie->ie_Qualifier &= ~KEY_QUALIFIERS;
                gpdie->ie_Qualifier |=
                    (InputDevice->ActQualifier & KEY_QUALIFIERS);

                /* Gameport just returns the tick count in ie_TimeStamp;
                   we therefore must add a real timestamp ourselves */
                gpdie->ie_NextEvent = NULL;
                GetSysTime(&gpdie->ie_TimeStamp);

                /* A button event carries the motion of its tick, which
                   handlers take only from IECODE_NOBUTTON events: the
                   motion goes first as an event of its own */
                if (gpdie->ie_Class == IECLASS_RAWMOUSE
                    && gpdie->ie_Code != IECODE_NOBUTTON
                    && (gpdie->ie_X != 0 || gpdie->ie_Y != 0))
                {
                    moveie = *gpdie;
                    moveie.ie_Code = IECODE_NOBUTTON;
                    AddEQTail(&moveie, InputDevice);
                    gpdie->ie_X = gpdie->ie_Y = 0;
                }

                /* Add event to queue */
                AddEQTail(gpdie, InputDevice);

                /* Forward event (and possible others in the queue) */
                ForwardQueuedEvents(InputDevice);
            }

            /* Wait for some more events */
            SEND_GPD_REQUEST(gpdio, gpdie);
        }
    }
}
