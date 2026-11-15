#ifndef CONSOLEIF_H
#define CONSOLEIF_H
/*
    Copyright © 1995-2026, The AROS Development Team. All rights reserved.
    $Id$

    Desc: Include for the console class
    Lang: english
*/

#ifndef UTILITY_TAGITEM_H
#   include <utility/tagitem.h>
#endif
#ifndef PROTO_INTUITION_H
#   include <proto/intuition.h>
#endif
#ifndef PROTO_ALIB_H
#   include <proto/alib.h>
#endif
#ifndef DEVICES_INPUTEVENT_H
#   include <devices/inputevent.h>
#endif

#define A_Console_Window (TAG_USER + 1)

#define M_Console_Write             (TAG_USER + 1)
#define M_Console_DoCommand         (TAG_USER + 3)
#define M_Console_Left              (TAG_USER + 4)
#define M_Console_Right             (TAG_USER + 5)
#define M_Console_Up                (TAG_USER + 6)
#define M_Console_Down              (TAG_USER + 7)
#define M_Console_GetDefaultParams  (TAG_USER + 8)
#define M_Console_RenderCursor      (TAG_USER + 9)
#define M_Console_UnRenderCursor    (TAG_USER + 10)
#define M_Console_NewWindowSize     (TAG_USER + 12)
#define M_Console_HandleGadgets     (TAG_USER + 13)
#define M_Console_Copy              (TAG_USER + 14)
#define M_Console_Paste             (TAG_USER + 15)
#define M_Console_GetColorPen       (TAG_USER + 20)

struct P_Console_DoCommand
{
    ULONG MethodID;
    BYTE Command;               /* Erase in display, scroll, next line etc.. */
    UBYTE NumParams;
    IPTR *Params;               /* The command's parameters */
};

/* M_Console_Left, M_Console_Right, M_Console_Up and M_Console_Down */
struct P_Console_Move
{
    ULONG MethodID;
    IPTR Num;
};

struct P_Console_HandleGadgets
{
    ULONG MethodID;
    struct InputEvent *Event;
};

struct P_Console_GetDefaultParams
{
    ULONG MethodID;
    BYTE Command;
    IPTR *Params;
};

struct P_Console_GetColorPen
{
    ULONG MethodID;
    UWORD ColorIdx;
    UBYTE *PenPtr;
};

/*
 * Method stubs (consoleclass.c). Each builds the message for one method of
 * the console classes and sends it to the object; the result is that of
 * the method.
 */

#define Console_Super(cl, o, msg) DoSuperMethodA((cl), (o), (msg))

IPTR Console_DoCommand(Object *o, BYTE cmd, UBYTE numparams, APTR params);
IPTR Console_Move(Object *o, ULONG mid, IPTR num);
IPTR Console_GetDefaultParams(Object *o, BYTE cmd, IPTR *params);
IPTR Console_GetColorPen(Object *o, UWORD coloridx, UBYTE *penptr);
IPTR Console_HandleGadgets(Object *o, struct InputEvent *e);
/* Methods that take no arguments: M_Console_RenderCursor, ... */
IPTR Console_Method(Object *o, ULONG mid);

#define Console_Left(o, num)    Console_Move((o), M_Console_Left, (num))
#define Console_Right(o, num)   Console_Move((o), M_Console_Right, (num))
#define Console_Up(o, num)      Console_Move((o), M_Console_Up, (num))
#define Console_Down(o, num)    Console_Move((o), M_Console_Down, (num))

#define Console_RenderCursor(o)   Console_Method((o), M_Console_RenderCursor)
#define Console_UnRenderCursor(o) Console_Method((o), M_Console_UnRenderCursor)
#define Console_NewWindowSize(o)  Console_Method((o), M_Console_NewWindowSize)
#define Console_Copy(o)           Console_Method((o), M_Console_Copy)
#define Console_Paste(o)          Console_Method((o), M_Console_Paste)

#endif /* CONSOLEIF_H */
