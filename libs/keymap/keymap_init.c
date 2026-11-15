/*
    Copyright (C) 1995-2011, The AROS Development Team. All rights reserved.

    Desc: Library header for keymap
*/

/****************************************************************************************/

#include <proto/exec.h>
#include <exec/resident.h>
#include <exec/execbase.h>
#include <exec/memory.h>
#include "keymap_intern.h"

/****************************************************************************************/

static char kmname[] = KEYMAPNAME;
static char kmid[] = "keymap 41.2 (14.11.2026)";
static char kmresname[] = "keymap.resource";
static char kmusa[] = "usa";

extern CONST struct KeyMap def_km;
extern APTR KeymapFuncs[];

/****************************************************************************************/

static struct KeyMapNode *AddKeymap(char *name, CONST struct KeyMap *data, struct KeymapBase *LIBBASE)
{
    struct KeyMapNode *kmn = AllocMem(sizeof(struct KeyMapNode), MEMF_CLEAR | MEMF_PUBLIC);

    if (kmn)
    {
        kmn->kn_Node.ln_Name = name;
        CopyMem((APTR)data, &kmn->kn_KeyMap, sizeof(struct KeyMap));

        /*
         * We are being called under Forbid(), so I don't have to arbitrate
         * That notwithstanding, if keymap resource or exec library loading
         * ever become semaphore based, there may be some problems.
         */
        AddTail(&(LIBBASE->KeymapResource.kr_List), &kmn->kn_Node);
    }
    return kmn;
}

static struct Library *kminit(struct Library *lib, APTR seg, APTR sysbase)
{
    struct KeymapBase *LIBBASE = KMBase(lib);

    lib->lib_Node.ln_Name = kmname;
    lib->lib_IdString = (APTR)kmid;
    lib->lib_Version = 41;
    lib->lib_Revision = 2;
    LIBBASE->SegList = seg;

    LIBBASE->DefaultKeymap = (struct KeyMap *)&def_km;

    /* Initialize and add the keymap.resource */
    LIBBASE->KeymapResource.kr_Node.ln_Type = NT_RESOURCE;
    LIBBASE->KeymapResource.kr_Node.ln_Name = kmresname;
    NewList(&(LIBBASE->KeymapResource.kr_List));

    /* AmigaOS default built-in keymap has "usa" name */
    if (!AddKeymap(kmusa, &def_km, LIBBASE))
    {
        FreeMem((char *)lib - lib->lib_NegSize,
            lib->lib_NegSize + lib->lib_PosSize);
        return NULL;
    }
    AddResource(&LIBBASE->KeymapResource);

    return lib;
}

/****************************************************************************************/

struct Library *kmopen(struct Library *lib, ULONG ver)
{
    lib->lib_OpenCnt++;
    return lib;
}

APTR kmclose(struct Library *lib)
{
    lib->lib_OpenCnt--;
    return NULL;
}

/* keymap.resource and the default keymap's users outlive the opens. */
APTR kmexpunge(struct Library *lib)
{
    return NULL;
}

APTR kmnull(void)
{
    return NULL;
}

static struct {
    ULONG it_Size;
    APTR it_Funcs, it_Struct, it_Init;
} kminittab = {
    sizeof(struct KeymapBase), (APTR)KeymapFuncs, NULL, (APTR)kminit
};

struct Resident kmtag = {
    RTC_MATCHWORD, &kmtag, (APTR)(&kmtag + 1), RTF_AUTOINIT, 41,
    NT_LIBRARY, 40, kmname, kmid, (APTR)&kminittab
};
