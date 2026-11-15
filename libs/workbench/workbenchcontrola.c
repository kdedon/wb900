/*
    Copyright (C) 1995-2011, The AROS Development Team. All rights reserved.

    Desc: Change global options and control the Workbench in various ways.
*/

#include <exec/ports.h>
#include <utility/tagitem.h>
#include <intuition/intuition.h>
#include <workbench/workbench.h>
#include <proto/utility.h>

#include "workbench_intern.h"
#include "support.h"

/*****************************************************************************

    NAME */

    #include <proto/workbench.h>

    AROS_LH2(BOOL, WorkbenchControlA,

/*  SYNOPSIS */
    AROS_LHA(STRPTR,           name, A0),
    AROS_LHA(struct TagItem *, tags,   A1),

/*  LOCATION */
    struct WorkbenchBase *, WorkbenchBase, 18, Workbench)

/*  FUNCTION

    INPUTS

    RESULT

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

******************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct TagItem *tstate = tags,
                   *tag;
    BOOL            rc       = TRUE;

#   define STORE(pointer, value) (pointer != NULL ? *pointer = (value) : (value))

    while ((tag = NextTagItem(&tstate)) != NULL)
    {
        switch (tag->ti_Tag)
        {
            case WBCTRLA_DuplicateSearchPath:
                STORE
                (
                    (BPTR *) tag->ti_Data,
                    DuplicateSearchPath(WorkbenchBase->wb_SearchPath)
                );
                break;

            case WBCTRLA_FreeSearchPath:
                FreeSearchPath((BPTR) tag->ti_Data);
                break;

            case WBCTRLA_GetDefaultStackSize:
                STORE
                (
                    (ULONG *) tag->ti_Data,
                    WorkbenchBase->wb_DefaultStackSize
                );
                break;

            case WBCTRLA_SetDefaultStackSize:
                WorkbenchBase->wb_DefaultStackSize = tag->ti_Data;
                break;
        }
    }

    return rc;

#   undef STORE

    AROS_LIBFUNC_EXIT
} /* WorkbenchControlA */

