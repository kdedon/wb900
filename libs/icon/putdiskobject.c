/*
    Copyright (C) 1995-2007, The AROS Development Team. All rights reserved.
*/

#include "icon_intern.h"

/*****************************************************************************

    NAME */
#include <proto/icon.h>

        AROS_LH2(BOOL, PutDiskObject,

/*  SYNOPSIS */
        AROS_LHA(CONST_STRPTR,        name, A0),
        AROS_LHA(struct DiskObject *, icon, A1),

/*  LOCATION */
        struct IconBase *, IconBase, 14, Icon)

/*  FUNCTION
        Writes icon to disk.
        
    INPUTS
        name - filename, ".info" will be appended.
        icon - diskobject to write
        
    RESULT
        TRUE on success, FALSE on error
        
    NOTES

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT
    
    struct TagItem tags[2];

    tags[0].ti_Tag = ICONPUTA_NotifyWorkbench;  tags[0].ti_Data = TRUE;
    tags[1].ti_Tag = TAG_DONE;
    return PutIconTagList(name, icon, tags);
    
    AROS_LIBFUNC_EXIT
} /* PutDiskObject() */
