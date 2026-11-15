/*
    Copyright (C) 1995-2007, The AROS Development Team. All rights reserved.
*/

#include "icon_intern.h"

/*****************************************************************************

    NAME */
#include <proto/icon.h>

        AROS_LH1(BOOL, PutDefDiskObject,

/*  SYNOPSIS */
        AROS_LHA(struct DiskObject *, icon, A0),

/*  LOCATION */
        struct IconBase *, IconBase, 21, Icon)

/*  FUNCTION
        Puts a new default icon for a certain type.

    INPUTS
        diskObject  - diskObject struct describing icon to put as new
                      default icon.

    RESULT
        TRUE if success, else FALSE. Error may be obtained via IoErr().

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO
        GetDefDiskObject(), PutDiskObject()

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT
    
    struct TagItem tags[3];

    tags[0].ti_Tag = ICONPUTA_PutDefaultType;   tags[0].ti_Data = icon->do_Type;
    tags[1].ti_Tag = ICONPUTA_NotifyWorkbench;  tags[1].ti_Data = TRUE;
    tags[2].ti_Tag = TAG_DONE;
    return PutIconTagList(NULL, icon, tags);
    
    AROS_LIBFUNC_EXIT
} /* PutDefDiskObject() */
