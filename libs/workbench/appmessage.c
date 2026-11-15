/* SPDX-License-Identifier: MIT */
/*
 * AppMessages: the message, its WBArgs and their names in one block.  The
 * handler frees it when the application replies, so senders never wait.
 */
#include "workbench_intern.h"
#include "support_messages.h"

BOOL __SendAppMessage_WB(struct MsgPort *port, UWORD type, IPTR id,
    IPTR userdata, ULONG numfiles, char **files, UWORD class, WORD mousex,
    WORD mousey, ULONG seconds, ULONG micros,
    struct WorkbenchBase *WorkbenchBase)
{
    struct AppMessage *am;
    struct WBArg *wa;
    char *name, *file, *dir;
    ULONG size, i, n;

    size = sizeof(*am) + numfiles * sizeof(*wa);
    for (i = 0; i < numfiles; i++)
        size += strlen(FilePart(files[i])) + 1;
    if ((am = AllocVec(size, MEMF_PUBLIC | MEMF_CLEAR)) == NULL)
        return FALSE;
    wa = (struct WBArg *)(am + 1);
    name = (char *)(wa + numfiles);
    am->am_NumArgs = numfiles;
    am->am_ArgList = numfiles ? wa : NULL;
    for (i = 0; i < numfiles; i++) {
        file = FilePart(files[i]);
        wa[i].wa_Name = strcpy(name, file);
        name += strlen(name) + 1;
        /* "Vol:dir/file" is "file" in Vol:dir, "Vol:" is "" in Vol: */
        n = PathPart(files[i]) - files[i];
        if ((dir = AllocVec(n + 1, MEMF_CLEAR)) != NULL) {
            CopyMem(files[i], dir, n);
            wa[i].wa_Lock = Lock(dir, SHARED_LOCK);
            FreeVec(dir);
        }
        if (wa[i].wa_Lock == NULL) {
            FreeAppMessage(am);
            return FALSE;
        }
    }
    am->am_Message.mn_Node.ln_Type = NT_MESSAGE;
    am->am_Message.mn_Length = sizeof(*am);
    am->am_Message.mn_ReplyPort = &WorkbenchBase->wb_AppReplyPort;
    am->am_Type = type;
    am->am_ID = id;
    am->am_UserData = userdata;
    am->am_Version = AM_VERSION;
    am->am_Class = class;
    am->am_MouseX = mousex;
    am->am_MouseY = mousey;
    am->am_Seconds = seconds;
    am->am_Micros = micros;
    PutMsg(port, (struct Message *)am);
    return TRUE;
}

VOID __FreeAppMessage_WB(struct AppMessage *am,
    struct WorkbenchBase *WorkbenchBase)
{
    LONG i;

    for (i = 0; i < am->am_NumArgs; i++)
        UnLock(am->am_ArgList[i].wa_Lock);
    FreeVec(am);
}
