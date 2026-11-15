/*
    Copyright (C) 2003-2026, The AROS Development Team. All rights reserved.
*/

#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/doshunks.h>
#include <workbench/workbench.h>
#include <workbench/icon.h>
#include <utility/hooks.h>

#include "icon_intern.h"
#include "identify.h"

/*** Prototypes *************************************************************/
struct DiskObject *__GetDefaultIconFromType_WB(LONG type, const struct TagItem *tags, struct IconBase *IconBase);

/*** Macros *****************************************************************/
#define GetDefaultIconFromType(type, tags) (__GetDefaultIconFromType_WB((type), (tags), IconBase))

/*** Functions **************************************************************/
LONG __FindType_WB(BPTR lock, struct IconBase *IconBase)
{
    LONG                  type = -1;
    struct FileInfoBlock *fib  = AllocDosObject(DOS_FIB, NULL);

    if (fib != NULL)
    {
        if (Examine(lock, fib))
        {
            /* Identify object ---------------------------------------------*/
            if (fib->fib_DirEntryType == ST_ROOT)
            {
                /* It's a disk/volume/root ---------------------------------*/
                type = WBDISK;
            }
            else if (fib->fib_DirEntryType > 0)
            {
                /* It's a directory ----------------------------------------*/
                type = WBDRAWER;
            }
            else
            {
                /* It's a file: executable files are tools */
                if ((fib->fib_Protection & FIBF_EXECUTE) == 0)
                {
                    type = WBTOOL;
                }
                else
                {
                    type = WBPROJECT;
                }
            }
        }

        FreeDosObject(DOS_FIB, fib);
    }

    return type;
}

/* The generic icon of a type if there is no icon yet, with the type forced */
static struct DiskObject *TypedDefaultIcon(struct DiskObject *icon, LONG type,
                                           const struct TagItem *tags, struct IconBase *IconBase)
{
    if (icon == NULL)
    {
        icon = GetDefaultIconFromType(type, tags);
    }

    if (icon != NULL)
    {
        /* Force the icon type */
        icon->do_Type = type;
    }

    return icon;
}

/* A file that starts with a hunk header is a program */
static BOOL IsExecutable(BPTR file, struct IconBase *IconBase)
{
    ULONG magic;

    return file != NULL && Read(file, &magic, 4) == 4 && magic == HUNK_HEADER;
}

struct DiskObject *__FindDefaultIcon_WB
(
    struct IconIdentifyMsg *iim, struct IconBase *IconBase
)
{
    struct DiskObject *icon = NULL;

    /* Identify object -----------------------------------------------------*/
    if (iim->iim_FIB->fib_DirEntryType == ST_ROOT)
    {
        /* It's a disk/volume/root -------------------------------------*/
        icon = TypedDefaultIcon(NULL, WBDISK, iim->iim_Tags, IconBase);
    }
    else if (iim->iim_FIB->fib_DirEntryType > 0)
    {
        /* It's a directory --------------------------------------------*/

        /* Check if it is a trashcan directory */
        if (iim->iim_ParentLock != NULL)
        {
            /* Is iim_ParentLock a root? */
            BPTR root = ParentDir(iim->iim_ParentLock);

            if (root == NULL)
            {
                /* Yes, it's a root. Is this its trashcan? */
                if (Stricmp(iim->iim_FIB->fib_FileName, "Trashcan") == 0 ||
                    Stricmp(iim->iim_FIB->fib_FileName, ".recycled") == 0)
                {
                    icon = GetDefaultIconFromType(WBGARBAGE, iim->iim_Tags);
                }
            }
            else
            {
                UnLock(root);
            }
        }

        /* Fall back to generic drawer icon */
        icon = TypedDefaultIcon(icon, icon ? WBGARBAGE : WBDRAWER, iim->iim_Tags, IconBase);
    }
    else
    {
        /* It's a file: programs are tools, the others projects */
        icon = TypedDefaultIcon(NULL,
            IsExecutable(iim->iim_FileHandle, IconBase) ? WBTOOL : WBPROJECT,
            iim->iim_Tags, IconBase);
    }

    return icon;
}

/*** Support functions ******************************************************/
struct DiskObject *__GetDefaultIconFromType_WB
(
    LONG type, const struct TagItem *tags, struct IconBase *IconBase
)
{
    struct TagItem deftags[2];

    deftags[0].ti_Tag = ICONGETA_GetDefaultType;
    deftags[0].ti_Data = type;
    deftags[1].ti_Tag = TAG_DONE;
    return GetIconTagList(NULL, deftags);
}
