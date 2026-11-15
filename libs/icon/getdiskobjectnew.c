/*
    Copyright (C) 1995-2014, The AROS Development Team. All rights reserved.
*/

#include "icon_intern.h"

/*****************************************************************************

    NAME */
#include <proto/icon.h>

        AROS_LH1(struct DiskObject *, GetDiskObjectNew,

/*  SYNOPSIS */
        AROS_LHA(CONST_STRPTR, name, A0),

/*  LOCATION */
        struct IconBase *, IconBase, 22, Icon)

/*  FUNCTION
        Tries to open the supplied info file via GetDiskObject(). If this
        does not succeed it will try to read the default info file for
        that type of file.

    INPUTS
        name - name of the file to read an icon for.

    RESULT
        DiskObject - pointer to diskobject struct.

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO
        GetDiskObject(), GetDefDiskObject()

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT
        
    struct TagItem tags[2];

    tags[0].ti_Tag = ICONGETA_FailIfUnavailable;    tags[0].ti_Data = FALSE;
    tags[1].ti_Tag = TAG_DONE;
    return GetIconTagList(name, tags);
    
    AROS_LIBFUNC_EXIT
} /* GetDiskObjectNew() */
