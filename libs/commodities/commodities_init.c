/*
    Copyright (C) 1995-2006, The AROS Development Team. All rights reserved.

    Desc: Commodities initialization code.
*/

#include <utility/utility.h>
#include "cxintern.h"   /* Must be included after utility.h */

#include <exec/types.h>
#include <exec/resident.h>
#include <exec/alerts.h>
#include <proto/exec.h>
#include <devices/timer.h>

#define AN_CommoditiesLib   0x35000000L

static char cxname[] = "commodities.library";
static char cxid[] = "commodities 50.3 (15.11.2026)";

extern APTR CommoditiesFuncs[];

static struct Library *cxinit(struct CommoditiesBase *CxBase, APTR seg, APTR sysbase)
{
    struct Library *lib = &CxBase->cx_Lib;

    lib->lib_Node.ln_Name = cxname;
    lib->lib_IdString = (APTR)cxid;
    lib->lib_Version = 50;
    lib->lib_Revision = 3;
    CxBase->cx_SegList = seg;

    CxBase->cx_TimerMP.mp_Node.ln_Type = NT_MSGPORT;
    CxBase->cx_TimerMP.mp_Flags = PA_IGNORE;
    NEWLIST(&CxBase->cx_TimerMP.mp_MsgList);

    CxBase->cx_TimerIO.tr_node.io_Message.mn_ReplyPort = &CxBase->cx_TimerMP;
    CxBase->cx_TimerIO.tr_node.io_Message.mn_Length = sizeof(struct timerequest);

    if (OpenDevice(TIMERNAME, UNIT_VBLANK,
                   (struct IORequest *)&CxBase->cx_TimerIO, 0) != 0)
        goto fail;
    CxBase->cx_TimerBase =
        (struct Library *)(CxBase->cx_TimerIO.tr_node.io_Device);

    if ((CxBase->cx_UtilityBase = OpenLibrary("utility.library", 0)) == NULL)
        goto notimer;
    if ((CxBase->cx_KeymapBase = OpenLibrary("keymap.library", 0)) == NULL)
    {
        CloseLibrary(CxBase->cx_UtilityBase);
        goto notimer;
    }

    InitSemaphore(&CxBase->cx_SignalSemaphore);
    NEWLIST(&CxBase->cx_BrokerList);
    NEWLIST(&CxBase->cx_MessageList);
    NEWLIST(&CxBase->cx_GeneratedInputEvents);

    CxBase->cx_MsgPort.mp_Node.ln_Type = NT_MSGPORT;
    CxBase->cx_MsgPort.mp_Flags = PA_IGNORE;
    NEWLIST(&CxBase->cx_MsgPort.mp_MsgList);

    return lib;

notimer:
    CloseDevice((struct IORequest *)&CxBase->cx_TimerIO);
fail:
    FreeMem((char *)lib - lib->lib_NegSize,
        lib->lib_NegSize + lib->lib_PosSize);
    return NULL;
}

/****************************************************************************************/

struct Library *cxopen(struct Library *lib, ULONG ver)
{
    lib->lib_OpenCnt++;
    return lib;
}

APTR cxclose(struct Library *lib)
{
    lib->lib_OpenCnt--;
    return NULL;
}

/* The input handler, once installed, outlives the opens. */
APTR cxexpunge(struct Library *lib)
{
    return NULL;
}

APTR cxnull(void)
{
    return NULL;
}

/* The private vector */
ULONG cxunimpl(void)
{
    Alert(AN_CommoditiesLib);
    return 0;
}

static struct {
    ULONG it_Size;
    APTR it_Funcs, it_Struct, it_Init;
} cxinittab = {
    sizeof(struct CommoditiesBase), (APTR)CommoditiesFuncs, NULL, (APTR)cxinit
};

struct Resident cxtag = {
    RTC_MATCHWORD, &cxtag, (APTR)(&cxtag + 1), RTF_AUTOINIT, 50,
    NT_LIBRARY, 0, cxname, cxid, (APTR)&cxinittab
};
