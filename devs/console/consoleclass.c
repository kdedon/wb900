/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: Base class for console units
*/


#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/keymap.h>

#include <aros/asmcall.h>
#include <devices/conunit.h>
#include <intuition/classes.h>
#include <intuition/intuition.h>


#include "consoleif.h"
#include "console_gcc.h"

/*
   Base class for the classes handling standard, charmap and snipmap
   consoles. This is a whitebox base class (like GADGETCLASS), which means subclasses
   can have direct access to its instance data.
*/

struct consoledata
{
    struct intConUnit intunit;
};

#undef ConsoleDevice
#define ConsoleDevice ((struct ConsoleBase *)cl->cl_UserData)

/*********************
**  Console::New()  **
*********************/
static Object *console_new(Class *cl, Object *o, struct opSet *msg)
{
    struct Window *win;

    /* Get console window */
    win =
        (struct Window *)GetTagData(A_Console_Window, 0, msg->ops_AttrList);

    if (!win)
    {
        return NULL;
    }

    o = (Object *) Console_Super(cl, o, (Msg) msg);
    if (o)
    {
        struct ConUnit *unit;
        struct consoledata *data;
        struct RastPort *rp = win->RPort;
        WORD i;

        data = INST_DATA(cl, o);

        unit = (struct ConUnit *)data;

        memset(data, 0, sizeof(struct consoledata));
        for (i = 0; i < CONUNIT_PEN_MAX; i ++)
        {
            data->intunit.pens[i] = i;
        }

        /* Initialize the unit fields */
        unit->cu_Window = win;
        unit->cu_KeyMapStruct = *AskKeyMapDefault();

        /* For now one should use only non-proportional fonts */
        unit->cu_XRSize = rp->Font->tf_XSize;
        unit->cu_YRSize = rp->Font->tf_YSize;


        /* Use whole window for console. */
        unit->cu_XMax =
            (win->Width - (win->BorderLeft +
                win->BorderRight)) / unit->cu_XRSize - 1;
        unit->cu_YMax =
            (win->Height - (win->BorderTop +
                win->BorderBottom)) / unit->cu_YRSize - 1;

        unit->cu_XROrigin =
            win->Flags & WFLG_GIMMEZEROZERO ? 0 : win->BorderLeft;
        unit->cu_YROrigin =
            win->Flags & WFLG_GIMMEZEROZERO ? 0 : win->BorderTop;


        unit->cu_XRExtant =
            unit->cu_XROrigin + (unit->cu_XRSize * (unit->cu_XMax + 1) - 1);
        unit->cu_YRExtant =
            unit->cu_YROrigin + (unit->cu_YRSize * (unit->cu_YMax + 1) - 1);

        /* The character and cursor positions start at DEF_CHAR_XMIN and
           DEF_CHAR_YMIN, 0, which the memset() above has set */

        for (i = 0; i < MAXTABS - 1; i++)
        {
            unit->cu_TabStops[i] = i * 8;
        }
        unit->cu_TabStops[i] = (UWORD) - 1;

        NEWLIST(&ICU(o)->pasteData);

        SET_MODE(o, PMB_ASM);   /* auto-scroll-mode ON */
        SET_MODE(o, PMB_AWM);   /* auto-wrap-mode ON */

        /* RKM: Default mode is SET */
        SET_MODE(o, M_LNM);     /* linefeed mode = linefeed+carriage return */
    }

    return o;
}

/**********************
**  Console::Left()  **
**********************/
static VOID console_left(Class *cl, Object *o, struct P_Console_Move *msg)
{
    IPTR count = msg->Num;
    IPTR minx = CHAR_XMIN(o);
    IPTR maxx = CHAR_XMAX(o);
    IPTR span = maxx - minx + 1;
    IPTR offset = (IPTR) XCCP - minx;
    IPTR scrollcount = 0;
    WORD newx;


    if (CHECK_MODE(o, PMB_AWM))
    {
        if (count <= offset)
        {
            newx = (WORD) ((IPTR) XCCP - count);
        }
        else
        {
            IPTR remaining = count - offset;
            IPTR rem = remaining % span;

            scrollcount = 1 + (remaining - 1) / span;
            newx = (WORD) (minx + (rem ? span - rem : 0));
        }

        XCP = XCCP = newx;

        if (scrollcount)
            Console_Up(o, scrollcount);
    }
    else
    {
        if (count > offset)
            newx = CHAR_XMIN(o);
        else
            newx = (WORD) ((IPTR) XCCP - count);
    }

    XCP = XCCP = newx;


    return;
}

/***********************
**  Console::Right()  **
***********************/
static VOID console_right(Class *cl, Object *o, struct P_Console_Move *msg)
{
    IPTR count = msg->Num;
    IPTR minx = CHAR_XMIN(o);
    IPTR maxx = CHAR_XMAX(o);
    IPTR span = maxx - minx + 1;
    IPTR offset = (IPTR) XCCP - minx;
    IPTR to_wrap = span - offset;
    IPTR scrollcount = 0;
    WORD newx;


    if (CHECK_MODE(o, PMB_AWM))
    {
        if (count < to_wrap)
        {
            newx = (WORD) ((IPTR) XCCP + count);
        }
        else
        {
            IPTR remaining = count - to_wrap;

            scrollcount = 1 + remaining / span;
            newx = (WORD) (minx + remaining % span);
        }

        XCP = XCCP = newx;

        if (scrollcount)
            Console_Down(o, scrollcount);
    }
    else
    {
        IPTR room = maxx - (IPTR) XCCP;

        if (count > room)
            newx = CHAR_XMAX(o);
        else
            newx = (WORD) ((IPTR) XCCP + count);
    }

    XCP = XCCP = newx;


    return;
}

/********************
**  Console::Up()  **
********************/
static VOID console_up(Class *cl, Object *o, struct P_Console_Move *msg)
{
    IPTR count = msg->Num;
    IPTR distance = (IPTR) YCCP - CHAR_YMIN(o);


    if (count > distance)
    {
        IPTR scroll_param = count - distance;

        YCCP = YCP = CHAR_YMIN(o);

        if (CHECK_MODE(o, PMB_ASM) && scroll_param)
            Console_DoCommand(o, C_SCROLL_DOWN, 1, &scroll_param);
    }
    else
    {
        YCCP = (WORD) ((IPTR) YCCP - count);
    }

    YCP = YCCP;

    return;
}



/**********************
**  Console::Down()  **
**********************/
static VOID console_down(Class *cl, Object *o, struct P_Console_Move *msg)
{
    IPTR count = msg->Num;
    IPTR distance = CHAR_YMAX(o) - (IPTR) YCCP;


    if (count > distance)
    {
        IPTR scroll_param = count - distance;

        YCCP = YCP = CHAR_YMAX(o);

        if (CHECK_MODE(o, PMB_ASM) && scroll_param)
            Console_DoCommand(o, C_SCROLL_UP, 1, &scroll_param);
    }
    else
    {
        YCCP = (WORD) ((IPTR) YCCP + count);
    }

    YCP = YCCP;

    return;
}

/***************************
**  Console::DoCommand()  **
***************************/

/* The text style bits that the SGR parameters 1 to 8 set, and the ones
   that the parameters 22 to 28 clear */
static const UBYTE sgr_set[8] =
{
    CON_TXTFLAGS_BOLD, 0, CON_TXTFLAGS_ITALIC, CON_TXTFLAGS_UNDERLINED,
    0, 0, CON_TXTFLAGS_REVERSED, CON_TXTFLAGS_CONCEALED
};
static const UBYTE sgr_reset[7] =
{
    CON_TXTFLAGS_BOLD, CON_TXTFLAGS_ITALIC, CON_TXTFLAGS_UNDERLINED,
    0, 0, CON_TXTFLAGS_REVERSED, CON_TXTFLAGS_CONCEALED
};

static VOID console_docommand(Class *cl, Object *o,
    struct P_Console_DoCommand *msg)
{

    switch (msg->Command)
    {
    case C_SET_LF_MODE:
        /* LF==LF+CR */
        /* ICU(o)->conFlags |= CF_LF_MODE_ON ; */
        SET_MODE(o, M_LNM);
        break;

    case C_RESET_LF_MODE:
        /* LF==LF */
        /* ICU(o)->conFlags &= ~CF_LF_MODE_ON; */
        CLEAR_MODE(o, M_LNM);
        break;

    case C_SET_AUTOSCROLL_MODE:
        SET_MODE(o, PMB_ASM);
        break;

    case C_RESET_AUTOSCROLL_MODE:
        CLEAR_MODE(o, PMB_ASM);
        break;

    case C_SET_AUTOWRAP_MODE:
        SET_MODE(o, PMB_AWM);
        break;

    case C_RESET_AUTOWRAP_MODE:
        CLEAR_MODE(o, PMB_AWM);
        break;

    case C_SELECT_GRAPHIC_RENDITION:
        {
            UBYTE i;
            IPTR param;

            for (i = 0; i < msg->NumParams; i++)
            {
                param = msg->Params[i];

                if (param == 0)
                {
                    Console_GetColorPen(o, 0, &CU(o)->cu_BgPen);
                    Console_GetColorPen(o, 1, &CU(o)->cu_FgPen);
                    CU(o)->cu_TxFlags = 0;
                }
                else if (param <= 8)
                {
                    /* 1 bold, 3 italic, 4 underlined, 7 reversed,
                       8 concealed; 2 ("faint") is not supported */
                    CU(o)->cu_TxFlags |= sgr_set[param - 1];
                }
                else if (param >= 22 && param <= 28)
                {
                    /* 22 not bold, 23 not italic, 24 not underlined,
                       27 not reversed, 28 not concealed */
                    CU(o)->cu_TxFlags &= ~sgr_reset[param - 22];
                }
                else if (param >= 30 && param <= 49)
                {
                    /* 30-37 foreground colour, 39 default foreground,
                       40-47 background colour, 49 default background;
                       38 and 48 are not supported */
                    BOOL bg = param >= 40;
                    UWORD color = (UWORD)param - (bg ? 40 : 30);

                    if (color == 9)
                        color = bg ? 0 : 1;
                    if (color != 8)
                        Console_GetColorPen(o, color,
                            bg ? &CU(o)->cu_BgPen : &CU(o)->cu_FgPen);
                }
            } /* for(i = 0; i < msg->NumParams; i++) */
        }
        break;

    case C_SET_RAWEVENTS:
    case C_RESET_RAWEVENTS:
        {
            UBYTE i;
            IPTR param;

            for (i = 0; i < msg->NumParams; i++)
            {
                param = msg->Params[i];

                if (param <= IECLASS_MAX)
                {
                    if (msg->Command == C_SET_RAWEVENTS)
                        SET_RAWEVENT(o, param);
                    else
                        RESET_RAWEVENT(o, param);
                }

            } /* for(i = 0; i < msg->NumParams; i++) */
        }
        break;
    } /* switch (msg->Command) */

    return;
}

/**********************************
**  Console::GetDefaultParams()  **
**********************************/
static VOID console_getdefaultparams(Class *cl, Object *o,
    struct P_Console_GetDefaultParams *msg)
{
    switch (msg->Command)
    {
    case C_INSERT_CHAR:
    case C_CURSOR_UP:
    case C_CURSOR_DOWN:
    case C_CURSOR_FORWARD:
    case C_CURSOR_BACKWARD:
    case C_CURSOR_NEXT_LINE:
    case C_CURSOR_PREV_LINE:
    case C_INSERT_LINE:
    case C_DELETE_LINE:
    case C_SET_TOP_OFFSET:
    case C_SET_PAGE_LENGTH:
/* FIXME: Autodocs state commands in between here, has params RKRM: Devs saye the do not */
    case C_CURSOR_HTAB:
    case C_DELETE_CHAR:
    case C_SCROLL_UP:
    case C_SCROLL_DOWN:
    case C_CURSOR_BACKTAB:
        msg->Params[0] = 1;
        break;
    case C_CURSOR_POS:
        msg->Params[0] = YCCP + 1;
        msg->Params[1] = XCCP + 1;
        break;

    case C_CURSOR_TAB_CTRL:
        msg->Params[0] = 0;     /* set tab */
        break;

    case C_SELECT_GRAPHIC_RENDITION:
        /* don't do anything, as params may be in any order */
        break;

    case C_SET_RAWEVENTS:
        /* don't do anything, as params may be in any order */
        break;

    case C_RESET_RAWEVENTS:
        /* don't do anything, as params may be in any order */
        break;
    } /* switch (msg->Command) */

    return;
}

/*******************************
**  Console::NewWindowSize()  **
*******************************/
static VOID console_newwindowsize(Class *cl, Object *o,
    Msg msg)
{
    struct ConUnit *unit;
    struct consoledata *data;
    struct Window *win;

    data = INST_DATA(cl, o);
    unit = (struct ConUnit *)data;

    win = unit->cu_Window;

    if (!(data->intunit.conFlags & CF_MANUAL_LINE_LENGTH))
    {
        unit->cu_XMax =
            (win->Width - win->BorderRight -
            unit->cu_XROrigin) / unit->cu_XRSize - 1;
    }
    if (!(data->intunit.conFlags & CF_MANUAL_PAGE_LENGTH))
    {
        unit->cu_YMax =
            (win->Height - win->BorderBottom -
            unit->cu_YROrigin) / unit->cu_YRSize - 1;
    }

    unit->cu_XRExtant =
        unit->cu_XROrigin + (unit->cu_XRSize * (unit->cu_XMax + 1) - 1);
    unit->cu_YRExtant =
        unit->cu_YROrigin + (unit->cu_YRSize * (unit->cu_YMax + 1) - 1);

    if (unit->cu_XCCP > unit->cu_XMax)
        unit->cu_XCCP = unit->cu_XMax;
    if (unit->cu_YCCP > unit->cu_YMax)
        unit->cu_YCCP = unit->cu_YMax;

    unit->cu_XCP = unit->cu_XCCP;
    unit->cu_YCP = unit->cu_YCCP;

    return;
}

static IPTR console_getpencolor(Class *cl, Object *o, struct P_Console_GetColorPen *msg)
{
    struct consoledata *data = INST_DATA(cl, o);
    *msg->PenPtr = (UBYTE)data->intunit.pens[msg->ColorIdx];
    return TRUE;
}

/********* Console class dispatcher **********************************/
AROS_UFH3S(IPTR, dispatch_consoleclass,
    AROS_UFHA(Class *, cl, A0),
    AROS_UFHA(Object *, o, A2), AROS_UFHA(Msg, msg, A1))
{
    AROS_USERFUNC_INIT

    IPTR retval = 0UL;

    switch (msg->MethodID)
    {
    case M_Console_GetColorPen:
        retval = (IPTR) console_getpencolor(cl, o, (struct P_Console_GetColorPen *)msg);
        break;

    case OM_NEW:
        retval = (IPTR) console_new(cl, o, (struct opSet *)msg);
        break;

    case M_Console_Left:
        console_left(cl, o, (struct P_Console_Move *)msg);
        break;

    case M_Console_Right:
        console_right(cl, o, (struct P_Console_Move *)msg);
        break;

    case M_Console_Up:
        console_up(cl, o, (struct P_Console_Move *)msg);
        break;

    case M_Console_Down:
        console_down(cl, o, (struct P_Console_Move *)msg);
        break;

    case M_Console_DoCommand:
        console_docommand(cl, o, (struct P_Console_DoCommand *)msg);
        break;

    case M_Console_GetDefaultParams:
        console_getdefaultparams(cl, o,
            (struct P_Console_GetDefaultParams *)msg);
        break;

    case M_Console_NewWindowSize:
        console_newwindowsize(cl, o, msg);
        break;

    default:
        retval = Console_Super(cl, o, msg);
        break;
    }
    return (retval);

    AROS_USERFUNC_EXIT
}



#undef ConsoleDevice

/* The method stubs of consoleif.h */

IPTR Console_DoCommand(Object *o, BYTE cmd, UBYTE numparams, APTR params)
{
    struct P_Console_DoCommand p;

    p.MethodID = M_Console_DoCommand;
    p.Command = cmd;
    p.NumParams = numparams;
    p.Params = (IPTR *)params;
    return DoMethodA(o, (Msg)&p);
}

IPTR Console_Move(Object *o, ULONG mid, IPTR num)
{
    struct P_Console_Move p;

    p.MethodID = mid;
    p.Num = num;
    return DoMethodA(o, (Msg)&p);
}

IPTR Console_GetDefaultParams(Object *o, BYTE cmd, IPTR *params)
{
    struct P_Console_GetDefaultParams p;

    p.MethodID = M_Console_GetDefaultParams;
    p.Command = cmd;
    p.Params = params;
    return DoMethodA(o, (Msg)&p);
}

IPTR Console_GetColorPen(Object *o, UWORD coloridx, UBYTE *penptr)
{
    struct P_Console_GetColorPen p;

    p.MethodID = M_Console_GetColorPen;
    p.ColorIdx = coloridx;
    p.PenPtr = penptr;
    return DoMethodA(o, (Msg)&p);
}

IPTR Console_HandleGadgets(Object *o, struct InputEvent *e)
{
    struct P_Console_HandleGadgets p;

    p.MethodID = M_Console_HandleGadgets;
    p.Event = e;
    return DoMethodA(o, (Msg)&p);
}

IPTR Console_Method(Object *o, ULONG mid)
{
    return DoMethodA(o, (Msg)&mid);
}

Class *makeConsoleClass(struct ConsoleBase *ConsoleDevice)
{
    Class *cl;

    cl = MakeClass(NULL, ROOTCLASS, NULL, sizeof(struct consoledata), 0UL);
    if (cl)
    {
        cl->cl_Dispatcher.h_Entry = (ULONG (*)()) dispatch_consoleclass;
        cl->cl_Dispatcher.h_SubEntry = NULL;

        cl->cl_UserData = (IPTR) ConsoleDevice;

        return cl;
    }
    return NULL;
}
