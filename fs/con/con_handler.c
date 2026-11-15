/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.
*/

/*
 * CON: and RAW:, windows on console.device.  A boot module: its romtag
 * lists the devices CON and RAW, and DOS starts each one's handler on
 * first use.  That process takes every open of its device and starts a
 * window process for it, which serves the new handle from then on.
 *
 *   DOS --FINDINPUT--> CON: process --packet--> window process
 *                                                  |
 *   DOS <--READ/WRITE/END on fh_Type---------------+
 */

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/utility.h>
#include <proto/console.h>
#include <proto/intuition.h>
#include <proto/gadtools.h>

#include <exec/libraries.h>
#include <exec/resident.h>
#include <exec/memory.h>
#include <exec/io.h>
#include <exec/errors.h>
#include <utility/tagitem.h>
#include <dos/dostags.h>
#include <intuition/intuition.h>
#include <libraries/gadtools.h>
#include <devices/conunit.h>

#include "con_handler_intern.h"
#include "support.h"

struct DosLibrary *DOSBase;

enum
{
    MEN_CONSOLE_CLOSE = 1,
    MEN_CONSOLE_COPY,
    MEN_CONSOLE_PASTE
};

/* The menu of a console window: for every entry its kind, the MEN_CONSOLE_
   number it reports when picked (0 for titles and separators), its keyboard
   shortcut and its label. MakeConWindow() builds the gadtools NewMenu array
   from this. */
static const struct conmenu
{
    UBYTE type;
    UBYTE id;
    char key[2];
    CONST_STRPTR label;
}
conmenu[] =
{
    { NM_TITLE, 0,                 "",  "Console"        },
    { NM_ITEM,  MEN_CONSOLE_CLOSE, "Q", "Close"          },
    { NM_TITLE, 0,                 "",  "Edit"           },
    { NM_ITEM,  MEN_CONSOLE_COPY,  "c", "Copy"           },
    { NM_ITEM,  MEN_CONSOLE_PASTE, "v", "Paste"          }
};
#define NUM_CONMENU (sizeof(conmenu) / sizeof(conmenu[0]))

#define ioReq(x) ((struct IORequest *)x)

static const struct NewWindow default_nw =
{
    0,              /* LeftEdge */
    0,              /* TopEdge */
    -1,             /* Width */
    -1,             /* Height */
    1,              /* DetailPen */
    0,              /* BlockPen */
    0,              /* IDCMP */
    WFLG_DEPTHGADGET   |
    WFLG_SIZEGADGET    |
    WFLG_DRAGBAR       |
    WFLG_SIZEBRIGHT    |
    WFLG_SMART_REFRESH |
    WFLG_ACTIVATE,
    0,              /* FirstGadget */
    0,              /* CheckMark */
    "CON:",         /* Title */
    0,              /* Screen */
    0,              /* Bitmap */
    100,            /* MinWidth */
    70,             /* MinHeight */
    32767,          /* MaxWidth */
    32767,          /* MaxHeight */
    WBENCHSCREEN    /* type */
};

/* Hand console.device an event of the window, as its input handler would */
static void con_event(struct filehandle *fh, UBYTE class, UWORD code,
    UWORD qual)
{
    struct Library *ConsoleDevice = (struct Library *)fh->conreadio->io_Device;
    struct InputEvent ie;

    memset(&ie, 0, sizeof(ie));
    ie.ie_Class = class;
    ie.ie_Code = code;
    ie.ie_Qualifier = qual;
    CDInputHandler(&ie, ConsoleDevice);
}

void close_con_window(struct filehandle *fh)
{
    if (fh->winmenu) {
        if (fh->window && fh->window->MenuStrip == fh->winmenu)
            ClearMenuStrip(fh->window);
        FreeMenus(fh->winmenu);
        fh->winmenu = NULL;
    }

    if (fh->window) {
        CloseWindow(fh->window);
        fh->window = NULL;
    }
}

static LONG MakeConWindow(struct filehandle *fh)
{
    LONG err = 0;

    if (fh->otherwindow == NULL) {
        struct TagItem win_tags[5];

        win_tags[0].ti_Tag = WA_PubScreen;
        win_tags[0].ti_Data = 0;
        win_tags[1].ti_Tag = WA_AutoAdjust;
        win_tags[1].ti_Data = TRUE;
        win_tags[2].ti_Tag = WA_PubScreenName;
        win_tags[2].ti_Data = (IPTR) fh->screenname;
        win_tags[3].ti_Tag = WA_PubScreenFallBack;
        win_tags[3].ti_Data = TRUE;
        win_tags[4].ti_Tag = TAG_DONE;

        /*  Autoadjust doesn't enforce the window's width and height to be larger than
         minwidth and minheight, so we set it here to avoid crashes in devs/console
         if a user does e.g. dir >con:0/0/0/0
         */
        fh->nw.Width = fh->nw.Width > fh->nw.MinWidth ? fh->nw.Width : -1;
        fh->nw.Height = fh->nw.Height > fh->nw.MinHeight ? fh->nw.Height :
            fh->nw.MinHeight;

        fh->window = OpenWindowTagList(&fh->nw, win_tags);
    }
    else
        fh->window = fh->otherwindow;

    if (fh->window)
    {
        struct TagItem menu_tags[2];
        struct NewMenu newconmenu[NUM_CONMENU + 1];
        ULONG i;

        menu_tags[0].ti_Tag = GTMN_NewLookMenus;
        menu_tags[0].ti_Data = TRUE;
        menu_tags[1].ti_Tag = TAG_DONE;

        /* Build the menu from its description. The entry after the last one
           stays cleared, which is NM_END. */
        memset(newconmenu, 0, sizeof(newconmenu));
        for (i = 0; i < NUM_CONMENU; i++) {
            newconmenu[i].nm_Type = conmenu[i].type;
            newconmenu[i].nm_Label = (STRPTR)conmenu[i].label;
            if (conmenu[i].key[0])
                newconmenu[i].nm_CommKey = (STRPTR)conmenu[i].key;
            newconmenu[i].nm_UserData = (APTR)(IPTR)conmenu[i].id;

            /* Without a close gadget there is nothing to close */
            if (conmenu[i].id == MEN_CONSOLE_CLOSE &&
                !(fh->nw.Flags & WFLG_CLOSEGADGET))
                newconmenu[i].nm_Flags = NM_ITEMDISABLED;
        }

        if (fh->gtbase && fh->otherwindow == NULL &&
            (fh->winmenu = CreateMenusA(newconmenu, menu_tags))) {
            APTR vi = GetVisualInfoA(fh->window->WScreen, NULL);
            if (vi) {
                if (LayoutMenusA(fh->winmenu, vi, menu_tags))
                    SetMenuStrip(fh->window, fh->winmenu);
                FreeVisualInfo(vi);
            }
        }
        fh->conreadio->io_Data = (APTR) fh->window;
        fh->conreadio->io_Length = sizeof(struct Window);

        if (0 == OpenDevice(CONSOLENAME, CONU_SNIPMAP, ioReq(fh->conreadio), 0)) {
            static const UBYTE lf_on[] =
            { 0x9B, 0x32, 0x30, 0x68 }; /* Set linefeed mode    */

            fh->flags |= FHFLG_CONSOLEDEVICEOPEN;

            fh->conwriteio = *fh->conreadio;
            fh->conwriteio.io_Message.mn_ReplyPort = fh->conwritemp;

            /* Turn the console into LF+CR mode so that both
             linefeed and carriage return is done on
             */
            fh->conwriteio.io_Command = CMD_WRITE;
            fh->conwriteio.io_Data = (APTR) lf_on;
            fh->conwriteio.io_Length = 4;

            DoIO(ioReq(&fh->conwriteio));
        }
        else
        {
            err = ERROR_INVALID_RESIDENT_LIBRARY;
            /* The pointer must not survive the close: MakeSureWinIsOpen
             * treats a non-NULL window as an open one. */
            close_con_window(fh);
        }

    } /* if (fh->window) */
    else {
        err = ERROR_NO_FREE_STORE;
    }

    return err;
}

static BOOL MakeSureWinIsOpen(struct filehandle *fh)
{
    if (fh->window)
        return TRUE;
    return MakeConWindow(fh) == 0;
}

static void close_con(struct filehandle *fh)
{
    /* Clean up */

    if (fh->timerreq) {
        if (fh->timerreq->tr_node.io_Device)
            CloseDevice((struct IORequest *) fh->timerreq);
        DeleteIORequest((struct IORequest *) fh->timerreq);
    }

    DeleteMsgPort(fh->timermp);

    if (fh->flags & FHFLG_CONSOLEDEVICEOPEN)
        CloseDevice((struct IORequest *) fh->conreadio);

    close_con_window(fh);

    DeleteIORequest(ioReq(fh->conreadio));

    if (fh->conreadmp) {
        FreeSignal((BYTE)fh->conreadmp->mp_SigBit);
        FreeSignal((BYTE)fh->conwritemp->mp_SigBit);
        FreeVec(fh->conreadmp);
    }

    if (fh->screenname)
        FreeVec(fh->screenname);
    if (fh->wintitle)
        FreeVec(fh->wintitle);
    if (fh->pastebuffer)
        FreeMem(fh->pastebuffer, PASTEBUFSIZE);

    CloseLibrary((struct Library*) fh->intuibase);
    CloseLibrary(fh->utilbase);
    if (fh->gtbase)
        CloseLibrary(fh->gtbase);

    FreeVec(fh);
}

/* Make a message port of the handler task with a signal of its own. The
   caller presets mp_SigBit to -1, so a port whose signal could not be
   allocated is recognisable when it is freed. */
static BOOL init_conport(struct MsgPort *mp, struct Task *task)
{
    mp->mp_Node.ln_Type = NT_MSGPORT;
    mp->mp_Flags = PA_SIGNAL;
    mp->mp_SigBit = AllocSignal(-1);
    if (mp->mp_SigBit == (UBYTE)-1)
        return FALSE;
    mp->mp_SigTask = task;
    NEWLIST(&mp->mp_MsgList);
    return TRUE;
}

/* The window for an open: dp_Arg3 is the name after "CON:", dp_Arg4 the
   DosList entry */
static struct filehandle *open_con(struct DosPacket *dp, LONG *perr)
{
    char *fn;
    struct filehandle *fh;
    struct DosList *dn;
    LONG err, ok = FALSE;

    dn = (struct DosList *)dp->dp_Arg4;
    *perr = ERROR_NO_FREE_STORE;
    fh = AllocVec(sizeof(struct filehandle), MEMF_PUBLIC | MEMF_CLEAR);
    if (!fh)
        return NULL;

    fh->dn = dn;

    fh->intuibase = (APTR) OpenLibrary("intuition.library", 39);
    fh->gtbase = (APTR) OpenLibrary("gadtools.library", 39);
    fh->utilbase = (APTR) OpenLibrary("utility.library", 39);

    Forbid();
    fh->inputbase = (struct Device *) FindName(&SysBase->DeviceList, "input.device");
    Permit();

    if (!fh->utilbase || !fh->intuibase || !fh->inputbase) {
        /* Only the libraries have been opened so far. close_con() closes
           them and frees fh; everything else it looks at is still NULL. */
        close_con(fh);
        return NULL;
    }

    fh->timermp = CreateMsgPort();
    if (!fh->timermp) {
        close_con(fh);
        return NULL;
    }
    fh->timerreq = (struct timerequest*) CreateIORequest(fh->timermp, sizeof(struct timerequest));
    if (!fh->timerreq) {
        close_con(fh);
        return NULL;
    }
    if (OpenDevice(TIMERNAME, UNIT_MICROHZ, (struct IORequest *) fh->timerreq, 0) != 0) {
        fh->timerreq->tr_node.io_Device = NULL;
        *perr = ERROR_DEVICE_NOT_MOUNTED;
        close_con(fh);
        return NULL;
    }

    err = 0;
    fn = (char *)dp->dp_Arg3;

    fh->contask = FindTask(0);

    NEWLIST(&fh->pendingReads);

    /* Create msgport for console.device communication */
    fh->conreadmp = AllocVec(sizeof(struct MsgPort) * 2, MEMF_PUBLIC | MEMF_CLEAR);
    if (fh->conreadmp) {
        fh->conwritemp = fh->conreadmp + 1;
        fh->conreadmp->mp_SigBit = (UBYTE)-1;
        fh->conwritemp->mp_SigBit = (UBYTE)-1;
        if (!init_conport(fh->conreadmp, fh->contask) ||
            !init_conport(fh->conwritemp, fh->contask)) {
            err = ERROR_NO_FREE_STORE;
            goto out;
        }

        fh->conreadio = (struct IOStdReq *) CreateIORequest(fh->conreadmp,
            sizeof(struct IOStdReq));
        if (fh->conreadio) {
            fh->nw = default_nw;
            fh->nw.IDCMPFlags |= IDCMP_MENUPICK;
            if (parse_filename(fh, fn, &fh->nw)) {
                if (!(fh->flags & FHFLG_AUTO)) {
                    err = MakeConWindow(fh);
                    if (!err)
                        ok = TRUE;
                }
                else
                    ok = TRUE;
            }
            else
                err = ERROR_BAD_STREAM_NAME;

            /* No cleanup here: close_con() below frees the IORequest, and
               doing it twice corrupts the allocator. */

        } /* if (fh->conreadio) */
        else {
            err = ERROR_NO_FREE_STORE;
        }

    } /* if (fh->conreadmp) */
    else {
        err = ERROR_NO_FREE_STORE;
    }

    /* The RAW entry's dol_Startup is 1 */
    if (dn->dol_misc.dol_handler.dol_Startup)
        fh->flags |= FHFLG_RAW;

out:
    if (!ok) {
        close_con(fh);
        fh = NULL;
    }

    *perr = err;
    return fh;
}

static void startread(struct filehandle *fh)
{
    if (fh->flags & FHFLG_ASYNCCONSOLEREAD)
        return;
    fh->conreadio->io_Command = CMD_READ;
    fh->conreadio->io_Data = fh->consolebuffer;
    /* console.device completes a read as soon as it has something, so
       asking for a bufferful costs nothing */
    fh->conreadio->io_Length = CONSOLEBUFFER_SIZE;
    SendIO((struct IORequest*) fh->conreadio);
    fh->flags |= FHFLG_ASYNCCONSOLEREAD;
}

static void pump_raw_input(struct filehandle *fh)
{
    while (fh->conbufferpos < fh->conbuffersize)
    {
        LONG available = fh->conbuffersize - fh->conbufferpos;
        LONG space = INPUTBUFFER_SIZE - fh->inputsize;
        LONG copylen;

        if (space <= 0)
            break;

        copylen = available < space ? available : space;
        CopyMem(fh->consolebuffer + fh->conbufferpos,
            fh->inputbuffer + fh->inputsize, copylen);
        fh->conbufferpos += copylen;
        fh->inputsize += copylen;
        fh->inputpos = fh->inputstart = fh->inputsize;

        HandlePendingReads(fh);

        /* Without a pending application read, keep the remaining device
         * bytes in consolebuffer until space is made by ACTION_READ. */
        if (fh->inputsize != 0)
            break;
    }
}

static void stopwait(struct filehandle *fh, struct DosPacket *waitingdp, ULONG result)
{
    if (waitingdp) {
        AbortIO((struct IORequest *) fh->timerreq);
        WaitIO((struct IORequest *) fh->timerreq);
        replypkt(waitingdp, result);
    }
}

static void stopread(struct filehandle *fh, struct DosPacket *waitingdp)
{
    struct Message *msg, *next_msg;

    stopwait(fh, waitingdp, DOSFALSE);

    ForeachNodeSafe(&fh->pendingReads, msg, next_msg) {
        struct DosPacket *dpr;

        Remove((struct Node *) msg);
        dpr = (struct DosPacket*) msg->mn_Node.ln_Name;
        replypkt(dpr, DOSFALSE);
    }
}

/* A window's process: its first packet is the open that made it */
static LONG CONMain(char *args, LONG len)
{
    struct MsgPort *mp = NULL;
    struct MsgPort *procmp;
    struct DosPacket *dp;
    struct Message *mn;
    struct FileHandle *dosfh;
    LONG error;
    struct filehandle *fh = NULL;
    struct FileLock *fl;
    struct DosPacket *waitingdp = NULL;

    procmp = &((struct Process*) FindTask(NULL))->pr_MsgPort;
    WaitPort(procmp);
    mn = GetMsg(procmp);
    dp = (struct DosPacket*) mn->mn_Node.ln_Name;

    /* Clients get a port of their own: they send packets to it, and DOS
     * calls the handler makes wait on the process port. */
    mp = CreateMsgPort();
    if (!mp) {
        error = ERROR_NO_FREE_STORE;
        goto end;
    }

    fh = open_con(dp, &error);
    if (!fh)
        goto end;

    /* The open itself is the first packet served */
    PutMsg(mp, mn);
    dp = NULL;
    {
        ULONG conreadmask = 1L << fh->conreadmp->mp_SigBit;
        ULONG timermask = 1L << fh->timermp->mp_SigBit;
        ULONG packetmask = 1L << mp->mp_SigBit;
        ULONG winmask;
        ULONG sigs;

        for (;;) {
            winmask = fh->window ? 1L << fh->window->UserPort->mp_SigBit : 0L;
            sigs = Wait(packetmask | conreadmask | timermask | winmask);

            if ((winmask) && (sigs & winmask)) {
                struct IntuiMessage *msg;
                while ((msg = (struct IntuiMessage *)GetMsg(fh->window->UserPort))) {
                    ULONG   msgclass = msg->Class;
                    UWORD   msgcode  = msg->Code;

                    if (msgclass == IDCMP_MENUPICK) {
                        struct MenuItem *item;
                        UWORD           men  = msgcode;

                        /* Applications using a console window may replace
                         * the handler's menu strip and request menu picks
                         * as console.device raw events.  Do not interpret
                         * their menu codes against our stale menu strip. */
                        if (fh->window->MenuStrip != fh->winmenu)
                        {
                            con_event(fh, IECLASS_MENULIST, msg->Code,
                                msg->Qualifier);
                            men = MENUNULL;
                        }

                        while(men != MENUNULL)
                        {
                            if ((item = ItemAddress(fh->winmenu, men)))
                            {
                                IPTR menu_selected = (IPTR)GTMENUITEM_USERDATA(item);
                                switch(menu_selected)
                                {
                                    case MEN_CONSOLE_CLOSE:
                                        con_event(fh, IECLASS_CLOSEWINDOW,
                                            0, 0);
                                        break;
                                    /* console.device's own Amiga-c and
                                       Amiga-v */
                                    case MEN_CONSOLE_COPY:
                                        con_event(fh, IECLASS_RAWKEY, 0x33,
                                            IEQUALIFIER_RCOMMAND);
                                        break;
                                    case MEN_CONSOLE_PASTE:
                                        con_event(fh, IECLASS_RAWKEY, 0x34,
                                            IEQUALIFIER_RCOMMAND);
                                        break;
                                }
                                men = item->NextSelect;
                            } else {
                                men = MENUNULL;
                            }
                        }
                    }
                    ReplyMsg((struct Message *)msg);
                }
            }

            if (sigs & timermask) {
                if (waitingdp && CheckIO((struct IORequest *)fh->timerreq)) {
                    WaitIO((struct IORequest *)fh->timerreq);
                    replypkt(waitingdp, DOSFALSE);
                    waitingdp = NULL;
                }
            }

            if (sigs & conreadmask) {
                if (GetMsg(fh->conreadmp)) {
                    fh->flags &= ~FHFLG_ASYNCCONSOLEREAD;
                    if (waitingdp) {
                        stopwait(fh, waitingdp, DOSTRUE);
                        waitingdp = NULL;
                    }
                    fh->conbuffersize = fh->conreadio->io_Actual;
                    fh->conbufferpos = 0;
                    /* terminate with 0 char */
                    fh->consolebuffer[fh->conbuffersize] = '\0';
                    if (fh->flags & FHFLG_RAW) {
                        /* raw mode */
                        pump_raw_input(fh);
                    } /* if (fh->flags & FHFLG_RAW) */
                    else {
                        /* Cooked mode */
                        if (process_input(fh)) {
                            /*
                             * process_input() returns TRUE when EOF was received after the WAIT console
                             * has been closed by the owner.
                             */
                            goto end;
                        }
                    } /* if (fh->flags & FHFLG_RAW) else ... */

                    if ((fh->flags & FHFLG_CONSOLEDEVICEOPEN) &&
                        (!(fh->flags & FHFLG_RAW) ||
                            fh->conbufferpos >= fh->conbuffersize))
                        startread(fh);
                }
            }

            while ((mn = GetMsg(mp))) {
                dp = (struct DosPacket*) mn->mn_Node.ln_Name;
                dp->dp_Res2 = 0;
                error = 0;
                switch (dp->dp_Type) {
                case ACTION_FH_FROM_LOCK:
                    fl = (struct FileLock *)dp->dp_Arg2;
                    if (fl->fl_Task != mp || fl->fl_Key != (IPTR) fh) {
                        replypkt2(dp, DOSFALSE, ERROR_OBJECT_NOT_FOUND);
                        break;
                    }
                    fh->usecount--;
                    FreeMem(fl, sizeof(*fl));
                    /* Fallthrough */
                case ACTION_FINDINPUT:
                case ACTION_FINDOUTPUT:
                case ACTION_FINDUPDATE:
                    dosfh = (struct FileHandle *)dp->dp_Arg1;
                    dosfh->fh_Port = (struct MsgPort *)DOSTRUE;
                    dosfh->fh_Arg1 = (SIPTR) fh;
                    /* Talk to us here from now on */
                    dosfh->fh_Type = mp;
                    fh->usecount++;
                    fh->breaktask = dp->dp_Port->mp_SigTask;
                    replypkt(dp, DOSTRUE);
                    break;
                case ACTION_COPY_DIR_FH:
                    fl = AllocMem(sizeof(*fl), MEMF_CLEAR | MEMF_PUBLIC);
                    if (fl == NULL) {
                        replypkt2(dp, 0, ERROR_NO_FREE_STORE);
                    }
                    else {
                        fh->usecount++;
                        fl->fl_Task = mp;
                        fl->fl_Access = ACCESS_READ;
                        fl->fl_Key = (IPTR) fh;
                        replypkt(dp, (SIPTR) fl);
                    }
                    break;
                case ACTION_FREE_LOCK:
                    fl = (struct FileLock *)dp->dp_Arg1;
                    fh = (struct filehandle *)fl->fl_Key;

                    FreeMem(fl, sizeof(*fl));
                    fh->usecount--;

                    replypkt(dp, DOSTRUE);
                    break;
                case ACTION_END:
                    fh->usecount--;
                    if (fh->usecount <= 0) {
                        if (fh->flags & FHFLG_WAIT) {
                            /*
                             * Bounce all pending read and waits (the same as we do when exiting).
                             * However the process is still around, waiting for EOF input.
                             * Our user has just closed his struct FileHandle and dropped us.
                             */
                            stopread(fh, waitingdp);
                            waitingdp = NULL;
                            fh->flags = (fh->flags & ~FHFLG_READPENDING) | FHFLG_WAITFORCLOSE;
                        }
                        else
                            goto end;
                    }
                    replypkt(dp, DOSTRUE);
                    break;
                case ACTION_READ:
                case ACTION_WRITE:
                    if (dp->dp_Arg3 < 0) {
                        replypkt2(dp, -1, ERROR_BAD_NUMBER);
                        break;
                    }
                    if (dp->dp_Arg3 == 0) {
                        replypkt2(dp, 0, 0);
                        break;
                    }
                    if (!MakeSureWinIsOpen(fh)) {
                        replypkt2(dp, -1, ERROR_NO_FREE_STORE);
                        break;
                    }
                    fh->breaktask = dp->dp_Port->mp_SigTask;
                    if (dp->dp_Type == ACTION_WRITE) {
                        startread(fh);
                        answer_write_request(fh, dp);
                    } else if (fh->flags & FHFLG_RAW) {
                        con_read(fh, dp);
                        pump_raw_input(fh);
                        if (fh->conbufferpos >= fh->conbuffersize)
                            startread(fh);
                    } else {
                        startread(fh);
                        con_read(fh, dp);
                    }
                    break;
                case ACTION_SCREEN_MODE:
                    {
                        if (dp->dp_Arg1 && !(fh->flags & FHFLG_RAW)) {
                            /* Switching from CON: mode to RAW: mode */
                            fh->flags |= FHFLG_RAW;
                            fh->inputstart = fh->inputsize;
                            fh->inputpos = fh->inputsize;
                            HandlePendingReads(fh);
                        } else {
                            /* otherwise just copy the flags */
                            if (dp->dp_Arg1)
                                fh->flags |= FHFLG_RAW;
                            else
                                fh->flags &= ~FHFLG_RAW;
                        }
                        replypkt(dp, DOSTRUE);
                    }
                    break;
                case ACTION_CHANGE_SIGNAL:
                    {
                        struct Task *old = fh->breaktask;
                        if (dp->dp_Arg2)
                            fh->breaktask = (struct Task*) dp->dp_Arg2;
                        replypkt2(dp, DOSTRUE, (SIPTR) old);
                    }
                    break;
                case ACTION_WAIT_CHAR:
                    {
                        if (!MakeSureWinIsOpen(fh)) {
                            replypkt2(dp, DOSFALSE, ERROR_NO_FREE_STORE);
                            break;
                        }
                        if (fh->inputsize > 0) {
                            replypkt(dp, DOSTRUE);
                        }
                        else if (dp->dp_Arg1 == 0) {
                            replypkt(dp, DOSFALSE);
                        } else if (waitingdp) {
                            replypkt2(dp, DOSFALSE, ERROR_OBJECT_IN_USE);
                        } else {
                            LONG timeout = dp->dp_Arg1;
                            LONG sec = timeout / 1000000;
                            LONG usec = timeout % 1000000;

                            fh->timerreq->tr_node.io_Command = TR_ADDREQUEST;
                            fh->timerreq->tr_time.tv_secs = sec;
                            fh->timerreq->tr_time.tv_micro = usec;
                            SendIO((struct IORequest *) fh->timerreq);
                            waitingdp = dp;
                        }
                        startread(fh);
                    }
                    break;
                case ACTION_IS_FILESYSTEM:
                    replypkt(dp, DOSFALSE);
                    break;
                case ACTION_DISK_INFO:
                    {
                        /* strange console handler features */
                        struct InfoData *id = (struct InfoData *)dp->dp_Arg1;
                        memset(id, 0, sizeof(struct InfoData));
                        id->id_DiskType =
                                (fh->flags & FHFLG_RAW) ? 0x52415700L : 0x434F4E00L;
                        /* The window we hang off, for whoever wants to put
                           something over it. Empty means we have none just
                           now: a console opened AUTO gets its window when
                           something first writes to it. */
                        id->id_VolumeNode = (BPTR)fh->window;
                        /* Classic console handlers return their read
                           IORequest here. Old programs such as the 1.3
                           SetMap command use it to find the console unit. */
                        id->id_InUse = (IPTR)fh->conreadio;
                        replypkt(dp, DOSTRUE);
                    }
                    break;
                case ACTION_SEEK:
                    /* Yes, DOSTRUE. Check Guru Book for details. */
                    replypkt2(dp, DOSTRUE, ERROR_ACTION_NOT_KNOWN);
                    break;
                default:
                    /* Saying no is a normal answer here: capability probes
                     * ask for actions a console has no use for. */
                    replypkt2(dp, DOSFALSE, ERROR_ACTION_NOT_KNOWN);
                    break;
                }
                dp = NULL;
            }
        }
    }
end:
    if (fh) {
        stopread(fh, waitingdp);

        if (fh->flags & FHFLG_ASYNCCONSOLEREAD) {
            AbortIO(ioReq(fh->conreadio));
            WaitIO(ioReq(fh->conreadio));
        }

        close_con(fh);
    }

    if (dp)
        replypkt2(dp, error ? DOSFALSE : DOSTRUE, error);

    if (mp)
        DeleteMsgPort(mp);

    return 0;
}

/* The device's process, started by DOS: it starts a window process for
   each open */
int main(void)
{
    struct Process *me = (struct Process *)FindTask(NULL);
    struct DosList *dl;
    struct DosPacket *dp;
    struct Process *p;
    struct TagItem t[5];

    dp = WaitPkt();
    dl = (struct DosList *)dp->dp_Arg3;
    dl->dol_Task = &me->pr_MsgPort;
    ReplyPkt(dp, DOSTRUE, 0);

    t[0].ti_Tag = NP_Entry;         t[0].ti_Data = (IPTR)CONMain;
    t[1].ti_Tag = NP_Name;          t[1].ti_Data = (IPTR)dl->dol_Name;
    t[2].ti_Tag = NP_StackSize;     t[2].ti_Data = CONTASK_STACKSIZE;
    t[3].ti_Tag = NP_Priority;      t[3].ti_Data = CONTASK_PRIORITY;
    t[4].ti_Tag = TAG_DONE;
    for (;;) {
        dp = WaitPkt();
        switch (dp->dp_Type) {
        case ACTION_FINDINPUT:
        case ACTION_FINDOUTPUT:
        case ACTION_FINDUPDATE:
            if ((p = CreateNewProc(t)) == NULL) {
                ReplyPkt(dp, DOSFALSE, ERROR_NO_FREE_STORE);
                break;
            }
            dp->dp_Arg4 = (LONG)dl;
            PutMsg(&p->pr_MsgPort, dp->dp_Link);
            break;
        case ACTION_IS_FILESYSTEM:
            ReplyPkt(dp, DOSFALSE, 0);
            break;
        default:
            ReplyPkt(dp, DOSFALSE, ERROR_ACTION_NOT_KNOWN);
            break;
        }
    }
}

static char conname[] = "con-handler";
static char conid[] = "con-handler 41.15 (15.11.2026)";

/* The romtag's init, as (0, seglist, SysBase): list CON and RAW */
static APTR coninit(APTR zero, APTR seg, APTR sysbase)
{
    static char *names[2] = { "CON", "RAW" };
    struct DosList *dl;
    int i;

    if ((DOSBase = (struct DosLibrary *)OpenLibrary(DOSNAME, 39L)) == NULL)
        return NULL;
    for (i = 0; i < 2; i++) {
        if ((dl = MakeDosEntry(names[i], (LONG)DLT_DEVICE)) == NULL)
            return NULL;
        dl->dol_misc.dol_handler.dol_Handler = conname;
        dl->dol_misc.dol_handler.dol_StackSize = 768;
        dl->dol_misc.dol_handler.dol_Priority = CONTASK_PRIORITY;
        dl->dol_misc.dol_handler.dol_Startup = i;
        dl->dol_misc.dol_handler.dol_SegList = (BPTR)seg;
        AddDosEntry(dl);
    }
    return (APTR)dl;
}

struct Resident contag = {
    RTC_MATCHWORD, &contag, (APTR)(&contag + 1), 0, 41,
    NT_UNKNOWN, 0, conname, conid, (APTR)coninit
};
