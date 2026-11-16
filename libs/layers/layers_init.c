/*
    Copyright (C) 1995-2020, The AROS Development Team. All rights reserved.

    Desc: Layers Resident and initialization.
*/

#include <exec/resident.h>
#include <exec/alerts.h>
#include <proto/exec.h>

#include <graphics/gfxbase.h>

#include "layers_intern.h"

static char laname[] = "layers.library";
static char laid[] = "layers 50.13 (14.11.2026)";

extern APTR LayersFuncs[];

static struct Library *lainit(struct Library *lib, APTR seg, APTR sysbase)
{
    struct LayersBase *LayersBase = (struct LayersBase *)lib;

    lib->lib_Node.ln_Name = laname;
    lib->lib_IdString = (APTR)laid;
    lib->lib_Version = 50;
    lib->lib_Revision = 13;
    LayersBase->lb_SegList = seg;

    GfxBase = (APTR)OpenLibrary(GRAPHICSNAME, 45L);
    UtilityBase = (APTR)OpenLibrary(UTILITYNAME, 39L);
    LayersBase->lb_ClipRectPool = CreatePool(MEMF_CLEAR|MEMF_PUBLIC|MEMF_SEM_PROTECTED, sizeof(struct ClipRect) * 50, sizeof(struct ClipRect) * 50);
    if (!GfxBase || !UtilityBase || !LayersBase->lb_ClipRectPool) {
        if (LayersBase->lb_ClipRectPool)
            DeletePool(LayersBase->lb_ClipRectPool);
        if (UtilityBase)
            CloseLibrary((struct Library *)UtilityBase);
        if (GfxBase)
            CloseLibrary((struct Library *)GfxBase);
        FreeMem((char *)lib - lib->lib_NegSize,
            lib->lib_NegSize + lib->lib_PosSize);
        return NULL;
    }

    /* Install ourself as graphics.library's layer manager */
    GfxBase->gb_LayersBase = (APTR)LayersBase;

    return lib;
}

struct Library *laopen(struct Library *lib, ULONG ver)
{
    lib->lib_OpenCnt++;
    return lib;
}

/* graphics.library calls the library through gb_LayersBase: it stays */
APTR laexpunge(struct Library *lib)
{
    return NULL;
}

APTR laclose(struct Library *lib)
{
    lib->lib_OpenCnt--;
    return NULL;
}

APTR lanull(void)
{
    return NULL;
}

/* Vector of every unimplemented function */
ULONG launimpl(void)
{
    Alert(AN_LayersLib);
    return 0;
}

static struct {
    ULONG it_Size;
    APTR it_Funcs, it_Struct, it_Init;
} lainittab = {
    sizeof(struct LayersBase), (APTR)LayersFuncs, NULL, (APTR)lainit
};

struct Resident latag = {
    RTC_MATCHWORD, &latag, (APTR)(&latag + 1), RTF_AUTOINIT, 50,
    NT_LIBRARY, 64, laname, laid, (APTR)&lainittab
};
