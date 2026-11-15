/*
    Copyright (C) 1995-2020, The AROS Development Team. All rights reserved.

    Open a drawer or launch a program.
*/


#include <exec/types.h>
#include <exec/ports.h>
#include <utility/tagitem.h>
#include <intuition/intuition.h>
#include <workbench/workbench.h>
#include <proto/utility.h>
#include <proto/dos.h>


#include "workbench_intern.h"
#include "support.h"
#include "support_messages.h"
#include "handler.h"
#include "handler_support.h"

/*** Prototypes *************************************************************/
static BOOL   CLI_LaunchProgram(CONST_STRPTR command, struct TagItem *tags, struct WorkbenchBase *WorkbenchBase);
static STRPTR CLI_BuildCommandLine(CONST_STRPTR command, struct TagItem *tags, struct WorkbenchBase *WorkbenchBase);
static BOOL   WB_LaunchProgram(BPTR lock, CONST_STRPTR name, struct TagItem *tags, struct WorkbenchBase *WorkbenchBase);
static BOOL   WB_BuildArguments(struct WBStartup *startup, BPTR lock, CONST_STRPTR name, struct TagItem *tags, struct WorkbenchBase *WorkbenchBase);
static BOOL   HandleDrawer(STRPTR name, struct WorkbenchBase *WorkbenchBase);
static BOOL   HandleTool(STRPTR name, LONG isDefaultIcon, struct DiskObject *icon, struct TagItem *tags, struct WorkbenchBase *WorkbenchBase);
static BOOL   HandleProject(STRPTR name, LONG isDefaultIcon, struct DiskObject *icon, struct TagItem *tags, struct WorkbenchBase *WorkbenchBase);

/* Whether a name is a plain file name, without a device or a path */
static BOOL IsPlainName(CONST_STRPTR name)
{
    for (; *name != 0; name++)
        if (*name == '/' || *name == ':')
            return FALSE;
    return TRUE;
}

/*****************************************************************************

    NAME */

        #include <proto/workbench.h>

        AROS_LH2(BOOL, OpenWorkbenchObjectA,

/*  SYNOPSIS */
        AROS_LHA(STRPTR,           name, A0),
        AROS_LHA(struct TagItem *, tags, A1),

/*  LOCATION */
        struct WorkbenchBase *, WorkbenchBase, 16, Workbench)

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

    BOOL                  success       = FALSE;
    LONG                  isDefaultIcon = 42;

    struct DiskObject    *icon;
    struct TagItem        icontags[3];

    icontags[0].ti_Tag = ICONGETA_FailIfUnavailable;
    icontags[0].ti_Data = FALSE;
    icontags[1].ti_Tag = ICONGETA_IsDefaultIcon;
    icontags[1].ti_Data = (IPTR) &isDefaultIcon;
    icontags[2].ti_Tag = TAG_DONE;
    icon = GetIconTagList(name, icontags);

    {
        if (icon != NULL)
        {
            switch (icon->do_Type)
            {
                case WBDISK:
                case WBDRAWER:
                case WBGARBAGE:
                    success = HandleDrawer(name, WorkbenchBase);
                    break;

                case WBTOOL:
                    success = HandleTool(name, isDefaultIcon, icon, tags, WorkbenchBase);
                    break;

                case WBPROJECT:
                    success = HandleProject(name, isDefaultIcon, icon, tags, WorkbenchBase);
                    break;
            }

            FreeDiskObject(icon);
        }
        else
        {
            /*
                Getting (a possibly default) icon for the path failed, and
                therefore there is no such file. We need to search the default
                search path for an executable of that name and launch it. This
                only makes sense if the name is *not* an (absolute or relative)
                path: that is, it must not contain any ':' or '/'.
            */

            if (IsPlainName(name))
            {
                struct CommandLineInterface *cli = Cli();
                if (cli != NULL)
                {
                    BPTR *paths;          /* Path list */
                    BOOL  running = TRUE;

                    /* Iterate over all paths in the path list */
                    for
                    (
                        paths = (BPTR *) BADDR(cli->cli_CommandDir);
                        running == TRUE && paths != NULL;
                        paths = (BPTR *) BADDR(paths[0]) /* next path */
                    )
                    {
                        BPTR cd   = CurrentDir(paths[1]);
                        BPTR lock = Lock(name, SHARED_LOCK);

                        if (lock != NULL)
                        {
                            success = OpenWorkbenchObjectA(name, tags);
                            running = FALSE;

                            UnLock(lock);
                        }

                        CurrentDir(cd);
                    }
                }
            }
        }
    }


    return success;

    AROS_LIBFUNC_EXIT
} /* OpenWorkbenchObjectA() */

/****************************************************************************/

static STRPTR CLI_BuildCommandLine
(
    CONST_STRPTR command, struct TagItem *tags,
    struct WorkbenchBase *WorkbenchBase
)
{
    struct TagItem *tstate   = tags;
    struct TagItem *tag      = NULL;
    BPTR            lastLock = NULL;
    STRPTR          buffer   = NULL;
    ULONG           length   = strlen(command) + 3 /* NULL + 2 '"' */;

    /*-- Calculate length of resulting string ------------------------------*/
    while ((tag = NextTagItem(&tstate)) != NULL)
    {
        switch (tag->ti_Tag)
        {
            case WBOPENA_ArgLock:
                lastLock = (BPTR) tag->ti_Data;
                break;

            case WBOPENA_ArgName:
                if (lastLock != NULL)
                {
                    BPTR cd   = CurrentDir(lastLock);
                    BPTR lock = Lock((STRPTR) tag->ti_Data, ACCESS_READ);
                    if (lock != NULL)
                    {
                        STRPTR path = AllocateNameFromLock(lock);
                        if (path != NULL)
                        {
                            length += 3 /* space + 2 '"' */ + strlen(path);
                            FreeVec(path);
                        }

                        UnLock(lock);
                    }

                    CurrentDir(cd);
                }
                break;
        }
    }

    /*-- Allocate space for command line string ----------------------------*/
    buffer = AllocVec(length, MEMF_ANY);

    if (buffer != NULL)
    {
        buffer[0] = '\0';

        /*-- Build command line --------------------------------------------*/
        strcat(buffer, "\"");
        strcat(buffer, command);
        strcat(buffer, "\"");

        tstate = tags; lastLock = NULL;
        while ((tag = NextTagItem(&tstate)) != NULL )
        {
            switch (tag->ti_Tag)
            {
                case WBOPENA_ArgLock:
                    lastLock = (BPTR) tag->ti_Data;
                    break;

                case WBOPENA_ArgName:
                    if (lastLock != NULL)
                    {
                        BPTR cd   = CurrentDir(lastLock);
                        BPTR lock = Lock((STRPTR) tag->ti_Data, ACCESS_READ);
                        if (lock != NULL)
                        {
                            STRPTR path = AllocateNameFromLock(lock);
                            if (path != NULL)
                            {
                                strcat(buffer, " \"");
                                strcat(buffer, path);
                                strcat(buffer, "\"");
                                FreeVec(path);
                            }

                            UnLock(lock);
                        }

                        CurrentDir(cd);
                    }
                    break;
            }
        }
    }
    else
    {
        SetIoErr(ERROR_NO_FREE_STORE);
    }

    return buffer;
}

/****************************************************************************/

static BOOL CLI_LaunchProgram
(
    CONST_STRPTR command, struct TagItem *tags,
    struct WorkbenchBase *WorkbenchBase
)
{
    BPTR   input       = NULL;
    STRPTR commandline = NULL;
    IPTR                stacksize = WorkbenchBase->wb_DefaultStackSize;
    LONG                priority = 0;
    struct TagItem      *foundTag = NULL;

    struct TagItem      systags[6];

    input = Open("CON:////Output Window/CLOSE/AUTO/WAIT", MODE_OLDFILE);
    if (input == NULL) goto error;

    commandline = CLI_BuildCommandLine(command, tags, WorkbenchBase);
    if (commandline == NULL) goto error;

    if (tags)
    {
        foundTag = FindTagItem(NP_StackSize, tags);
        if (foundTag != NULL)
        {
            stacksize = foundTag->ti_Data;
        }

        foundTag = FindTagItem(NP_Priority, tags);
        if (foundTag != NULL)
        {
            priority = foundTag->ti_Data;
        }
    }

    systags[0].ti_Tag = SYS_Asynch;     systags[0].ti_Data = TRUE;
    systags[1].ti_Tag = SYS_Input;      systags[1].ti_Data = (IPTR) input;
    systags[2].ti_Tag = SYS_Output;     systags[2].ti_Data = (IPTR) NULL;
    systags[3].ti_Tag = NP_StackSize;   systags[3].ti_Data = stacksize;
    systags[4].ti_Tag = NP_Priority;    systags[4].ti_Data = priority;
    systags[5].ti_Tag = TAG_DONE;

    if
    (
        /*
            Launch the program. Note that there is no advantage of doing this
            in the handler, since we need to wait for the return value anyway
            (and thus we cannot cut down the blocking time by returning before
            the program is loaded from disk).
        */

        SystemTagList(commandline, systags) == -1
    )
    {
        goto error;
    }
    FreeVec(commandline);

    return TRUE;

error:
    if (input != NULL) Close(input);
    if (commandline != NULL) FreeVec(commandline);

    return FALSE;
}

/****************************************************************************/

static BOOL WB_BuildArguments
(
    struct WBStartup *startup, BPTR lock, CONST_STRPTR name, struct TagItem *tags,
    struct WorkbenchBase *WorkbenchBase
)
{
    struct TagItem *tstate   = tags,
                   *tag      = NULL;
    BPTR            lastLock = NULL;
    struct WBArg   *args     = NULL;

    /*-- Calculate the number of arguments ---------------------------------*/
    startup->sm_NumArgs = 1;
    while ((tag = NextTagItem(&tstate)) != NULL)
    {
        switch (tag->ti_Tag)
        {
            case WBOPENA_ArgLock:
                lastLock = (BPTR) tag->ti_Data;
                break;

            case WBOPENA_ArgName:
                /*
                    Filter out args where both lock AND name are NULL, since
                    they are completely worthless to the application.
                */
                if (lastLock != NULL || (STRPTR) tag->ti_Data != NULL)
                {
                    startup->sm_NumArgs++;
                }
                break;
        }
    }

    /*-- Allocate memory for the arguments ---------------------------------*/
    args = AllocMem(sizeof(struct WBArg) * startup->sm_NumArgs, MEMF_ANY | MEMF_CLEAR);
    if (args != NULL)
    {
        LONG i = 0;

        startup->sm_ArgList = args;

        /*-- Build the argument list ---------------------------------------*/
        if
        (
               (args[i].wa_Lock = DupLock(lock)) == NULL
            || (args[i].wa_Name = StrDup(name))  == NULL
        )
        {
            goto error;
        }
        i++;

        tstate = tags; lastLock = NULL;
        while ((tag = NextTagItem(&tstate)) != NULL)
        {
            switch (tag->ti_Tag)
            {
                case WBOPENA_ArgLock:
                    lastLock = (BPTR) tag->ti_Data;
                    break;

                case WBOPENA_ArgName:
                    /*
                        Filter out args where both lock AND name are NULL,
                        since they are completely worthless to the application.
                    */
                    if (lastLock != NULL || (STRPTR) tag->ti_Data != NULL)
                    {
                        STRPTR argname = (STRPTR) tag->ti_Data;

                        /* Duplicate the lock */
                        if (lastLock != NULL)
                        {
                            args[i].wa_Lock = DupLock(lastLock);

                            if (args[i].wa_Lock == NULL)
                            {
                                goto error;
                            }
                        }
                        else
                        {
                            args[i].wa_Lock = NULL;
                        }

                        /* Duplicate the name */
                        if (argname != NULL)
                        {
                            args[i].wa_Name = StrDup(argname);

                            if (args[i].wa_Name == NULL)
                            {
                                goto error;
                            }
                        }
                        else
                        {
                            args[i].wa_Name = NULL;
                        }

                        i++;
                    }
                    break;
            }
        }

        return TRUE;
    }

error:
    /* Free allocated resources */
    if (args != NULL)
    {
        int i;

        for (i = 0; i < startup->sm_NumArgs; i++)
        {
            if (args[i].wa_Lock != NULL) UnLock(args[i].wa_Lock);
            if (args[i].wa_Name != NULL) FreeVec(args[i].wa_Name);
        }

        FreeMem(args, sizeof(struct WBArg) * startup->sm_NumArgs);
    }

    startup->sm_NumArgs = 0;
    startup->sm_ArgList = NULL;

    return FALSE;
}

/****************************************************************************/

static BOOL WB_LaunchProgram
(
    BPTR lock, CONST_STRPTR name, struct TagItem *tags,
    struct WorkbenchBase *WorkbenchBase
)
{
    struct WBStartup        *startup = NULL;
    struct WBCommandMessage *message = NULL;
    struct TagItem          *stackTag = NULL;
    struct TagItem          *prioTag = NULL;

    /*-- Allocate memory for messages --------------------------------------*/
    startup = CreateWBS();
    if (startup == NULL)
    {
        SetIoErr(ERROR_NO_FREE_STORE);
        goto error;
    }

    message = CreateWBCM(WBCM_TYPE_LAUNCH);
    if (message == NULL)
    {
        SetIoErr(ERROR_NO_FREE_STORE);
        goto error;
    }

    if (tags)
    {
        stackTag = FindTagItem(NP_StackSize, tags);
        prioTag = FindTagItem(NP_Priority, tags);

        if (stackTag || prioTag)
        {
            message->wbcm_Tags =  AllocateTagItems(3);
            if (message->wbcm_Tags == NULL)
            {
                SetIoErr(ERROR_NO_FREE_STORE);
                goto error;
            }

            message->wbcm_Tags[0].ti_Tag = stackTag ? stackTag->ti_Tag : TAG_IGNORE;
            message->wbcm_Tags[0].ti_Data = stackTag ? stackTag->ti_Data : 0;

            message->wbcm_Tags[1].ti_Tag = prioTag ? prioTag->ti_Tag : TAG_IGNORE;
            message->wbcm_Tags[1].ti_Data = prioTag ? prioTag->ti_Data : 0;

            message->wbcm_Tags[2].ti_Tag = TAG_DONE;
        }
    }

    /*-- Build the arguments array -----------------------------------------*/
    if (!WB_BuildArguments(startup, lock, name, tags, WorkbenchBase))
    {
        goto error;
    }

    /*-- Send message to handler -------------------------------------------*/
    /* NOTE: The handler will deallocate all resources of this message! */
    message->wbcm_Data.Launch.Startup = startup;

    PutMsg(&(WorkbenchBase->wb_HandlerPort), (struct Message *) message);


    return TRUE;

error:
    if (startup != NULL) DestroyWBS(startup);
    if (message != NULL)
    {
        if (message->wbcm_Tags != NULL) FreeTagItems(message->wbcm_Tags);
        DestroyWBCM(message);
    }


    return FALSE;
}

/****************************************************************************/

static BOOL HandleDrawer(STRPTR name, struct WorkbenchBase *WorkbenchBase)
{
    /*
        Since it's a directory or volume, tell the Workbench
        Application to open the corresponding drawer.
    */


    BOOL success                      = FALSE;
    struct WBCommandMessage *wbcm     = NULL;
    struct WBHandlerMessage *wbhm     = NULL;
    CONST_STRPTR             namecopy = NULL;
    BPTR                     lock     = NULL;

    lock = Lock(name, ACCESS_READ);
    if (lock == NULL)
    {
        /* Inform user if the path the icon points to does not exist. Can happen with
           .icon files left after deleting the directory itself. */
        DisplayBeep(NULL);
        return FALSE;
    }
    UnLock(lock);

    if
    (
           (wbcm     = CreateWBCM(WBCM_TYPE_RELAY)) != NULL
        && (wbhm     = CreateWBHM(WBHM_TYPE_OPEN))  != NULL
        && (namecopy = StrDup(name))                != NULL

    )
    {
        wbhm->wbhm_Data.Open.Name     = namecopy;
        wbcm->wbcm_Data.Relay.Message = wbhm;

        PutMsg(&(WorkbenchBase->wb_HandlerPort), (struct Message *) wbcm);
        success = TRUE;
    }
    else
    {
        FreeVec((STRPTR)namecopy);
        DestroyWBHM(wbhm);
        DestroyWBCM(wbcm);
    }
    return success;
}

/****************************************************************************/

static BOOL HandleTool
(
    STRPTR name, LONG isDefaultIcon, struct DiskObject *icon,
    struct TagItem *tags, struct WorkbenchBase *WorkbenchBase
)
{
    /*
        It's an executable. Before I launch it, I must check
        whether it is a Workbench program or a CLI program.
    */

    BOOL success = FALSE;


    if
    (
           !isDefaultIcon
        && FindToolType(icon->do_ToolTypes, "CLI") == NULL
    )
    {
        /* It's a Workbench program */
        BPTR lock = Lock(name, ACCESS_READ);


        if (lock != NULL)
        {
            BPTR parent = ParentDir(lock);

            if (parent != NULL)
            {
                IPTR stacksize = icon->do_StackSize;

                if (stacksize < WorkbenchBase->wb_DefaultStackSize)
                    stacksize = WorkbenchBase->wb_DefaultStackSize;

                /* check for TOOLPRI */
                LONG priority = 0;
                STRPTR prio_tt = FindToolType(icon->do_ToolTypes, "TOOLPRI");
                if (prio_tt)
                {
                    StrToLong(prio_tt, &priority);
                    if (priority < -128)
                        priority = -128;
                    if (priority > 127)
                        priority = 127;
                }
                

                struct TagItem wbp_Tags[] =
                {
                    { NP_StackSize,   stacksize },
                    { NP_Priority,    priority  },
                    { TAG_MORE, (IPTR)tags      },
                    { TAG_DONE,       0         }
                };

                if (tags == NULL)
                    wbp_Tags[2].ti_Tag = TAG_IGNORE;

                success = WB_LaunchProgram
                (
                    parent, FilePart(name), wbp_Tags, WorkbenchBase
                );

                if (!success)
                {
                    /*
                        Fallback to launching it as a CLI program.
                        Most likely it will also fail, but we
                        might get lucky.
                    */
                    success = CLI_LaunchProgram(name, wbp_Tags, WorkbenchBase);
                }

                UnLock(parent);
            }

            UnLock(lock);
        }
    }
    else
    {
        /* It's a CLI program */


        success = CLI_LaunchProgram(name, tags, WorkbenchBase);
    }

    return success;
}

/****************************************************************************/

static BOOL HandleProject
(
    STRPTR name, LONG isDefaultIcon, struct DiskObject *icon,
    struct TagItem *tags, struct WorkbenchBase *WorkbenchBase
)
{
    /* It's a project; try to launch it via its default tool. */

    BOOL success = FALSE;


    if
    (
           icon->do_DefaultTool != NULL
        && strlen(icon->do_DefaultTool) > 0
    )
    {
        BPTR lock = NULL, parent = NULL;

        lock = Lock(name, ACCESS_READ);
        if (lock != NULL)
            parent = ParentDir(lock);
        else
        /*
         *  A project icon (without a project file) is actually enough for
         *  original Workbench (i.e. to launch the supplied tool)
         */
        {
            STRPTR iconname;
            if ((iconname = AllocVec(strlen(name) + 6, MEMF_ANY)) == NULL)
                return success;
            iconname[0] = '\0';
            strcat(iconname, name);
            strcat(iconname, ".info");
            if ((lock = Lock(iconname, ACCESS_READ)) != NULL)
                parent = ParentDir(lock);
            FreeVec(iconname);
        }
        if (parent != NULL)
        {
            IPTR stacksize = icon->do_StackSize;

            if (stacksize < WorkbenchBase->wb_DefaultStackSize)
                stacksize = WorkbenchBase->wb_DefaultStackSize;

            /* check for TOOLPRI */
            LONG priority = 0;
            STRPTR prio_tt = FindToolType(icon->do_ToolTypes, "TOOLPRI");
            if (prio_tt)
            {
                StrToLong(prio_tt, &priority);
                if (priority < -128)
                    priority = -128;
                if (priority > 127)
                    priority = 127;
            }
            

            struct TagItem deftool_tags[] =
            {
                { NP_StackSize    ,        stacksize      },
                { NP_Priority     ,        priority       },
                { WBOPENA_ArgLock , (IPTR) parent         },
                { WBOPENA_ArgName , (IPTR) FilePart(name) },
                { TAG_MORE        , (IPTR) tags           },
                { TAG_DONE        ,        0              }
            };

            if (tags == NULL)
                deftool_tags[4].ti_Tag = TAG_IGNORE;
            
            if (FindToolType(icon->do_ToolTypes, "CLI") == NULL)
            {
                BPTR deftool_lock = NULL, deftool_parent = NULL, old_parent = NULL;
                BOOL found = FALSE;

                /* try current dir / absolute path first */
                deftool_lock = Lock(icon->do_DefaultTool, ACCESS_READ);
                if (deftool_lock != NULL)
                    deftool_parent = ParentDir(deftool_lock);

                if (deftool_parent != NULL)
                {
                    found = TRUE;
                    success = WB_LaunchProgram
                    (
                        deftool_parent, FilePart(icon->do_DefaultTool), deftool_tags, WorkbenchBase
                    );
                }
                UnLock(deftool_parent);
                UnLock(deftool_lock);

                if (!found && IsPlainName(icon->do_DefaultTool)) /* relative path */
                {
                    struct CommandLineInterface *cli = Cli();
                    if (cli != NULL)
                    {
                        BPTR *paths;          /* Path list */
                        BOOL  running = TRUE;

                        /* Iterate over all paths in the path list */
                        for
                        (
                            paths = (BPTR *) BADDR(cli->cli_CommandDir);
                            running == TRUE && paths != NULL;
                            paths = (BPTR *) BADDR(paths[0]) /* next path */
                        )
                        {
                            old_parent  = CurrentDir(paths[1]);
                            deftool_parent = DupLock(paths[1]);
                            deftool_lock = Lock(icon->do_DefaultTool, SHARED_LOCK);

                            if (deftool_lock != NULL)
                            {
                                success = WB_LaunchProgram
                                (
                                    deftool_parent, icon->do_DefaultTool, deftool_tags, WorkbenchBase
                                );
 
                                running = FALSE;

                                UnLock(deftool_lock);
                            }
                            UnLock(deftool_parent);
                            CurrentDir(old_parent);
                        }
                    }
                }
            }
            else
            {
                success = CLI_LaunchProgram
                (
                    icon->do_DefaultTool, deftool_tags, WorkbenchBase
                );
            }

            UnLock(parent);
            UnLock(lock);
        }
    }

    return success;
}
