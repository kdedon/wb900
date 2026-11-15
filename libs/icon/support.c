/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Miscellaneous support functions.
*/

#include "icon_intern.h"
#include "support.h"

BPTR __OpenIcon_WB(CONST_STRPTR name, LONG mode, struct IconBase *IconBase)
{
    BPTR  file       = NULL;
    ULONG nameLength = strlen(name);
       
    if (name[nameLength - 1] == ':')
    {
        BPTR lock = Lock(name, ACCESS_READ);
        
        if (lock != NULL)
        {
            BPTR cd = CurrentDir(lock);
            
            file = Open("Disk.info", mode);
            
            CurrentDir(cd);
            UnLock(lock);
        }
    }
    else
    {
        ULONG  length = nameLength + 5 /* strlen(".info") */ + 1 /* '\0' */;
        STRPTR path   = AllocVec(length, MEMF_PUBLIC);
        
        if(path != NULL)
        {
            strcpy(path, name);
            strcat(path, ".info");
            
            file = Open(path, mode);
            
            FreeVec(path);
        }
        else
        {
            SetIoErr(ERROR_NO_FREE_STORE);
            return NULL;
        }
    }
    
    return file;
}

BOOL __CloseIcon_WB(BPTR file, struct IconBase *IconBase)
{
    return Close(file);
}

BPTR __OpenDefaultIcon_WB(CONST_STRPTR name, LONG mode, struct IconBase *IconBase)
{
    static const char * const  readPaths[]  = { "ENV:SYS", "ENVARC:SYS", NULL };
    static const char * const  writePaths[] = { "ENV:SYS", NULL };
    const char * const        *paths        = NULL;
    CONST_STRPTR               path         = NULL;
    BPTR                       file         = NULL;
    UBYTE                      i;
    CONST_STRPTR               c;
    
    /* Make sure it's a plain filename; paths are not allowed */
    for (c = name; *c != 0; c++)
        if (*c == '/' || *c == ':') return NULL;
    
    if (mode == MODE_OLDFILE) paths = readPaths;
    else                      paths = writePaths;
    
    /* Attempt to open the icon from each path in turn */
    for (i = 0, path = paths[0]; path != NULL; i++, path = paths[i])
    {
        TEXT  buffer[64]; /* Filename buffer */

        if (strlen(path) + strlen(name) + 11 <= sizeof(buffer))
        {
            strcpy(buffer, path);
            strcat(buffer, "/def_");
            strcat(buffer, name);
            strcat(buffer, ".info");
            if ((file = Open(buffer, mode)) != NULL)
            {
                break;
            }
        }
    }
    
    return file;
}

BOOL __CloseDefaultIcon_WB(BPTR file, struct IconBase *IconBase)
{
    return Close(file);
}

/* Any last-minute changes to the Icon go here before it
 * is returned.
 */
VOID __PrepareIcon_WB(struct DiskObject *icon, struct IconBase *IconBase)
{
    /* Ensure that do_DrawerData exists for WBDISK and WBDRAWER objects */
    if ((icon->do_Type == WBDISK || icon->do_Type == WBDRAWER) &&
        icon->do_DrawerData == NULL) {
        icon->do_DrawerData = AllocMemIcon(icon, sizeof(struct DrawerData),
            MEMF_PUBLIC | MEMF_CLEAR, IconBase);
        if (icon->do_DrawerData == NULL)
            return;
        icon->do_DrawerData->dd_NewWindow.LeftEdge = 50;
        icon->do_DrawerData->dd_NewWindow.TopEdge = 50;
        icon->do_DrawerData->dd_NewWindow.Width = 400;
        icon->do_DrawerData->dd_NewWindow.Height = 150;
    }

    /* Clean out dangling pointers that should no longer be
     * present
     */
    if (icon->do_DrawerData) {
        icon->do_DrawerData->dd_NewWindow.FirstGadget = NULL;
        icon->do_DrawerData->dd_NewWindow.Screen = NULL;
        icon->do_DrawerData->dd_NewWindow.BitMap = NULL;
    }

    return;
}



struct DiskObject *__ReadIcon_WB(BPTR file, struct IconBase *IconBase)
{
    struct DiskObject *icon;

    icon = NewDiskObject(0);
    if (icon == NULL)
        return NULL;

    Seek(file, 0, OFFSET_BEGINNING);
    if (ReadIconFile(NATIVEICON(icon), file, IconBase))
        return icon;

    FreeDiskObject(icon);
    return NULL;
}

/* Absolute offsets for the 'only update position' mode */
#define OFFSET_DO_CURRENTX      0x3a
#define OFFSET_DO_CURRENTY      0x3e

BOOL __WriteIcon_WB(BPTR file, struct DiskObject *icon, struct TagItem *tags, struct IconBase *IconBase)
{
    if (GetTagData(ICONPUTA_OnlyUpdatePosition, FALSE, tags)) {
        LONG tmp;

        Seek(file, OFFSET_DO_CURRENTX, OFFSET_BEGINNING);
        tmp = icon->do_CurrentX;
        if (Write(file, &tmp, sizeof(tmp)) == sizeof(tmp)) {
            Seek(file, OFFSET_DO_CURRENTY, OFFSET_BEGINNING);
            tmp = icon->do_CurrentY;
            if (Write(file, &tmp, sizeof(tmp)) == sizeof(tmp)) {
                return TRUE;
            }
        }
        return FALSE;
    }

    if (icon->do_Gadget.GadgetRender == NULL) {
        SetIoErr(ERROR_OBJECT_WRONG_TYPE);
        return FALSE;
    }

    return WriteIconFile(icon, file, IconBase);
}

BPTR __LockObject_WB(CONST_STRPTR name, LONG mode, struct IconBase *IconBase)
{
    BPTR lock;
   
    if (name == NULL)
        return NULL;

    lock = Lock(name, mode);
    
    if (lock == NULL && (strlen(name) >= 5) && Stricmp(name + strlen(name) - 5, ":Disk") == 0)
    {
        ULONG  length = strlen(name) - 4; /* Up to the ':' */
        STRPTR buffer = AllocVec(length + 1, MEMF_ANY);

        if (buffer != NULL)
        {
            CopyMem((APTR)name, buffer, length);
            buffer[length] = 0;
            lock = Lock(buffer, mode);
            FreeVec(buffer);
        }
    }
    
    return lock;
}

VOID __UnLockObject_WB(BPTR lock, struct IconBase *IconBase)
{
    if (lock != NULL) UnLock(lock);
}

CONST_STRPTR GetDefaultIconName(LONG type)
{
    static const char * const defaultNames[] =
    {
        "Disk",         /* WBDISK    (1) */
        "Drawer",       /* WBDRAWER  (2) */
        "Tool",         /* WBTOOL    (3) */
        "Project",      /* WBPROJECT (4) */
        "Trashcan",     /* WBGARBAGE (5) */
        "Device",       /* WBDEVICE  (6) */
        "Kick",         /* WBKICK    (7) */
        "AppIcon"       /* WBAPPICON (8) */
    };
    
    if (type >= 1 && type <= 8)
    {
        return defaultNames[type - 1];
    }
    else
    {
        return NULL;
    }
}

/* Use the FNV-1 hash function over the object's pointer.
 * http://en.wikipedia.org/wiki/Fowler-Noll-Vo_hash_function
 */
LONG CalcIconHash(struct DiskObject *dobj)
{
    const ULONG FNV1_32_Offset = 2166136261UL;
    const ULONG FNV1_32_Prime  = 16777619UL;
    IPTR data = (IPTR)dobj;
    ULONG hash;
    int i;

    hash = FNV1_32_Offset;
    for (i = 0; i < sizeof(IPTR); i++) {
            hash *= FNV1_32_Prime;
            hash ^= data & 0xff;
            data >>= 8;
    }
        
    return hash & (ICONLIST_HASHSIZE-1);
}

/* Allocate memory, and save to an icon's freelist
 */
APTR AllocMemIcon(struct DiskObject *icon, IPTR size, ULONG req,
    struct IconBase *IconBase)
{
    APTR mem;

    mem = AllocMem(size, req);
    if (mem != NULL)
    {
        if (!AddFreeList((struct FreeList *)&icon[1], mem, size))
        {
            FreeMem(mem, size);
            mem = NULL;
        }
    }

    return mem;
}

VOID AddIconToList(struct NativeIcon *icon, struct IconBase *IconBase)
{
    LONG hash;
    
    hash = CalcIconHash(&icon->ni_DiskObject);
    
    ObtainSemaphore(&IconBase->iconlistlock);
    AddTail((struct List *)&IconBase->iconlists[hash], (struct Node *)&icon->ni_Node);
    ReleaseSemaphore(&IconBase->iconlistlock);
}

VOID RemoveIconFromList(struct NativeIcon *icon, struct IconBase *IconBase)
{
    ObtainSemaphore(&IconBase->iconlistlock);
    Remove((struct Node *)&icon->ni_Node);
    ReleaseSemaphore(&IconBase->iconlistlock);
}

struct NativeIcon *GetNativeIcon(struct DiskObject *dobj, struct IconBase *IconBase)
{
    struct NativeIcon *icon, *reticon = NULL;
    LONG               hash;
    
    hash = CalcIconHash(dobj);
    
    ObtainSemaphoreShared(&IconBase->iconlistlock);
    for (icon = (struct NativeIcon *)IconBase->iconlists[hash].mlh_Head;
         icon->ni_Node.mln_Succ != NULL;
         icon = (struct NativeIcon *)icon->ni_Node.mln_Succ)
    {
        if (dobj == &icon->ni_DiskObject)
        {
            reticon = icon;
            break;
        }
    }
    ReleaseSemaphore(&IconBase->iconlistlock);
    
    return reticon;
}

APTR DemandOpenLibrary(struct Library **libp, CONST_STRPTR libname, ULONG version)
{
    if (*libp == NULL)
        *libp = OpenLibrary(libname, version);

    return *libp;
}
