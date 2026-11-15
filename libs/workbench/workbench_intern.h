#ifndef __WORKBENCH_INTERN_H__
#define __WORKBENCH_INTERN_H__

/*
    Copyright  1995-2012, The AROS Development Team. All rights reserved.

    Internal header file for workbench.library.
*/

#include <exec/types.h>
#include <exec/nodes.h>
#include <exec/lists.h>
#include <exec/semaphores.h>
#include <exec/memory.h>
#include <exec/libraries.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <utility/utility.h>
#include <intuition/intuition.h>

#include <workbench/icon.h>
#include <workbench/workbench.h>
#include <workbench/startup.h>
#include <workbench/handler.h>

#include <aros/libcall.h>
#include <proto/intuition.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/icon.h>
#include <proto/utility.h>

/* The C library functions workbench.library uses (crt.c) */
ULONG strlen(CONST_STRPTR s);
char *strcpy(char *d, CONST_STRPTR s);
char *strcat(char *d, CONST_STRPTR s);

/*
    This is the WorkbenchBase structure. It is defined here because it is 
    completely private. Applications should treat it as a struct Library, and
    use the workbench.library functions to get information.
*/

struct WorkbenchBase
{
    struct Library          LibNode;

    struct MsgPort          wb_HandlerPort;            /* The handler's message port */
    struct MsgPort         *wb_WorkbenchPort;          /* The workbench application's message port */
    struct SignalSemaphore  wb_WorkbenchPortSemaphore; /* Arbitrates initializetion access to the port above */
    
    struct List             wb_AppWindows;
    struct List             wb_AppIcons;
    struct List             wb_AppMenuItems;

    BPTR                    wb_SearchPath;
    ULONG                   wb_DefaultStackSize;

    struct SignalSemaphore  wb_BaseSemaphore;           /* Arbitrates library base access */
    APTR                    wb_SegList;

    struct Library         *wb_UtilityBase;
    struct Library         *wb_DOSBase;
    struct Library         *wb_IntuitionBase;
    struct Library         *wb_IconBase;
};

#define LB(lb)          ((struct WorkbenchBase *) (lb))

#define LockWorkbench()   ObtainSemaphore(&(LB(WorkbenchBase)->wb_BaseSemaphore))
#define LockWorkbenchShared()   ObtainSemaphoreShared(&(LB(WorkbenchBase)->wb_BaseSemaphore))
#define UnlockWorkbench() ReleaseSemaphore(&(LB(WorkbenchBase)->wb_BaseSemaphore))

#define UtilityBase	(LB(WorkbenchBase)->wb_UtilityBase)
#define DOSBase		((struct DosLibrary *)LB(WorkbenchBase)->wb_DOSBase)
#define IntuitionBase	((struct IntuitionBase *)LB(WorkbenchBase)->wb_IntuitionBase)
#define IconBase	(LB(WorkbenchBase)->wb_IconBase)

/*
 * Definition of internal structures.
 */

struct AppIcon
{
    struct MinNode     ai_Node;
    UWORD              ai_AppID;
    UWORD              ai_Type;

    IPTR               ai_ID;
    IPTR               ai_UserData;

    struct MsgPort    *ai_MsgPort;

    struct DiskObject *ai_DiskObject;

    ULONG              ai_Flags;
    CONST_STRPTR       ai_Text;

    struct Hook       *ai_RenderHook;
};

/* Valid values for ai_Flags. These correspond to the tag items
 * in <workbench/workbench.h>. */

#define WBAPPICONF_SupportsOpen            (1<<1)
#define WBAPPICONF_SupportsCopy            (1<<2)
#define WBAPPICONF_SupportsRename          (1<<3)
#define WBAPPICONF_SupportsInformation     (1<<4)
#define WBAPPICONF_SupportsSnapshot        (1<<5)
#define WBAPPICONF_SupportsUnSnapshot      (1<<6)
#define WBAPPICONF_SupportsLeaveOut        (1<<7)
#define WBAPPICONF_SupportsPutAway         (1<<8)
#define WBAPPICONF_SupportsDelete          (1<<9)
#define WBAPPICONF_SupportsFormatDisk      (1<<10)
#define WBAPPICONF_SupportsEmptyTrash      (1<<11)
#define WBAPPICONF_PropagatePosition       (1<<12)
#define WBAPPICONF_NotifySelectState       (1<<13)

struct AppWindow
{
    struct MinNode  aw_Node;
    UWORD           aw_AppID;
    UWORD           aw_Type;

    IPTR            aw_ID;
    IPTR            aw_UserData;

    struct MsgPort *aw_MsgPort;

    struct Window  *aw_Window;
};

struct AppMenuItem
{
    struct MinNode  aw_Node;
    UWORD           aw_AppID;
    UWORD           aw_Type;

    IPTR            ami_ID;
    IPTR            ami_UserData;

    struct MsgPort *ami_MsgPort;

    STRPTR          ami_Text;

    STRPTR          ami_CommandKey;
};

/* Internal WBHM structure, used to hold data which the user doesn't
   have to know about.  */
struct IntWBHandlerMessage
{
    struct WBHandlerMessage iwbhm_wbhm;
    union
    {
        struct
        {
	    struct IntuiMessage *imsg;
        } Hide;
    } iwbhm_Data;
};
#endif /* __WORKBENCH_INTERN_H__ */
