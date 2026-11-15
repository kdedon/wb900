#ifndef __WORKBENCH_SUPPORT_H__
#define __WORKBENCH_SUPPORT_H__

/*
    Copyright (C) 1995-2003, The AROS Development Team. All rights reserved.

    Headers for the miscellanous support functions.
*/

#include "workbench_intern.h"

/*** Prototypes ************************************************************/
STRPTR __AllocateNameFromLock(BPTR lock, struct WorkbenchBase *WorkbenchBase);
STRPTR __StrDup(CONST_STRPTR string, struct WorkbenchBase *WorkbenchBase);
BPTR   __DuplicateSearchPath(BPTR list, struct WorkbenchBase *WorkbenchBase);
VOID   __FreeSearchPath(BPTR list, struct WorkbenchBase *WorkbenchBase);

/*** Macros *****************************************************************/
#define AllocateNameFromLock(lock) (__AllocateNameFromLock((lock), WorkbenchBase))
#define StrDup(string) (__StrDup((string), WorkbenchBase))
#define DuplicateSearchPath(list) (__DuplicateSearchPath((list), WorkbenchBase))
#define FreeSearchPath(list) (__FreeSearchPath((list), WorkbenchBase))

#endif /* __WORKBENCH_SUPPORT_H__ */
