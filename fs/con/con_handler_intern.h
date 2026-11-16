#ifndef __CON_HANDLER_INTERN_H
#define __CON_HANDLER_INTERN_H
/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.
    $Id$

    Desc: Internal header-file for emulation-handler.
    Lang: english
*/

/* AROS includes */
#include <exec/libraries.h>
#include <exec/types.h>
#include <dos/dosextens.h>
#include <intuition/intuition.h>
#include <aros/types.h>
#include <devices/timer.h>

#define RMB_FREEZES_OUTPUT      1

/* A window's handler process: it also runs console.device's writes */
#define CONTASK_STACKSIZE       3840
#define CONTASK_PRIORITY        5

#define CONSOLEBUFFER_SIZE      256
#define INPUTBUFFER_SIZE        256
#define CMD_HISTORY_SIZE        32
#define PASTEBUFSIZE            16384

struct filehandle
{
    struct IOStdReq         *conreadio;
    struct IOStdReq         conwriteio;
    struct MsgPort          *conreadmp;
    struct MsgPort          *conwritemp;
    struct Window           *window;
    struct Menu             *winmenu;
    struct Window           *otherwindow; /* WINDOW0xXXXXXXXX parameter */
    struct Task             *contask;
    struct Task             *breaktask;
    struct MinList          pendingReads;
    struct NewWindow        nw;
    struct MsgPort          *timermp;
    struct timerequest      *timerreq;
    UBYTE                   *wintitle;
    UBYTE                   *screenname;
    WORD                    conbufferpos;
    WORD                    conbuffersize;
    WORD                    inputstart; /* usually 0, but needed for multi-lines (CONTROL RETURN) */
    WORD                    inputpos; /* cursor pos. inside line */
    WORD                    inputsize; /* length of input string */
    WORD                    killsize;
    WORD                    canreadsize;
    WORD                    historysize;
    WORD                    historypos;
    WORD                    historyviewpos;
    WORD                    usecount;
    UWORD                   flags;

    UBYTE                   consolebuffer[CONSOLEBUFFER_SIZE + 2];
    UBYTE                   inputbuffer[INPUTBUFFER_SIZE + 2];
    UBYTE                   killbuffer[INPUTBUFFER_SIZE + 1];
    UBYTE                   historybuffer[CMD_HISTORY_SIZE][INPUTBUFFER_SIZE + 1];

    /* If pastebuffer != 0, this contains data to paste from ConClip */
    WORD                    pastebufferpos;
    WORD                    pastebuffersize;
    STRPTR                  pastebuffer;
    struct DosList          *dn;

    struct Device           *inputbase;
    struct IntuitionBase    *intuibase;
    struct Library          *gtbase;
    struct Library          *utilbase;
};

extern struct DosLibrary *DOSBase;

/* The C library functions the handler uses */
ULONG strlen(CONST_STRPTR s);
int strcmp(CONST_STRPTR a, CONST_STRPTR b);
int strncmp(CONST_STRPTR a, CONST_STRPTR b, ULONG n);
char *strcpy(char *d, CONST_STRPTR s);
APTR memmove(APTR d, CONST_APTR s, ULONG n);
APTR memset(APTR d, int c, ULONG n);
#define ISSPACE(c) ((c) == ' ' || ((c) >= 9 && (c) <= 13))

#define ForeachNodeSafe(list, node, next) \
    for (node = (void *)(((struct List *)(list))->lh_Head); \
         (next = (void *)((struct Node *)(node))->ln_Succ) != NULL; \
         node = (void *)next)

#define NEWLIST(l) NewList((struct List *)(l))

/* filehandle flags */

#define FHFLG_READPENDING       1
#define FHFLG_CANREAD           4
#define FHFLG_WAIT              8   /* filename contained WAIT */
#define FHFLG_RAW               16  /* in RAW mode */
#define FHFLG_ASYNCCONSOLEREAD  32  /* There is a pending async console.device CMD_READ request */
#define FHFLG_AUTO              64  /* filename contained AUTO */
#define FHFLG_CONSOLEDEVICEOPEN 128
#define FHFLG_EOF               256
#define FHFLG_WAITFORCLOSE      512 /* Console with WAIT is waiting to be closed */

#undef InputBase
#undef IntuitionBase
#undef GadToolsBase
#undef UtilityBase

/*
 * FIXME: Remove these #define xxxBase hacks
 * Do not use this in new code !
 */
#define InputBase fh->inputbase
#define IntuitionBase fh->intuibase
#define GadToolsBase fh->gtbase
#define UtilityBase fh->utilbase

#endif /* __CON_HANDLER_INTERN_H */
