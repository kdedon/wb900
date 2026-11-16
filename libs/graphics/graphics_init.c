/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: Graphics library
*/

#include <exec/resident.h>
#include <proto/exec.h>
#include <aros/libcall.h>
#include <exec/execbase.h>
#include <exec/memory.h>
#include <exec/lists.h>
#include <graphics/gfxbase.h>
#include <graphics/text.h>
#include <graphics/regions.h>
#include <proto/graphics.h>
#include <utility/utility.h>

#include "graphics_intern.h"
#include "default_font.h"
#include "gfxformat.h"

#define AN_GraphicsLib  0x02000000L

static char gfxname[] = GRAPHICSNAME;
static char gfxid[] = "graphics 45.29 (14.11.2026)";

extern APTR GraphicsFuncs[];

static struct Library *gfxinit(struct Library *lib, APTR seg, APTR sysbase)
{
    struct GfxBase *GfxBase = (struct GfxBase *)lib;

    lib->lib_Node.ln_Name = gfxname;
    lib->lib_IdString = (APTR)gfxid;
    lib->lib_Version = 45;
    lib->lib_Revision = 29;
    PrivGBase(GfxBase)->seglist = seg;
    GfxBase->ExecBase = SysBase;

    NewList(&GfxBase->BlitWaitQ);
    NewList(&GfxBase->TOF_WaitQ);
    NewList(&GfxBase->TextFonts);
    InitSemaphore(&PrivGBase(GfxBase)->hashtab_sema);
    InitSemaphore(&PrivGBase(GfxBase)->view_sema);
    /* Public, for diskfont.library and intuition to hold the font list */
    InitSemaphore(&PrivGBase(GfxBase)->fontsem);
    PrivGBase(GfxBase)->fontsem.ss_Link.ln_Name = "graphics.library/TextFonts";
    AddSemaphore(&PrivGBase(GfxBase)->fontsem);
    GfxBase->HashTableSemaphore = &PrivGBase(GfxBase)->hashtab_sema;
    GfxBase->ActiViewCprSemaphore = &PrivGBase(GfxBase)->view_sema;

    GfxBase->hash_table = AllocMem(GFXASSOCIATE_HASHSIZE * sizeof(APTR), MEMF_CLEAR | MEMF_PUBLIC);

    InitSemaphore(&PrivGBase(GfxBase)->sprite.ss_Lock);
    gfx_InitFormats();
    InitSemaphore(&PrivGBase(GfxBase)->regionsem);

    GfxBase->UtilBase = OpenLibrary(UTILITYNAME, 39L);
    if (!GfxBase->hash_table || !GfxBase->UtilBase || !InitROMFont(GfxBase))
    {
        if (GfxBase->hash_table)
            FreeMem(GfxBase->hash_table, GFXASSOCIATE_HASHSIZE * sizeof(APTR));
        if (GfxBase->UtilBase)
            CloseLibrary(GfxBase->UtilBase);
        FreeMem((char *)lib - lib->lib_NegSize,
            lib->lib_NegSize + lib->lib_PosSize);
        return NULL;
    }

    PrivGBase(GfxBase)->tofint.is_Node.ln_Name = gfxname;
    PrivGBase(GfxBase)->tofint.is_Data = GfxBase;
    PrivGBase(GfxBase)->tofint.is_Code = gfx_TOFServer;
    AddIntServer(INTB_VERTB, &PrivGBase(GfxBase)->tofint);

    return lib;
}

struct Library *gfxopen(struct Library *lib, ULONG ver)
{
    lib->lib_OpenCnt++;
    lib->lib_Flags &= ~LIBF_DELEXP;
    return lib;
}

/*
 * The ROM font and the region chunk pools stay while the library is
 * open: other tasks may still hold regions and the font.
 */
APTR gfxexpunge(struct Library *lib)
{
    struct GfxBase *GfxBase = (struct GfxBase *)lib;
    APTR seg;

    if (lib->lib_OpenCnt != 0)
    {
        lib->lib_Flags |= LIBF_DELEXP;
        return NULL;
    }
    seg = PrivGBase(GfxBase)->seglist;
    Remove(&lib->lib_Node);
    RemIntServer(INTB_VERTB, &PrivGBase(GfxBase)->tofint);
    RemSemaphore(&PrivGBase(GfxBase)->fontsem);
    if (PrivGBase(GfxBase)->drvlib)
        CloseLibrary(PrivGBase(GfxBase)->drvlib);
    FreeMem(GfxBase->DefaultFont, sizeof(struct TextFont));
    CloseLibrary(GfxBase->UtilBase);
    FreeMem(GfxBase->hash_table, GFXASSOCIATE_HASHSIZE * sizeof(APTR));
    FreeMem((char *)lib - lib->lib_NegSize,
        lib->lib_NegSize + lib->lib_PosSize);
    return seg;
}

APTR gfxclose(struct Library *lib)
{
    if (--lib->lib_OpenCnt == 0 && (lib->lib_Flags & LIBF_DELEXP))
        return gfxexpunge(lib);
    return NULL;
}

APTR gfxnull(void)
{
    return NULL;
}

/* The vectors of the functions not ported */
ULONG gfxunimpl(void)
{
    Alert(AN_GraphicsLib);
    return 0;
}

static struct {
    ULONG it_Size;
    APTR it_Funcs, it_Struct, it_Init;
} gfxinittab = {
    sizeof(struct GfxBase_intern), (APTR)GraphicsFuncs, NULL, (APTR)gfxinit
};

struct Resident gfxtag = {
    RTC_MATCHWORD, &gfxtag, (APTR)(&gfxtag + 1), RTF_AUTOINIT, 45,
    NT_LIBRARY, 65, gfxname, gfxid, (APTR)&gfxinittab
};
