/*
    Copyright (C) 1995-2020, The AROS Development Team. All rights reserved.

    Miscellanous support functions.
*/


#include <dos/dostags.h>

#include "workbench_intern.h"
#include "support.h"

STRPTR __AllocateNameFromLock(BPTR lock, struct WorkbenchBase *WorkbenchBase)
{
    ULONG  length = 512;
    STRPTR buffer = NULL;
    BOOL   done   = FALSE;
    
    while (!done)
    {
        if (buffer != NULL) FreeVec(buffer);
        
        buffer = AllocVec(length, MEMF_ANY);
        if (buffer != NULL)
        {
            if (NameFromLock(lock, buffer, length))
            {
                done = TRUE;
                break;
            }
            else
            {
                if (IoErr() == ERROR_LINE_TOO_LONG)
                {
                    length += 512;
                    continue;
                }
                else
                {
                    break;
                }
            }
        }
        else
        {
            SetIoErr(ERROR_NO_FREE_STORE);
            break;
        }
    }
    
    if (done)
    {
        return buffer;
    }
    else
    {
        if (buffer != NULL) FreeVec(buffer);
        return NULL;
    }
}

STRPTR __StrDup(CONST_STRPTR str, struct WorkbenchBase *WorkbenchBase)
{
    STRPTR dup;
    ULONG  len;

    if (str == NULL) return NULL;
    
    len = strlen(str);
    dup = AllocVec(len + 1, MEMF_PUBLIC);
    if (dup != NULL) CopyMem((APTR)str, dup, len + 1);
    
    return dup;
}

BPTR __DuplicateSearchPath(BPTR list, struct WorkbenchBase *WorkbenchBase)
{
    BPTR *paths, *current = NULL, *previous = NULL, first = NULL;
    
    for
    (
        paths = (BPTR *) BADDR(list);
        paths != NULL;
        paths = (BPTR *) BADDR(paths[0]) /* next path */
    )
    {
        if ((current = (BPTR *) AllocVec(2 * sizeof(BPTR), MEMF_ANY)) != NULL)
        {
            current[0] = NULL;
            current[1] = DupLock(paths[1]);
            
            if (previous != NULL) previous[0] = (BPTR) MKBADDR(current);
            else                  first       = (BPTR) MKBADDR(current);
            
            previous = current;
        }
        else
        {
            break;
        }
    }
    
    return first;
}

VOID __FreeSearchPath(BPTR list, struct WorkbenchBase *WorkbenchBase)
{
    BPTR *current = BADDR(list);
    
    while (current != NULL)
    {
        BPTR *next = (BPTR *) BADDR(current[0]);
        
        UnLock(current[1]);
        FreeVec(current);
        
        current = next;
    }
}
