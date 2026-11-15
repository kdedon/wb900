/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Initialization of workbench.library.
*/

#include <exec/resident.h>
#include <exec/alerts.h>
#include <dos/dostags.h>
#include "workbench_intern.h"
#include "handler.h"
#include "support.h"

#define AN_WorkbenchLib  0x39000000L

static char wbname[] = WORKBENCH_NAME;
static char wbid[] = "workbench 45.6 (15.11.2026)";
static char wbhandler[] = "Workbench Handler";

extern APTR WorkbenchFuncs[];

/* The handler's stack */
#define HANDLER_STACK   1280

static BOOL openall(struct WorkbenchBase *WorkbenchBase)
{
    if ((UtilityBase = OpenLibrary(UTILITYNAME, 39L))) {
        if ((WorkbenchBase->wb_DOSBase = OpenLibrary(DOSNAME, 36L))) {
            if ((WorkbenchBase->wb_IntuitionBase = OpenLibrary(INTUITIONNAME, 39L))) {
                if ((IconBase = OpenLibrary(ICONNAME, 44L))) {
                    return TRUE;
                }
                CloseLibrary(WorkbenchBase->wb_IntuitionBase);
            }
            CloseLibrary(WorkbenchBase->wb_DOSBase);
        }
        CloseLibrary(UtilityBase);
    }
    return FALSE;
}

static struct Library *wbinit(struct Library *lib, APTR seg, APTR sysbase)
{
    struct WorkbenchBase *WorkbenchBase = LB(lib);
    struct CommandLineInterface *cli;
    struct TagItem tags[4];

    lib->lib_Node.ln_Name = wbname;
    lib->lib_IdString = (APTR)wbid;
    lib->lib_Version = 45;
    lib->lib_Revision = 6;
    WorkbenchBase->wb_SegList = seg;

    /* Initialize our private lists ----------------------------------------*/
    NewList(&(WorkbenchBase->wb_AppWindows));
    NewList(&(WorkbenchBase->wb_AppIcons));
    NewList(&(WorkbenchBase->wb_AppMenuItems));

    /* Initialize our semaphores -------------------------------------------*/
    InitSemaphore(&(WorkbenchBase->wb_WorkbenchPortSemaphore));
    InitSemaphore(&(WorkbenchBase->wb_BaseSemaphore));

    /* Initialize handler message port -------------------------------------*/
    WorkbenchBase->wb_HandlerPort.mp_Node.ln_Type = NT_MSGPORT;
    WorkbenchBase->wb_HandlerPort.mp_SigBit  = SIGBREAKB_CTRL_F;
    WorkbenchBase->wb_HandlerPort.mp_Flags   = PA_IGNORE;
    NewList(&(WorkbenchBase->wb_HandlerPort.mp_MsgList));
    WorkbenchBase->wb_AppReplyPort.mp_Node.ln_Type = NT_MSGPORT;
    WorkbenchBase->wb_AppReplyPort.mp_Flags = PA_IGNORE;
    NewList(&(WorkbenchBase->wb_AppReplyPort.mp_MsgList));

    /* Initialize miscellanous variables -----------------------------------*/
    WorkbenchBase->wb_DefaultStackSize = 2048;

    if (!openall(WorkbenchBase))
        goto fail;

    /* Duplicate the search path ---------------------------------------*/
    if ((cli = Cli()) != NULL)
    {
        WorkbenchBase->wb_SearchPath = DuplicateSearchPath
        (
            cli->cli_CommandDir
        );
    }

    /* Start workbench handler -----------------------------------------*/
    tags[0].ti_Tag = NP_Entry;      tags[0].ti_Data = (IPTR)WorkbenchHandler;
    tags[1].ti_Tag = NP_Name;       tags[1].ti_Data = (IPTR)wbhandler;
    tags[2].ti_Tag = NP_StackSize;  tags[2].ti_Data = HANDLER_STACK;
    tags[3].ti_Tag = TAG_DONE;
    HandlerBase = WorkbenchBase;
    if (CreateNewProc(tags) != NULL)
        return lib;

    FreeSearchPath(WorkbenchBase->wb_SearchPath);
    CloseLibrary(IconBase);
    CloseLibrary(WorkbenchBase->wb_IntuitionBase);
    CloseLibrary(WorkbenchBase->wb_DOSBase);
    CloseLibrary(UtilityBase);
fail:
    FreeMem((char *)lib - lib->lib_NegSize,
        lib->lib_NegSize + lib->lib_PosSize);
    return NULL;
}

/****************************************************************************************/

struct Library *wbopen(struct Library *lib, ULONG ver)
{
    lib->lib_OpenCnt++;
    return lib;
}

APTR wbclose(struct Library *lib)
{
    lib->lib_OpenCnt--;
    return NULL;
}

/* The handler keeps running, so the library stays. */
APTR wbexpunge(struct Library *lib)
{
    return NULL;
}

APTR wbnull(void)
{
    return NULL;
}

/* The vectors of the functions not ported */
ULONG wbunimpl(void)
{
    Alert(AN_WorkbenchLib);
    return 0;
}

static struct {
    ULONG it_Size;
    APTR it_Funcs, it_Struct, it_Init;
} wbinittab = {
    sizeof(struct WorkbenchBase), (APTR)WorkbenchFuncs, NULL, (APTR)wbinit
};

struct Resident wbtag = {
    RTC_MATCHWORD, &wbtag, (APTR)(&wbtag + 1), RTF_AUTOINIT, 45,
    NT_LIBRARY, -120, wbname, wbid, (APTR)&wbinittab
};
