/*
    Copyright (C) 1995-2023, The AROS Development Team. All rights reserved.
*/

#include <workbench/icon.h>

#include "icon_intern.h"
#include "support.h"
#include "support_builtin.h"
#include "identify.h"

/*****************************************************************************

    NAME */
#include <proto/icon.h>

        AROS_LH2(struct DiskObject *, GetIconTagList,

/*  SYNOPSIS */
        AROS_LHA(CONST_STRPTR, name, A0),
        AROS_LHA(const struct TagItem *, tags, A1),
/*  LOCATION */
        struct IconBase *, IconBase, 30, Icon)

/*  FUNCTION
        Opens an icon from disk.

    INPUTS
        name - object path (without .info extension). May be NULL when
            retrieving a default icon.
        tags - tag list containing tags described below.

    TAGS
        ICONA_ErrorCode (LONG *)
        ICONGETA_GetDefaultType (LONG) - Default icon type to get. This
                overrides the "name" parameter.
        ICONGETA_GetDefaultName (STRPTR) - Name of default icon to get. This
                overrides the "name" parameter.
        ICONGETA_FailIfUnavailable (BOOL) - Find a default icon if there is no
                specific icon.
        ICONGETA_IsDefaultIcon (LONG *) - Upon completion of this function, the
            referenced LONG will be set to a boolean value indicating whether
            the returned icon is a default icon.
        ICONGETA_RemapIcon (BOOL)
        ICONGETA_Screen (struct Screen *)

    RESULT

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct TagItem       *tstate            = (struct TagItem *)tags;
    struct TagItem *tag;
    struct DiskObject    *icon              = NULL;
    LONG                  defaultType       = -1;
    CONST_STRPTR          defaultName       = NULL;
    IPTR                  failIfUnavailable = TRUE;
    LONG                 *isDefaultIcon     = NULL;
    IPTR                  remapIcon            = TRUE;
    struct Screen        *screen               = NULL;
    LONG                 error                 = 0;
    LONG                 *errorCode            = NULL;
    struct TagItem        deftags[2];

#   define SET_ISDEFAULTICON(value) (isDefaultIcon != NULL ? *isDefaultIcon = (value) : (value))

    /* Parse taglist -------------------------------------------------------*/
    while ((tag = NextTagItem(&tstate)) != NULL)
    {
        switch (tag->ti_Tag)
        {
            case ICONGETA_GetDefaultType:
                if (defaultType == -1)
                    defaultType = tag->ti_Data;
                break;

            case ICONGETA_GetDefaultName:
                if (defaultName == NULL)
                    defaultName = (CONST_STRPTR) tag->ti_Data;
                break;

            case ICONGETA_FailIfUnavailable:
                failIfUnavailable = tag->ti_Data;
                break;

            case ICONGETA_IsDefaultIcon:
                isDefaultIcon  = (LONG *) tag->ti_Data;
                SET_ISDEFAULTICON(FALSE);
                break;

            case ICONGETA_RemapIcon:
                remapIcon = tag->ti_Data;
                break;

            case ICONGETA_Screen:
                screen = (struct Screen *) tag->ti_Data;
                break;

            case ICONA_ErrorCode:
                errorCode = (LONG *) tag->ti_Data;
                break;
        }
    }

    if (defaultType != -1 || defaultName != NULL)
    {
        if (defaultName != NULL)
        {
            BPTR file = OpenDefaultIcon(defaultName, MODE_OLDFILE);

            if (file != NULL)
            {
                icon = ReadIcon(file);
                CloseDefaultIcon(file);
                SET_ISDEFAULTICON(TRUE);
            }
        }

        if (icon == NULL && defaultType != -1 && defaultName == NULL)
        {
            CONST_STRPTR defaultIconName = GetDefaultIconName(defaultType);

            if (defaultIconName != NULL)
            {
                deftags[0].ti_Tag = ICONGETA_GetDefaultName;
                deftags[0].ti_Data = (IPTR)defaultIconName;
                deftags[1].ti_Tag = TAG_DONE;
                icon = GetIconTagList(NULL, deftags);

                if (icon == NULL)
                {
                    icon = GetBuiltinIcon(defaultType);
                }
                SET_ISDEFAULTICON(TRUE);
            }
        }
    }
    else if (name != NULL)
    {
        BPTR file = OpenIcon(name, MODE_OLDFILE);

        if (file != NULL)
        {
            icon = ReadIcon(file);
            CloseIcon(file);

            if (icon != NULL && icon->do_Type == 0)
            {
                /* Force the icon type */
                BPTR lock = LockObject(name, ACCESS_READ);

                if (lock != NULL)
                {
                    LONG type = FindType(lock);

                    if (type != -1) icon->do_Type = type;

                    UnLockObject(lock);
                }
            }
            /*
             * If everything else fails, just lie. Not having do_Type set
             * causes problems.
             */
            if (icon != NULL && icon->do_Type == 0)
                icon->do_Type = WBPROJECT;
        }
        else
        {
            error = IoErr();
        }
    } else {
        /* NULL name = return empty DiskObject */
        icon = NewDiskObject(0);
    }

    /* Try to identify it by name or type */
    if (icon == NULL && !failIfUnavailable)
    {
        struct IconIdentifyMsg iim;

        error = 0;
        if (name && (iim.iim_FileLock = LockObject(name, ACCESS_READ)) != NULL)
        {
            if ((iim.iim_FIB = AllocDosObject(DOS_FIB, NULL)) != NULL)
            {
                if (Examine(iim.iim_FileLock, iim.iim_FIB))
                {
                    iim.iim_SysBase     = (struct Library *) SysBase;
                    iim.iim_DOSBase     = (struct Library *) DOSBase;
                    iim.iim_UtilityBase = (struct Library *) UtilityBase;
                    iim.iim_IconBase    = (struct Library *) IconBase;
                    iim.iim_Tags        = tags;

                    iim.iim_ParentLock  = ParentDir(iim.iim_FileLock);
                    if (iim.iim_ParentLock == NULL && IoErr() == 0)
                        iim.iim_FIB->fib_DirEntryType = ST_ROOT;
                    iim.iim_FileHandle  = iim.iim_FIB->fib_DirEntryType < 0 ?
                        Open(name, MODE_OLDFILE) : NULL;

                    if (LB(IconBase)->ib_IdentifyHook != NULL)
                    {
                        /* Use user-provided identify hook */
                        icon = (struct DiskObject *) CALLHOOKPKT
                        (
                            LB(IconBase)->ib_IdentifyHook, NULL, &iim
                        );

                        if (icon != NULL)
                        {
                            /*
                                Sanity check since we don't trust the
                                user-provided hook too much. ;-)
                            */

                            LONG type = FindType(iim.iim_FileLock);
                            if (type != -1) icon->do_Type = type;
                        }
                    }

                    if (icon == NULL)
                    {
                        /* Fall back to the default identify function */
                        icon = FindDefaultIcon(&iim);
                    }

                    if (iim.iim_ParentLock != NULL) UnLock(iim.iim_ParentLock);
                    if (iim.iim_FileHandle != NULL) Close(iim.iim_FileHandle);
                }
                FreeDosObject(DOS_FIB, iim.iim_FIB);
            }
            else
            {
                error = IoErr();
            }

            UnLockObject(iim.iim_FileLock);
        }

        if (icon == NULL && defaultType > 0) {
            icon = GetBuiltinIcon(defaultType);
        }

        if (icon == NULL) {
            LONG type = (name && Stricmp("Disk", name)==0) ? WBDISK : WBPROJECT;
            icon = GetBuiltinIcon(type);
        }
    }

    if (!icon)
        goto exit;

    if (remapIcon) {
        if (screen == NULL)
            screen = LB(IconBase)->ib_Screen;
    } else {
        screen = NULL;
    }

    /* Any last-minute fixups */
    PrepareIcon(icon);

    LayoutIconA(icon, screen, (struct TagItem *)tags);

exit:
    /* Set error code */
    if (errorCode != NULL)
        *errorCode = error;

    SetIoErr(error);

    return icon;

    AROS_LIBFUNC_EXIT

#   undef SET_ISDEFAULTICON
} /* GetIconTagList() */
