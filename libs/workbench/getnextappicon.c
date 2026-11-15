/*
    Copyright (C) 1995-2020, The AROS Development Team. All rights reserved.

    Desc: Accesses AppIcon information from workbench.library.
*/


#include <exec/types.h>
#include <exec/ports.h>
#include <utility/tagitem.h>
#include <dos/dos.h>
#include <graphics/gfx.h>

#include <proto/utility.h>

#include "workbench_intern.h"
#include <workbench/workbench.h>


/*****************************************************************************

    NAME */
    #include <proto/workbench.h>

        AROS_LH2(struct DiskObject *, GetNextAppIcon,
/*  SYNOPSIS */
        AROS_LHA(struct DiskObject *, lastdiskobj,  A0),
        AROS_LHA(char *,              text,         A1),

/*  LOCATION */
        struct WorkbenchBase *, WorkbenchBase, 27, Workbench)

/*  FUNCTION

    Accesses AppIcon information from workbench.library. This function
    is meant for iterations through wblibs' AppIcon storage as needed
    to display those.
        Initialised with a NULL as the first argument, it iterates
    through all AppIcons stored in workbench.library by returning the
    pointer to the next AppIcon's DiskObject structure and copies the
    name of the given AppIcon to the given array. The function returns
    a NULL if the end of AppIcon list was reached or if no AppIcons
    were stored.

    INPUTS

    lastdiskobj  --  NULL (initial value) or pointer to a DiskObject
                     structure stored in workbench.library
    text         --  char array pointer to store AppIcon's name in

    RESULT

    A pointer to an DiskObject structure -- which should be used within
    the next function call to access the next AppIcon -- or NULL if no
    AppIcons were found or end of list was reached.

    NOTES

    EXAMPLE

      struct DiskObject *_nb_dob = NULL;
      char text[32];

      while ( _nb_dob = GetNextAppIcon(_nb_dob, &text) )
      {
          printf("appicon found: %s \n", text);
      }


    BUGS

    SEE ALSO

    AddAppIconA(), RemoveAppIcon(), WorkbenchControlA(), icon.library/DrawIconStateA()

    INTERNALS

******************************************************************************/
{
    AROS_LIBFUNC_INIT

    /* get wblibs list of appicons */
    struct List *appiconlist = &WorkbenchBase->wb_AppIcons;
    struct AppIcon *ai, *next = NULL;

    LockWorkbenchShared();

    /* The AppIcon after lastdiskobj's; the first if it is NULL or not found */
    for (ai = (struct AppIcon *)appiconlist->lh_Head;
         ai->ai_Node.mln_Succ != NULL;
         ai = (struct AppIcon *)ai->ai_Node.mln_Succ)
    {
        if (ai->ai_DiskObject == lastdiskobj)
        {
            next = (struct AppIcon *)ai->ai_Node.mln_Succ;
            break;
        }
    }
    if (ai->ai_Node.mln_Succ == NULL)
        next = (struct AppIcon *)appiconlist->lh_Head;

    UnlockWorkbench();

    /* NULL after the last one */
    if (next->ai_Node.mln_Succ == NULL)
        return NULL;

    strcpy(text, next->ai_Text);
    return next->ai_DiskObject;

    AROS_LIBFUNC_EXIT
} /* GetNextAppIcon() */
