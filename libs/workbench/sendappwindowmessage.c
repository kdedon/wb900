/*
    Copyright (C) 1995-2013, The AROS Development Team. All rights reserved.

    Desc: check if the given window is an app window and send it a list of files
*/

#include <exec/types.h>
#include <exec/ports.h>
#include <dos/dos.h>
#include <intuition/intuition.h>

#include <proto/utility.h>

#include "workbench_intern.h"
#include "support_messages.h"
#include <workbench/workbench.h>


/*****************************************************************************

    NAME */
    #include <proto/workbench.h>

        AROS_LH8(BOOL, SendAppWindowMessage,
/*  SYNOPSIS */
        AROS_LHA(struct Window *, win, A0),
        AROS_LHA(ULONG, numfiles, D0),
        AROS_LHA(char **, files, A1),
        AROS_LHA(UWORD, class, D1),
        AROS_LHA(WORD, mousex, D2),
        AROS_LHA(WORD, mousey, D3),
        AROS_LHA(ULONG, seconds, D4),
        AROS_LHA(ULONG, micros, D5),

/*  LOCATION */
        struct WorkbenchBase *, WorkbenchBase, 26, Workbench)

/*  FUNCTION
        This function sends the given list of files to a registered
        AppWindow's application. If the window is not an AppWindow, nothing
        is done.

    INPUTS
        win -  window that should be checked
        numfiles - number of files in the attached array of string pointers
        files - files "list"

    RESULT
        TRUE if the message was sent; the AppWindow's application replies
        it later

    NOTES

    EXAMPLE

        char *FileList[] =
            {"images:image1.png", "images:image2.png", "images:image3.png"};

        SendAppWindowMessage(myWindow, 3, FileList);

    BUGS

    SEE ALSO

    INTERNALS

******************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct AppWindow *aw;
    BOOL success = FALSE;

    if (numfiles == 0) return FALSE;

    LockWorkbenchShared();
    for (aw = (struct AppWindow *)WorkbenchBase->wb_AppWindows.lh_Head;
         aw->aw_Node.mln_Succ != NULL;
         aw = (struct AppWindow *)aw->aw_Node.mln_Succ)
    {
        if (aw->aw_Window == win)
        {
            success = SendAppMessage(aw->aw_MsgPort, AMTYPE_APPWINDOW,
                aw->aw_ID, aw->aw_UserData, numfiles, files, class,
                mousex, mousey, seconds, micros);
            break;
        }
    }
    UnlockWorkbench();

    return success;

    AROS_LIBFUNC_EXIT
} /* SendAppWindowMessage() */
