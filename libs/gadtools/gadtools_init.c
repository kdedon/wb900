/*
    Copyright (C) 1995-2006, The AROS Development Team. All rights reserved.

    Desc: GadTools initialization code.
*/
#include <exec/libraries.h>
#include <exec/types.h>
#include <exec/resident.h>
#include <exec/alerts.h>

#include <utility/tagitem.h>
#include <utility/utility.h>
#include <intuition/classes.h>
#include <intuition/imageclass.h>
#include <proto/exec.h>

#include "gadtools_intern.h"

#define AN_GadToolsLib  0x31000000L

static char gtname[] = GADTOOLSNAME;
static char gtid[] = "gadtools 41.2 (15.11.2026)";

extern APTR GadToolsFuncs[];

BOOL InitClasses(struct GadToolsBase_intern *GadToolsBase);
BOOL FreeClasses(struct GadToolsBase_intern *GadToolsBase);
void LV_RenderHook_Init(struct GadToolsBase_intern *LIBBASE);

/****************************************************************************************/

static int openall(struct GadToolsBase_intern *GadToolsBase)
{
    if ((UtilityBase = OpenLibrary("utility.library", 0)) != NULL) {
        if ((GfxBase = OpenLibrary("graphics.library", 0)) != NULL) {
            if ((LayersBase = OpenLibrary("layers.library", 0)) != NULL) {
                if ((IntuitionBase = OpenLibrary("intuition.library", 0)) != NULL) {
                    return TRUE;
                }
                CloseLibrary(LayersBase);
            }
            CloseLibrary(GfxBase);
        }
        CloseLibrary(UtilityBase);
    }
    UtilityBase = NULL;
    return FALSE;
}

static void closeall(struct GadToolsBase_intern *GadToolsBase)
{
    CloseLibrary(IntuitionBase);
    CloseLibrary(LayersBase);
    CloseLibrary(GfxBase);
    CloseLibrary(UtilityBase);
    UtilityBase = NULL;
}

/****************************************************************************************/

Object *makebevelobj(struct GadToolsBase_intern *GadToolsBase)
{
    Object * obj;
    struct TagItem tags[4];

    tags[0].ti_Tag  = IA_EdgesOnly;
    tags[0].ti_Data = TRUE;
    tags[1].ti_Tag  = IA_Left;
    tags[1].ti_Data = 0UL;
    tags[2].ti_Tag  = IA_Top;
    tags[2].ti_Data = 0UL;
    tags[3].ti_Tag  = TAG_DONE;
    obj = NewObjectA(NULL, FRAMEICLASS, tags);

    return obj;
}

/****************************************************************************************/

static struct Library *gtinit(struct GadToolsBase_intern *lib, APTR seg, APTR sysbase)
{
    lib->lib.lib_Node.ln_Name = gtname;
    lib->lib.lib_IdString = (APTR)gtid;
    lib->lib.lib_Version = 41;
    lib->lib.lib_Revision = 2;
    lib->gt_SegList = (BPTR)seg;

    InitSemaphore(&lib->bevelsema);
    lib->bevel = NULL;
    LV_RenderHook_Init(lib);

    return &lib->lib;
}

/* The first open opens the libraries and makes the classes, in the caller's task */
struct Library *gtopen(struct GadToolsBase_intern *GadToolsBase, ULONG ver)
{
    if (!UtilityBase && !openall(GadToolsBase))
        return NULL;
    if (!GadToolsBase->buttonclass && !InitClasses(GadToolsBase))
    {
        FreeClasses(GadToolsBase);
        return NULL;
    }
    if (!GadToolsBase->bevel)
        GadToolsBase->bevel = (struct Image *)makebevelobj(GadToolsBase);
    if (!GadToolsBase->bevel)
        return NULL;

    GadToolsBase->lib.lib_OpenCnt++;
    GadToolsBase->lib.lib_Flags &= ~LIBF_DELEXP;
    return &GadToolsBase->lib;
}

/* Kept while gadgets of its classes exist */
APTR gtexpunge(struct GadToolsBase_intern *GadToolsBase)
{
    APTR seg;

    if (GadToolsBase->lib.lib_OpenCnt != 0)
    {
        GadToolsBase->lib.lib_Flags |= LIBF_DELEXP;
        return NULL;
    }
    if (GadToolsBase->bevel)
        DisposeObject(GadToolsBase->bevel);
    GadToolsBase->bevel = NULL;
    if (!FreeClasses(GadToolsBase))
    {
        GadToolsBase->lib.lib_Flags |= LIBF_DELEXP;
        return NULL;
    }
    if (UtilityBase)
        closeall(GadToolsBase);

    seg = (APTR)GadToolsBase->gt_SegList;
    Remove(&GadToolsBase->lib.lib_Node);
    FreeMem((char *)GadToolsBase - GadToolsBase->lib.lib_NegSize,
        GadToolsBase->lib.lib_NegSize + GadToolsBase->lib.lib_PosSize);
    return seg;
}

APTR gtclose(struct GadToolsBase_intern *GadToolsBase)
{
    if (--GadToolsBase->lib.lib_OpenCnt == 0 && (GadToolsBase->lib.lib_Flags & LIBF_DELEXP))
        return gtexpunge(GadToolsBase);
    return NULL;
}

APTR gtnull(void)
{
    return NULL;
}

/* The private vectors */
ULONG gtunimpl(void)
{
    Alert(AN_GadToolsLib);
    return 0;
}

static struct {
    ULONG it_Size;
    APTR it_Funcs, it_Struct, it_Init;
} gtinittab = {
    sizeof(struct GadToolsBase_intern), (APTR)GadToolsFuncs, NULL, (APTR)gtinit
};

struct Resident gttag = {
    RTC_MATCHWORD, &gttag, (APTR)(&gttag + 1), RTF_AUTOINIT, 41,
    NT_LIBRARY, 0, gtname, gtid, (APTR)&gtinittab
};
