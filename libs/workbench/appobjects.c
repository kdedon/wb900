/* SPDX-License-Identifier: MIT */
/*
 * The Workbench application's side of AppIcons and AppMenuItems: it
 * lists them, and sends their AppMessages.  An AppIcon is known by its
 * DiskObject, as GetNextAppIcon() gives it; a removed one gets nothing.
 */
#include "workbench_intern.h"
#include "support_messages.h"
#include <proto/workbench.h>

/*
 * SendAppIconMessage(diskobj, numfiles, files, class, mousex, mousey,
 *     seconds, micros)
 * An AppMessage of type AMTYPE_APPICON to the AppIcon of diskobj: files
 * dropped on it, or none for a double click.
 */
AROS_LH8(BOOL, SendAppIconMessage,
    AROS_LHA(struct DiskObject *, diskobj, A0),
    AROS_LHA(ULONG, numfiles, D0),
    AROS_LHA(char **, files, A1),
    AROS_LHA(UWORD, class, D1),
    AROS_LHA(WORD, mousex, D2),
    AROS_LHA(WORD, mousey, D3),
    AROS_LHA(ULONG, seconds, D4),
    AROS_LHA(ULONG, micros, D5),
    struct WorkbenchBase *, WorkbenchBase, 28, Workbench)
{
    struct AppIcon *ai;
    BOOL success = FALSE;

    LockWorkbenchShared();
    for (ai = (struct AppIcon *)WorkbenchBase->wb_AppIcons.lh_Head;
         ai->ai_Node.mln_Succ != NULL;
         ai = (struct AppIcon *)ai->ai_Node.mln_Succ)
        if (ai->ai_DiskObject == diskobj) {
            success = SendAppMessage(ai->ai_MsgPort, AMTYPE_APPICON,
                ai->ai_ID, ai->ai_UserData, numfiles, files, class,
                mousex, mousey, seconds, micros);
            break;
        }
    UnlockWorkbench();
    return success;
}

/*
 * GetNextAppMenuItem(last, text, key)
 * The AppMenuItem after last, or the first for NULL; NULL after the
 * last.  Its text goes to text, at most APPMENUTEXT - 1 characters, and
 * its command key to key, "" for none.
 */
AROS_LH3(struct AppMenuItem *, GetNextAppMenuItem,
    AROS_LHA(struct AppMenuItem *, last, A0),
    AROS_LHA(char *, text, A1),
    AROS_LHA(char *, key, A2),
    struct WorkbenchBase *, WorkbenchBase, 29, Workbench)
{
    struct List *l = &WorkbenchBase->wb_AppMenuItems;
    struct AppMenuItem *ami;
    WORD n;

    LockWorkbenchShared();
    ami = (struct AppMenuItem *)l->lh_Head;
    if (last != NULL) {
        for (; ami->aw_Node.mln_Succ != NULL;
             ami = (struct AppMenuItem *)ami->aw_Node.mln_Succ)
            if (ami == last)
                break;
        ami = (struct AppMenuItem *)ami->aw_Node.mln_Succ;
    }
    if (ami == NULL || ami->aw_Node.mln_Succ == NULL) {
        UnlockWorkbench();
        return NULL;
    }
    for (n = 0; n < APPMENUTEXT - 1 && ami->ami_Text[n] != 0; n++)
        text[n] = ami->ami_Text[n];
    text[n] = 0;
    key[0] = ami->ami_CommandKey != NULL ? ami->ami_CommandKey[0] : 0;
    key[1] = 0;
    UnlockWorkbench();
    return ami;
}

/*
 * SendAppMenuItemMessage(item, numfiles, files, seconds, micros)
 * An AppMessage of type AMTYPE_APPMENUITEM for a pick of item, with the
 * selected icons.
 */
AROS_LH5(BOOL, SendAppMenuItemMessage,
    AROS_LHA(struct AppMenuItem *, item, A0),
    AROS_LHA(ULONG, numfiles, D0),
    AROS_LHA(char **, files, A1),
    AROS_LHA(ULONG, seconds, D1),
    AROS_LHA(ULONG, micros, D2),
    struct WorkbenchBase *, WorkbenchBase, 30, Workbench)
{
    struct AppMenuItem *ami;
    BOOL success = FALSE;

    LockWorkbenchShared();
    for (ami = (struct AppMenuItem *)WorkbenchBase->wb_AppMenuItems.lh_Head;
         ami->aw_Node.mln_Succ != NULL;
         ami = (struct AppMenuItem *)ami->aw_Node.mln_Succ)
        if (ami == item) {
            success = SendAppMessage(ami->ami_MsgPort, AMTYPE_APPMENUITEM,
                ami->ami_ID, ami->ami_UserData, numfiles, files, 0, 0, 0,
                seconds, micros);
            break;
        }
    UnlockWorkbench();
    return success;
}
