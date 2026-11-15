/*
    Copyright (C) 1995-2020, The AROS Development Team. All rights reserved.

    Desc: Utility Resident and initialization.
*/

#include "intern.h"

static char utname[] = UTILITYNAME;
static char utid[] = "utility 50.4 (14.11.2026)";

extern APTR UtilityFuncs[];

static struct Library *utinit(struct Library *lib, APTR seg, APTR sysbase)
{
    lib->lib_Node.ln_Name = utname;
    lib->lib_IdString = (APTR)utid;
    lib->lib_Version = 50;
    lib->lib_Revision = 4;
    GetIntUtilityBase(lib)->ub_SegList = seg;
    GetIntUtilityBase(lib)->ub_LastID = 0;

    /*
        I no longer allocate memory here for the global namespace, since
        that is not quite legal. (AllocMem is not Forbid() protected).

        Also makes this a little bit shorter. (In time and length).
    */
    InitSemaphore(&GetIntUtilityBase(lib)->ub_NameSpace.ns_Lock);
    NewMinList(&GetIntUtilityBase(lib)->ub_NameSpace.ns_List);
    GetIntUtilityBase(lib)->ub_NameSpace.ns_Flags = NSF_NODUPS;

    return lib;
}

struct Library *utopen(struct Library *lib, ULONG ver)
{
    lib->lib_OpenCnt++;
    lib->lib_Flags &= ~LIBF_DELEXP;
    return lib;
}

/* The global NameSpace lives in the base: keep it while it has objects. */
APTR utexpunge(struct Library *lib)
{
    APTR seg;

    if (lib->lib_OpenCnt != 0 ||
        !IsMinListEmpty(&GetIntUtilityBase(lib)->ub_NameSpace.ns_List))
    {
        lib->lib_Flags |= LIBF_DELEXP;
        return NULL;
    }
    seg = GetIntUtilityBase(lib)->ub_SegList;
    Remove(&lib->lib_Node);
    FreeMem((char *)lib - lib->lib_NegSize,
        lib->lib_NegSize + lib->lib_PosSize);
    return seg;
}

APTR utclose(struct Library *lib)
{
    if (--lib->lib_OpenCnt == 0 && (lib->lib_Flags & LIBF_DELEXP))
        return utexpunge(lib);
    return NULL;
}

APTR utnull(void)
{
    return NULL;
}

/* The vectors of SDivMod32, SMult64, UMult64 and the private ones. */
ULONG utunimpl(void)
{
    Alert(AN_UtilityLib);
    return 0;
}

static struct {
    ULONG it_Size;
    APTR it_Funcs, it_Struct, it_Init;
} utinittab = {
    sizeof(struct IntUtilityBase), (APTR)UtilityFuncs, NULL, (APTR)utinit
};

struct Resident uttag = {
    RTC_MATCHWORD, &uttag, (APTR)(&uttag + 1), RTF_AUTOINIT, 50,
    NT_LIBRARY, 0, utname, utid, (APTR)&utinittab
};
