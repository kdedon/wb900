/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    ASL initialization code.
*/

#include <exec/libraries.h>
#include <exec/lists.h>
#include <exec/resident.h>
#include <exec/alerts.h>
#include <intuition/screens.h>
#include <graphics/modeid.h>
#include <graphics/hrmodeid.h>

#include "asl_intern.h"
#include "strings.h"
#include "filereqhooks.h"
#include "fontreqhooks.h"
#include "modereqhooks.h"

#define AN_AslLib   0x32000000L

static char aslname[] = AslName;
static char aslid[] = "asl 50.11 (15.11.2026)";

extern APTR AslFuncs[];

BOOL InitClasses(struct AslBase_intern *AslBase);
BOOL FreeClasses(struct AslBase_intern *AslBase);

/*****************************************************************************************/

#define DEF_WIDTH   400
#define DEF_HEIGHT  500

/* Requester type specific default data */
const struct IntFileReq def_filereq =
{
    {
        ASL_FileRequest,
        NULL,                   /* Window               */
        NULL,                   /* Screen               */
        NULL,                   /* PubScreenName        */
        NULL,                   /* IntuiMsgFunc         */
        NULL,                   /* TextAttr             */
        NULL,                   /* MemPool              */
        2048,                   /* MemPoolPuddle        */
        2048,                   /* MemPoolThresh        */
        MSG_FILEREQ_TITLE,      /* TitleID              */
        NULL,                   /* TitleText            */
        NULL,                   /* PositiveText         */
        NULL,                   /* NegativeText         */
        -1, -1,                 /* --> center on screen */
        DEF_WIDTH, DEF_HEIGHT,  /* Width/Height         */
        IF_POPTOFRONT
    },

    "",                         /* File                 */
    "",                         /* Drawer               */
    "#?",                       /* Pattern              */
    NULL,                       /* AcceptPattern        */ /* def. = "#?", but must be ParsePatternNoCase'ed */
    NULL,                       /* RejectPattern        */ /* def. = "~(#?)", but must be ParsePatternNoCase'ed */
    0,                          /* Flags1               */
    0,                          /* Flags2               */
    NULL,                       /* FilterFunc           */
    NULL,                       /* HookFunc             */
    NULL,                       /* GetSortBy            */
    NULL,                       /* GetSortOrder         */
    NULL,                       /* GetSortDrawers       */
    ASLFRSORTBY_Name,           /* SortBy               */
    ASLFRSORTORDER_Ascend,      /* SortOrder            */
    ASLFRSORTDRAWERS_First,     /* SortDrawers          */
    FALSE                       /* InitialShowVolumes   */
};

/*****************************************************************************************/

const struct IntSMReq def_smreq =
{
    {
        ASL_ScreenModeRequest,
        NULL,                           /* Window               */
        NULL,                           /* Screen               */
        NULL,                           /* PubScreenName        */
        NULL,                           /* IntuiMsgFunc         */
        NULL,                           /* TextAttr             */
        NULL,                           /* MemPool              */
        2048,                           /* MemPoolPuddle        */
        2048,                           /* MemPoolThresh        */
        MSG_MODEREQ_TITLE,              /* TitleID              */
        NULL,                           /* TitleText            */
        NULL,                           /* PositiveText         */
        NULL,                           /* NegativeText         */
        -1, -1,                         /* --> center on screen */
        DEF_WIDTH, DEF_HEIGHT,          /* Width/Height         */
        IF_POPTOFRONT
    },

    NULL,                               /* CustomSMList         */
    NULL,                               /* FilterFunc           */
    0,                                  /* Flags                */
    HRMONO_800_ID,                      /* DisplayID            */
    1024,                               /* DisplayWidth         */
    800,                                /* DisplayHeight        */
    1024,                               /* BitMapWidth          */
    800,                                /* BitMapHeight         */
    1,                                  /* DisplayDepth         */
    OSCAN_TEXT,                         /* OverscanType         */
    TRUE,                               /* AutoScroll           */
    DIPF_IS_WB,                         /* PropertyFlags        */
    DIPF_IS_WB,                         /* PropertyMask         */
    1,                                  /* MinDepth             */
    24,                                 /* MaxDepth             */
    16,                                 /* MinWidth             */
    16384,                              /* MaxWidth             */
    16,                                 /* MinHeight            */
    16384,                              /* MaxHeight            */
    20,                                 /* InfoLeftEdge         */
    20,                                 /* InfoTopEdge          */
    FALSE                               /* InfoOpened           */
};

/*****************************************************************************************/

const struct IntFontReq def_fontreq =
{
    {
        ASL_FontRequest,
        NULL,                           /* Window               */
        NULL,                           /* Screen               */
        NULL,                           /* PubScreenName        */
        NULL,                           /* IntuiMsgFunc         */
        NULL,                           /* TextAttr             */
        NULL,                           /* MemPool              */
        2048,                           /* MemPoolPuddle        */
        2048,                           /* MemPoolThresh        */
        MSG_FONTREQ_TITLE,              /* TitleID              */
        NULL,                           /* TitleText            */
        NULL,                           /* PositiveText         */
        NULL,                           /* NegativeText         */
        -1, -1,                         /* --> center on screen */
        DEF_WIDTH, DEF_HEIGHT,          /* Width/Height         */
        IF_POPTOFRONT
    },
    {"topaz", 8, FS_NORMAL,FPF_ROMFONT},/* Default textattr     */
    1,                                  /* FrontPen             */
    0,                                  /* BackPen              */
    JAM1,                               /* DrawMode             */
    0,                                  /* Flags                */

    5,                                  /* Minheight            */
    24,                                 /* MaxHeight            */
    NULL,                               /* FilterFunc           */
    NULL,                               /* HookFunc             */
    32,                                 /* MaxFrontPen          */
    32,                                 /* MaxBackPen           */

    NULL,                               /* ModeList             */
    NULL,                               /* FrontPens            */
    NULL,                               /* BackPens             */

};

static VOID InitReqInfo(struct AslBase_intern *AslBase)
{
    struct AslReqInfo *reqinfo;

    /* Set file requester info */

    reqinfo = &(ASLB(AslBase)->ReqInfo[ASL_FileRequest]);
    reqinfo->IntReqSize         = sizeof (struct IntFileReq);
    reqinfo->ReqSize            = sizeof (struct FileRequester);
    reqinfo->DefaultReq         = (struct IntFileReq *)&def_filereq;
    reqinfo->UserDataSize       = sizeof (struct FRUserData);

    bzero(&(reqinfo->ParseTagsHook), sizeof (struct Hook));
    bzero(&(reqinfo->GadgetryHook), sizeof (struct Hook));
    reqinfo->ParseTagsHook.h_Entry      = (void *)(FRTagHook);
    reqinfo->GadgetryHook.h_Entry       = (void *)(FRGadgetryHook);

    /* Set font requester info */

    reqinfo = &(ASLB(AslBase)->ReqInfo[ASL_FontRequest]);
    reqinfo->IntReqSize         = sizeof (struct IntFontReq);
    reqinfo->ReqSize            = sizeof (struct FontRequester);
    reqinfo->DefaultReq         = (struct IntFontReq *)&def_fontreq;
    reqinfo->UserDataSize       = sizeof (struct FOUserData);

    bzero(&(reqinfo->ParseTagsHook), sizeof (struct Hook));
    bzero(&(reqinfo->GadgetryHook), sizeof (struct Hook));
    reqinfo->ParseTagsHook.h_Entry      = (void *)(FOTagHook);
    reqinfo->GadgetryHook.h_Entry       = (void *)(FOGadgetryHook);

    /* Set screenmode requester info */

    reqinfo = &(ASLB(AslBase)->ReqInfo[ASL_ScreenModeRequest]);
    reqinfo->IntReqSize         = sizeof (struct IntSMReq);
    reqinfo->ReqSize            = sizeof (struct ScreenModeRequester);
    reqinfo->DefaultReq         = (struct IntSMReq *)&def_smreq;
    reqinfo->UserDataSize       = sizeof(struct SMUserData);

    bzero(&(reqinfo->ParseTagsHook), sizeof (struct Hook));
    bzero(&(reqinfo->GadgetryHook), sizeof (struct Hook));
    reqinfo->ParseTagsHook.h_Entry      = (void *)(SMTagHook);
    reqinfo->GadgetryHook.h_Entry       = (void *)(SMGadgetryHook);
}

/*****************************************************************************************/

/* Opened by the first OpenLibrary(), in the caller's task */
static BOOL openall(struct AslBase_intern *AslBase)
{
    static const char *const names[] =
    {
        "dos.library", "utility.library", "graphics.library", "layers.library",
        "intuition.library", "gadtools.library"
    };
    struct Library **bases[6];
    WORD i;

    bases[0] = &AslBase->asl_DOSBase;
    bases[1] = &AslBase->asl_UtilityBase;
    bases[2] = (struct Library **)&AslBase->asl_GfxBase;
    bases[3] = &AslBase->asl_LayersBase;
    bases[4] = (struct Library **)&AslBase->asl_IntuitionBase;
    bases[5] = &AslBase->asl_GadToolsBase;

    for (i = 0; i < 6; i++)
    {
        if ((*bases[i] = OpenLibrary(names[i], 39L)) == NULL)
        {
            while (--i >= 0)
            {
                CloseLibrary(*bases[i]);
                *bases[i] = NULL;
            }
            return FALSE;
        }
    }
    AslBase->asl_WorkbenchBase = OpenLibrary("workbench.library", 0L);
    return TRUE;
}

static void closeall(struct AslBase_intern *AslBase)
{
    if (AslBase->asl_DiskfontBase) CloseLibrary(AslBase->asl_DiskfontBase);
    if (AslBase->asl_WorkbenchBase) CloseLibrary(AslBase->asl_WorkbenchBase);
    CloseLibrary(AslBase->asl_GadToolsBase);
    CloseLibrary((struct Library *)AslBase->asl_IntuitionBase);
    CloseLibrary(AslBase->asl_LayersBase);
    CloseLibrary((struct Library *)AslBase->asl_GfxBase);
    CloseLibrary(AslBase->asl_UtilityBase);
    CloseLibrary(AslBase->asl_DOSBase);
    AslBase->asl_DOSBase = NULL;
}

/*****************************************************************************************/

static struct Library *aslinit(struct AslBase_intern *AslBase, APTR seg, APTR sysbase)
{
    AslBase->Lib.lib_Node.ln_Name = aslname;
    AslBase->Lib.lib_IdString = (APTR)aslid;
    AslBase->Lib.lib_Version = 50;
    AslBase->Lib.lib_Revision = 11;
    AslBase->SegList = (BPTR)seg;

    NewList((struct List *)&AslBase->ReqList);
    InitSemaphore(&AslBase->ReqListSem);
    InitReqInfo(AslBase);

    return &AslBase->Lib;
}

struct Library *aslopen(struct AslBase_intern *AslBase, ULONG ver)
{
    if (!AslBase->asl_DOSBase && !openall(AslBase))
        return NULL;
    if (!AslBase->aslpropclass && !InitClasses(AslBase))
    {
        FreeClasses(AslBase);
        return NULL;
    }

    AslBase->Lib.lib_OpenCnt++;
    AslBase->Lib.lib_Flags &= ~LIBF_DELEXP;
    return &AslBase->Lib;
}

APTR aslexpunge(struct AslBase_intern *AslBase)
{
    APTR seg;

    if (AslBase->Lib.lib_OpenCnt != 0 || !FreeClasses(AslBase))
    {
        AslBase->Lib.lib_Flags |= LIBF_DELEXP;
        return NULL;
    }
    if (AslBase->asl_DOSBase)
        closeall(AslBase);

    seg = (APTR)AslBase->SegList;
    Remove(&AslBase->Lib.lib_Node);
    FreeMem((char *)AslBase - AslBase->Lib.lib_NegSize,
        AslBase->Lib.lib_NegSize + AslBase->Lib.lib_PosSize);
    return seg;
}

APTR aslclose(struct AslBase_intern *AslBase)
{
    if (--AslBase->Lib.lib_OpenCnt == 0 && (AslBase->Lib.lib_Flags & LIBF_DELEXP))
        return aslexpunge(AslBase);
    return NULL;
}

APTR aslnull(void)
{
    return NULL;
}

/* The private vectors */
ULONG aslunimpl(void)
{
    Alert(AN_AslLib);
    return 0;
}

static struct {
    ULONG it_Size;
    APTR it_Funcs, it_Struct, it_Init;
} aslinittab = {
    sizeof(struct AslBase_intern), (APTR)AslFuncs, NULL, (APTR)aslinit
};

struct Resident asltag = {
    RTC_MATCHWORD, &asltag, (APTR)(&asltag + 1), RTF_AUTOINIT, 50,
    NT_LIBRARY, 0, aslname, aslid, (APTR)&aslinittab
};
