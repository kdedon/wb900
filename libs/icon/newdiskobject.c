/*
    Copyright (C) 1995-2013, The AROS Development Team. All rights reserved.
*/

#include <exec/types.h>
#include <workbench/icon.h>
#include <proto/icon.h>

#include "icon_intern.h"


/*****************************************************************************

    NAME */

    AROS_LH1(struct DiskObject *, NewDiskObject,

/*  SYNOPSIS */
        AROS_LHA(ULONG, type, D0),

/*  LOCATION */
        struct IconBase *, IconBase, 29, Icon)

/*  FUNCTION
        Creates an empty DiskObject structure.

    INPUTS
        type - WBDISK, WBDRAWER, WBTOOL, WBPROJECT,
               WBGARBAGE, WBDEVICE or WBKICK

    RESULT

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    APTR result = NULL;
    struct NativeIcon *ni;

    ni = (struct NativeIcon *)AllocMem(sizeof(struct NativeIcon), MEMF_CLEAR);
    if (ni == NULL)
        return NULL;

    ni->ni_DiskObject.do_Type = type;

    NewList(&ni->ni_FreeList.fl_MemList);

    AddIconToList(ni, IconBase);
    result = &ni->ni_DiskObject;

    return result;

    AROS_LIBFUNC_EXIT
}
