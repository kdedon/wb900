/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc:
*/

#include <proto/exec.h>
#include <exec/tasks.h>
#include <graphics/gfxbase.h>

#include "graphics_intern.h"

/*****************************************************************************

    NAME */

        AROS_LH0(VOID, WaitTOF,

/*  SYNOPSIS */

/*  LOCATION */
        struct GfxBase *, GfxBase, 45, Graphics)

/*  FUNCTION

    Wait for vertical blank.

    INPUTS

    RESULT

    Adds the task to the TOF queue; it will be signalled when the vertical
    blank interrupt occurs.

    NOTES
    There is no vertical blank: this waits for the next 100 Hz tick (the
    INTB_VERTB server chain), between 0 and 10 ms.

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

    HISTORY

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct Node wait;  /* We cannot use the task's node here as that is
                          used to queue the task in Wait() */

    wait.ln_Name = (char *)FindTask(NULL);
    SetSignal(0, SIGF_SINGLE);

    /* The tick signals every waiter, so a tick before Wait() is not lost */
    Disable();
    AddTail((struct List *)&GfxBase->TOF_WaitQ, (struct Node *)&wait);
    Enable();
    Wait(SIGF_SINGLE);
    Disable();
    Remove((struct Node *)&wait);
    Enable();
    /* nor one between Wait() and Remove() left for the next Wait() */
    SetSignal(0, SIGF_SINGLE);

    AROS_LIBFUNC_EXIT

} /* WaitTOF */
