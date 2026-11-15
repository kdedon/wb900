/*
    Copyright (C) 1995-2014, The AROS Development Team. All rights reserved.
*/


#include <utility/tagitem.h>
#include <proto/icon.h>
#include <proto/workbench.h>

#include "icon_intern.h"

/*****************************************************************************

    NAME */

    AROS_LH3(BOOL, PutIconTagList,

/*  SYNOPSIS */
        AROS_LHA(CONST_STRPTR,        name, A0),
        AROS_LHA(struct DiskObject *, icon, A1),
        AROS_LHA(struct TagItem *,    tags, A2),

/*  LOCATION */
        struct IconBase *, IconBase, 31, Icon)

/*  FUNCTION

    INPUTS

    RESULT

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT
    
    struct TagItem       *tstate  = tags;
    struct TagItem       *tag     = NULL;
    
    BOOL    success               = FALSE;
    LONG    defaultType           = -1;
    STRPTR  defaultName           = NULL;
    LONG error = 0;
    LONG   *errorCode             = NULL;
    
    BOOL    notifyWorkbench       = FALSE;
    BOOL    onlyUpdatePosition    = FALSE;
    
    /* Check input parameters ----------------------------------------------*/
    if (icon == NULL) return FALSE;

    /* Parse taglist -------------------------------------------------------*/
    while ((tag = NextTagItem(&tstate)) != NULL)
    {
        switch (tag->ti_Tag)
        {
                
            case ICONPUTA_PutDefaultType:
                defaultType = tag->ti_Data;
                tag->ti_Tag = TAG_IGNORE; /* avoid recursion */
                break;
                
            case ICONPUTA_PutDefaultName:
                defaultName = (STRPTR) tag->ti_Data;
                tag->ti_Tag = TAG_IGNORE; /* avoid recursion */
                break;
            
            case ICONA_ErrorCode:
                errorCode = (LONG *) tag->ti_Data;
                break;
            
            case ICONPUTA_NotifyWorkbench:
                notifyWorkbench = tag->ti_Data;
                break;
            case ICONPUTA_OnlyUpdatePosition:
                onlyUpdatePosition = tag->ti_Data;
                break;
        }
    }

    if (defaultType != -1)
    {
        CONST_STRPTR defaultIconName = GetDefaultIconName(defaultType);
        
        if (defaultIconName != NULL)
        {
            struct TagItem deftags[2];

            deftags[0].ti_Tag = ICONPUTA_PutDefaultName;
            deftags[0].ti_Data = (IPTR) defaultIconName;
            deftags[1].ti_Tag = TAG_MORE;
            deftags[1].ti_Data = (IPTR) tags;
            success = PutIconTagList(NULL, icon, deftags);
            if (!success)
                error = IoErr();
        }
    }
    else if (defaultName != NULL)
    {
        BPTR file = OpenDefaultIcon(defaultName, MODE_NEWFILE);
        
        if (file != NULL)
        {
            success = WriteIcon(file, icon, tags);
            if (!success)
                error = IoErr();
            CloseDefaultIcon(file);
        }
    }
    else if (name != NULL)
    {
        BPTR file = OpenIcon(name, onlyUpdatePosition ? MODE_OLDFILE : MODE_NEWFILE);
        if (file != NULL)
        {
            success = WriteIcon(file, icon, tags);
            if (!success)
                error = IoErr();
            CloseIcon(file);
        }
    }

    /* Notify workbench if we added/changed the icon */
    if (success && name && notifyWorkbench && WorkbenchBase) {
        BPTR lock, parent;

        lock = Lock(name, SHARED_LOCK);
        if (lock) {
            parent = ParentDir(lock);
            if (parent) {
                UpdateWorkbench(FilePart(name), parent, UPDATEWB_ObjectAdded);
                UnLock(parent);
            }
            UnLock(lock);
        }
    }

    SetIoErr(error);
    if (errorCode != NULL)
        *errorCode = error;
    return success;

    AROS_LIBFUNC_EXIT
} /* PutIconTagList() */

