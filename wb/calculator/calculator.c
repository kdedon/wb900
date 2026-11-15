/*
    Copyright (C) 1995-2025, The AROS Development Team. All rights reserved.

    Desc: A four-function calculator with memory, on gadtools.
    Command line options:

    1. PUBSCREEN <name>: the name of the public screen to open the window on
    2. TAPE <filename>: the name of a file to record the user interactions
       into
*/
#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <intuition/intuition.h>
#include <libraries/gadtools.h>
#include <workbench/startup.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/gadtools.h>

#include "calc.h"

const char *version = "$VER: Calculator 1.5 (15.11.2026) \xA9 1995-2026 The AROS Dev Team";

#define ARG_TEMPLATE "PUBSCREEN,TAPE/K"
enum {ARG_PUBSCREEN,ARG_TAPE,NUM_ARGS};

enum
{
    BTYPE_0 = 0,
    BTYPE_1,
    BTYPE_2,
    BTYPE_3,
    BTYPE_4,
    BTYPE_5,
    BTYPE_6,
    BTYPE_7,
    BTYPE_8,
    BTYPE_9,
    BTYPE_COMMA,
    BTYPE_BS,
    BTYPE_CA,
    BTYPE_CE,
    BTYPE_MUL,
    BTYPE_DIV,
    BTYPE_SUB,
    BTYPE_ADD,
    BTYPE_SIGN,
    BTYPE_EQU,
    BTYPE_MC,
    BTYPE_MR,
    BTYPE_MSUB,
    BTYPE_MADD,
    BTYPE_NONE
};

#define NUM_BUTTONS 24
#define COLS 5
#define MEM_BUTTONS 4

/* The display's gadget ID; the buttons' are their index */
#define DISPLAY_ID 100

struct CalcButtonInfo
{
    const char *label;
    UBYTE btype;
    char shortcut;
};

/* The memory row, then the keypad */
static struct CalcButtonInfo BUTTONS[] =
{
    {"MC", BTYPE_MC, 'C'}, {"MR", BTYPE_MR, 'R'}, {"M-", BTYPE_MSUB, 'N'}, {"M+", BTYPE_MADD, 'M'},
    {"7", BTYPE_7, '7'}, {"8", BTYPE_8, '8'}, {"9", BTYPE_9, '9'}, {"CA", BTYPE_CA, 'A'}, {"CE", BTYPE_CE, 'E'},
    {"4", BTYPE_4, '4'}, {"5", BTYPE_5, '5'}, {"6", BTYPE_6, '6'}, {"*", BTYPE_MUL, '*'}, {"/", BTYPE_DIV, '/'},
    {"1", BTYPE_1, '1'}, {"2", BTYPE_2, '2'}, {"3", BTYPE_3, '3'}, {"+", BTYPE_ADD, '+'}, {"-", BTYPE_SUB, '-'},
    {"0", BTYPE_0, '0'}, {".", BTYPE_COMMA, '.'}, {"<<", BTYPE_BS, 8}, {"+/-", BTYPE_SIGN, 'S'}, {"=", BTYPE_EQU, '='}
};

struct DosLibrary *DOSBase;
struct IntuitionBase *IntuitionBase;
struct Library *GadToolsBase;

static char pubscrname[256];
static BPTR tapefh;

/**********************************************************************
 Tape
 **********************************************************************/

static void tape_puts(const char *s)
{
    const char *p;

    for (p = s; *p; p++)
        ;
    Write(tapefh, (APTR)s, p - s);
}

static void tape_line(char lead, const char *str)
{
    char c[2];

    if (!tapefh)
        return;
    c[0] = lead;
    c[1] = 0;
    if (lead)
        tape_puts(c);
    tape_puts("\t");
    tape_puts(str);
    tape_puts("\n");
}

/**********************************************************************
 Calculator
 **********************************************************************/

#define MAX_DIGITS 13

#define EDIT_BUFFER_SIZE MAX_DIGITS + 2  /* space for '-' sign and terminator byte */
enum { STATE_LEFTVAL, STATE_OP, STATE_RIGHTVAL, STATE_EQU, STATE_ERROR };

struct CalculatorData
{
    char edit_buffer[EDIT_BUFFER_SIZE];
    int num_digits;
    int state;
    BOOL fresh;     /* the next digit starts a new number */

    struct Num lvalue, rvalue, memory;
    int op;
};

static struct CalculatorData calc;

/* The display: the memory mark, then the value right-aligned, filling
 * the gadget */
#define DISPLAY_MAX 40
static char display_buffer[DISPLAY_MAX + 1];
static WORD display_len;

static struct Window *win;
static struct Gadget *display;

static char op2char(int op)
{
    switch (op) {
        case BTYPE_MUL: return '*';
        case BTYPE_DIV: return ':';
        case BTYPE_SUB: return '-';
        case BTYPE_ADD: return '+';
        default: return '?';
    }
}

static void clear_edit_buffer(struct CalculatorData *data)
{
    WORD i;

    for (i = 0; i < EDIT_BUFFER_SIZE; i++)
        data->edit_buffer[i] = 0;
    data->num_digits = 0;
}

static BOOL is_operator(int btype)
{
    return btype >= BTYPE_MUL && btype <= BTYPE_ADD;
}

static BOOL can_insert_comma(struct CalculatorData *data)
{
    int i;

    for (i = 0; i < data->num_digits; i++)
    {
        if (data->edit_buffer[i] == '.') return FALSE;
    }
    return TRUE;
}

static BOOL eval_result(struct Num *lvalue, struct Num *rvalue, int op)
{
    switch (op)
    {
        case BTYPE_MUL: return num_mul(lvalue, rvalue, lvalue);
        case BTYPE_DIV: return num_div(lvalue, rvalue, lvalue);
        case BTYPE_SUB: return num_sub(lvalue, rvalue, lvalue);
        case BTYPE_ADD: return num_add(lvalue, rvalue, lvalue);
        default: *lvalue = *rvalue; return TRUE;
    }
}

static BOOL is_zero(struct Num *n)
{
    WORD i;

    for (i = 0; i < ND; i++)
        if (n->d[i])
            return FALSE;
    return TRUE;
}

static void display_state(struct CalculatorData *data)
{
    struct TagItem t[2];
    char *s;
    WORD i, n;

    for (i = 0; i < display_len; i++)
        display_buffer[i] = ' ';
    display_buffer[display_len] = 0;
    if (!is_zero(&data->memory))
        display_buffer[0] = 'M';
    if (data->state == STATE_ERROR)
        s = "Error";
    else if (data->num_digits == 0)
        s = "0";
    else
        s = data->edit_buffer;
    for (n = 0; s[n]; n++)
        ;
    for (i = 0; i < n; i++)
        display_buffer[display_len - n + i] = s[i];

    t[0].ti_Tag = GTTX_Text;  t[0].ti_Data = (IPTR)display_buffer;
    t[1].ti_Tag = TAG_DONE;
    GT_SetGadgetAttrsA(display, win, NULL, t);
}

/* A result into the edit buffer, or the error state */
static void show_value(struct CalculatorData *data, struct Num *n, BOOL ok)
{
    clear_edit_buffer(data);
    if (ok && num_format(n, data->edit_buffer, MAX_DIGITS))
    {
        for (data->num_digits = 0; data->edit_buffer[data->num_digits]; data->num_digits++)
            ;
    }
    else
    {
        data->state = STATE_ERROR;
        tape_line(0, "Error");
    }
}

static void toggle_sign(struct CalculatorData *data)
{
    int i;

    if (data->num_digits > 0 && data->edit_buffer[0] == '-')
    {
        /* eliminate the sign by shifting to the left */
        data->num_digits--;
        for (i = 0; i < data->num_digits; i++)
            data->edit_buffer[i] = data->edit_buffer[i + 1];
        data->edit_buffer[data->num_digits] = 0;
    }
    else
    {
        /* add sign by shifting to the right and inserting - */
        for (i = data->num_digits; i > 0; i--)
            data->edit_buffer[i] = data->edit_buffer[i - 1];
        data->num_digits++;
        data->edit_buffer[0] = '-';
        data->edit_buffer[data->num_digits] = 0;
    }
    if (data->state == STATE_EQU)
        num_parse(&data->lvalue, data->edit_buffer);
}

/* A key that starts a new number after an operator or a result */
static void start_entry(struct CalculatorData *data)
{
    if (data->fresh)
        clear_edit_buffer(data);
    data->fresh = FALSE;
    if (data->state == STATE_OP)
    {
        data->state = STATE_RIGHTVAL;
        clear_edit_buffer(data);
    }
    if (data->state == STATE_EQU)
    {
        data->state = STATE_LEFTVAL;
        clear_edit_buffer(data);
    }
}

/* The edit buffer's digits, without the sign */
static int entry_length(struct CalculatorData *data)
{
    return data->num_digits - (data->num_digits > 0 && data->edit_buffer[0] == '-');
}

/*
 * The calculator's state machine.  An operator after a second operand
 * applies the pending one first, so 2 + 3 * 4 = is 20, and a second
 * operator replaces the first.
 */
static void add_key(struct CalculatorData *data, int btype)
{
    struct Num n;

    if (data->state == STATE_ERROR && btype != BTYPE_CA && btype != BTYPE_CE)
        return;

    if (btype <= BTYPE_9)
    {
        start_entry(data);
        if (entry_length(data) < MAX_DIGITS)
            data->edit_buffer[data->num_digits++] = '0' + btype;
    }
    else if (btype == BTYPE_COMMA)
    {
        start_entry(data);
        if (can_insert_comma(data) && entry_length(data) < MAX_DIGITS - 1)
        {
            if (entry_length(data) == 0)
                data->edit_buffer[data->num_digits++] = '0';
            data->edit_buffer[data->num_digits++] = '.';
        }
    }
    else if (is_operator(btype))
    {
        if (data->state == STATE_LEFTVAL || data->state == STATE_EQU)
        {
            num_parse(&data->lvalue, data->edit_buffer);
            tape_line(0, data->num_digits ? data->edit_buffer : "0");
        }
        else if (data->state == STATE_RIGHTVAL)
        {
            num_parse(&data->rvalue, data->edit_buffer);
            tape_line(op2char(data->op), data->num_digits ? data->edit_buffer : "0");
            show_value(data, &data->lvalue, eval_result(&data->lvalue, &data->rvalue, data->op));
        }
        if (data->state != STATE_ERROR)
        {
            data->state = STATE_OP;
            data->op = btype;
        }
    }
    else if (btype == BTYPE_EQU)
    {
        if (data->state != STATE_EQU)
        {
            num_parse(&data->rvalue, data->edit_buffer);
            if (data->op != BTYPE_NONE)
                tape_line(op2char(data->op), data->num_digits ? data->edit_buffer : "0");
        }

        data->state = STATE_EQU;
        show_value(data, &data->lvalue, eval_result(&data->lvalue, &data->rvalue, data->op));
        if (data->state == STATE_EQU)
            tape_line('=', data->edit_buffer);
    }
    else if (btype == BTYPE_CA ||
             (btype == BTYPE_CE && data->state == STATE_ERROR))
    {
        num_zero(&data->lvalue);
        num_zero(&data->rvalue);
        data->op = BTYPE_NONE;
        data->state = STATE_LEFTVAL;
        data->fresh = FALSE;
        clear_edit_buffer(data);
        if (tapefh) tape_puts("\n");
    }
    else if (btype == BTYPE_CE &&
             (data->state == STATE_LEFTVAL || data->state == STATE_RIGHTVAL))
    {
        clear_edit_buffer(data);
    }
    else if (btype == BTYPE_SIGN && data->state != STATE_OP)
    {
        toggle_sign(data);
    }
    else if (btype == BTYPE_BS &&
             (data->state == STATE_LEFTVAL || data->state == STATE_RIGHTVAL) &&
             data->num_digits > 0)
    {
        data->edit_buffer[--data->num_digits] = 0;
    }
    else if (btype == BTYPE_MC)
    {
        num_zero(&data->memory);
    }
    else if (btype == BTYPE_MR)
    {
        start_entry(data);
        n = data->memory;
        show_value(data, &n, TRUE);
        data->fresh = TRUE;
    }
    else if (btype == BTYPE_MADD || btype == BTYPE_MSUB)
    {
        num_parse(&n, data->edit_buffer);
        if (btype == BTYPE_MADD ? !num_add(&data->memory, &n, &data->memory) :
            !num_sub(&data->memory, &n, &data->memory))
        {
            num_zero(&data->memory);
            data->state = STATE_ERROR;
        }
        data->fresh = TRUE;
    }
    display_state(data);
}

/**********************************************************************
 Main program
 **********************************************************************/

static struct Screen *scr;
static APTR vi;
static struct Gadget *glist;
static char errbuf[100];

static void cleanup(char *msg)
{
    struct EasyStruct es;

    if (msg)
    {
        if (IntuitionBase && !((struct Process *)FindTask(NULL))->pr_CLI)
        {
            es.es_StructSize   = sizeof(es);
            es.es_Flags        = 0;
            es.es_Title        = "Calculator";
            es.es_TextFormat   = msg;
            es.es_GadgetFormat = "OK";
            EasyRequestArgs(NULL, &es, NULL, NULL);
        }
        else
        {
            PutStr("Calculator: ");
            PutStr(msg);
            PutStr("\n");
        }
    }
    if (win) CloseWindow(win);
    if (glist) FreeGadgets(glist);
    if (vi) FreeVisualInfo(vi);
    if (scr) UnlockPubScreen(NULL, scr);
    if (tapefh) Close(tapefh);
    if (GadToolsBase) CloseLibrary(GadToolsBase);
    if (IntuitionBase) CloseLibrary((struct Library *)IntuitionBase);
}

static BOOL get_arguments(void)
{
    struct RDArgs *rdargs;
    IPTR args[NUM_ARGS];
    char *s;
    int i;

    for (i = 0; i < NUM_ARGS; i++) args[i] = (IPTR) NULL;

    if (!(rdargs = ReadArgs(ARG_TEMPLATE, (LONG *) args, NULL)))
    {
        Fault(IoErr(), NULL, errbuf, sizeof(errbuf));
        return FALSE;
    }

    if ((s = (char *)args[ARG_PUBSCREEN]))
    {
        for (i = 0; i < 255 && s[i]; i++)
            pubscrname[i] = s[i];
    }

    if (args[ARG_TAPE] && !(tapefh = Open((char *)args[ARG_TAPE], MODE_NEWFILE)))
    {
        Fault(IoErr(), NULL, errbuf, sizeof(errbuf));
        FreeArgs(rdargs);
        return FALSE;
    }
    FreeArgs(rdargs);
    return TRUE;
}

static struct Gadget *make_gadgets(void)
{
    static struct TagItem buttontags[] = { { TAG_DONE, 0 } };
    static struct TagItem displaytags[] = {
        { GTTX_Text, 0 }, { GTTX_Border, TRUE },
        { GTTX_Justification, GTJ_RIGHT }, { TAG_DONE, 0 } };
    struct NewGadget ng;
    struct Gadget *g;
    struct TextFont *font = scr->RastPort.Font;
    WORD fw = font->tf_XSize, fh = font->tf_YSize;
    WORD x0, y0, bw, bh, gap, gridw, mw, i;

    gap = 4;
    bw = 4 * fw + 8;
    bh = fh + 6;
    gridw = COLS * bw + (COLS - 1) * gap;
    mw = (gridw - (MEM_BUTTONS - 1) * gap) / MEM_BUTTONS;
    x0 = scr->WBorLeft + 6;
    y0 = scr->WBorTop + scr->Font->ta_YSize + 1 + 5;

    g = CreateContext(&glist);

    ng.ng_TextAttr = scr->Font;
    ng.ng_VisualInfo = vi;
    ng.ng_Flags = 0;
    ng.ng_UserData = NULL;

    ng.ng_LeftEdge = x0;
    ng.ng_TopEdge = y0;
    ng.ng_Width = gridw;
    ng.ng_Height = fh + 8;
    ng.ng_GadgetText = NULL;
    ng.ng_GadgetID = DISPLAY_ID;
    displaytags[0].ti_Data = (IPTR)display_buffer;
    display_len = (gridw - 6) / fw;
    if (display_len > DISPLAY_MAX)
        display_len = DISPLAY_MAX;
    g = display = CreateGadgetA(TEXT_KIND, g, &ng, displaytags);
    y0 += ng.ng_Height + gap + 2;

    ng.ng_Height = bh;
    ng.ng_Flags = PLACETEXT_IN;
    for (i = 0; i < NUM_BUTTONS; i++)
    {
        if (i < MEM_BUTTONS)
        {
            ng.ng_LeftEdge = x0 + i * (mw + gap);
            ng.ng_TopEdge = y0;
            ng.ng_Width = i == MEM_BUTTONS - 1 ? gridw - i * (mw + gap) : mw;
        }
        else
        {
            ng.ng_LeftEdge = x0 + (i - MEM_BUTTONS) % COLS * (bw + gap);
            ng.ng_TopEdge = y0 + (1 + (i - MEM_BUTTONS) / COLS) * (bh + gap);
            ng.ng_Width = bw;
        }
        ng.ng_GadgetText = (UBYTE *)BUTTONS[i].label;
        ng.ng_GadgetID = i;
        g = CreateGadgetA(BUTTON_KIND, g, &ng, buttontags);
    }
    return g;
}

static BOOL open_window(void)
{
    struct TagItem t[14];
    struct Gadget *g;
    WORD w = 0, h = 0;

    for (g = glist; g; g = g->NextGadget)
    {
        if (g->LeftEdge + g->Width > w) w = g->LeftEdge + g->Width;
        if (g->TopEdge + g->Height > h) h = g->TopEdge + g->Height;
    }

    t[0].ti_Tag = WA_Title;         t[0].ti_Data = (IPTR)"Calculator";
    t[1].ti_Tag = WA_PubScreen;     t[1].ti_Data = (IPTR)scr;
    t[2].ti_Tag = WA_Width;         t[2].ti_Data = w + 6 + scr->WBorRight;
    t[3].ti_Tag = WA_Height;        t[3].ti_Data = h + 6 + scr->WBorBottom;
    t[4].ti_Tag = WA_Left;          t[4].ti_Data = (scr->Width - (w + 6 + scr->WBorRight)) / 2;
    t[5].ti_Tag = WA_Top;           t[5].ti_Data = (scr->Height - (h + 6 + scr->WBorBottom)) / 3;
    t[6].ti_Tag = WA_Gadgets;       t[6].ti_Data = (IPTR)glist;
    t[7].ti_Tag = WA_IDCMP;         t[7].ti_Data = BUTTONIDCMP | IDCMP_VANILLAKEY |
                                                   IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW;
    t[8].ti_Tag = WA_DragBar;       t[8].ti_Data = TRUE;
    t[9].ti_Tag = WA_DepthGadget;   t[9].ti_Data = TRUE;
    t[10].ti_Tag = WA_CloseGadget;  t[10].ti_Data = TRUE;
    t[11].ti_Tag = WA_Activate;     t[11].ti_Data = TRUE;
    t[12].ti_Tag = WA_SimpleRefresh; t[12].ti_Data = TRUE;
    t[13].ti_Tag = TAG_DONE;
    if (!(win = OpenWindowTagList(NULL, t)))
        return FALSE;
    GT_RefreshWindow(win, NULL);
    return TRUE;
}

/* The button of a key, or -1 */
static int key_button(UWORD code)
{
    int i;

    if (code == 13) code = '=';
    if (code == ',') code = '.';
    if (code == 127) code = 'E';
    if (code >= 'a' && code <= 'z') code -= 'a' - 'A';
    for (i = 0; i < NUM_BUTTONS; i++)
        if (BUTTONS[i].shortcut == code)
            return i;
    return -1;
}

static void handle_all(void)
{
    struct IntuiMessage *msg;
    ULONG sigs;
    UWORD code;
    int b;

    for (;;)
    {
        sigs = Wait((1L << win->UserPort->mp_SigBit) | SIGBREAKF_CTRL_C);
        if (sigs & SIGBREAKF_CTRL_C)
            return;
        while ((msg = GT_GetIMsg(win->UserPort)))
        {
            ULONG class = msg->Class;

            code = msg->Code;
            b = class == IDCMP_GADGETUP ? ((struct Gadget *)msg->IAddress)->GadgetID : -1;
            GT_ReplyIMsg(msg);
            switch (class)
            {
                case IDCMP_CLOSEWINDOW:
                    return;
                case IDCMP_REFRESHWINDOW:
                    GT_BeginRefresh(win);
                    GT_EndRefresh(win, TRUE);
                    break;
                case IDCMP_VANILLAKEY:
                    b = key_button(code);
                    /* fall through */
                case IDCMP_GADGETUP:
                    if (b >= 0 && b < NUM_BUTTONS)
                        add_key(&calc, BUTTONS[b].btype);
                    break;
            }
        }
    }
}

int main(void)
{
    struct Process *me = (struct Process *)FindTask(NULL);
    struct WBStartup *startup = NULL;
    char *err = NULL;

    if (!me->pr_CLI)
    {
        WaitPort(&me->pr_MsgPort);
        startup = (struct WBStartup *)GetMsg(&me->pr_MsgPort);
    }
    if ((DOSBase = (struct DosLibrary *)OpenLibrary(DOSNAME, 36)))
    {
        calc.op = BTYPE_NONE;
        if (!startup && !get_arguments())
            err = errbuf;
        else if (!(IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 39)) ||
                 !(GadToolsBase = OpenLibrary("gadtools.library", 39)))
            err = "Can't open gadtools.library";
        else if (!((pubscrname[0] && (scr = LockPubScreen(pubscrname))) ||
                   (scr = LockPubScreen(NULL))))
            err = "Can't lock the screen";
        else if (!(vi = GetVisualInfoA(scr, NULL)) || !make_gadgets() || !open_window())
            err = "Can't open the window";
        else
        {
            display_state(&calc);
            handle_all();
        }
        cleanup(err);
        CloseLibrary((struct Library *)DOSBase);
    }
    if (startup)
    {
        Forbid();
        ReplyMsg((struct Message *)startup);
    }
    return err ? RETURN_FAIL : RETURN_OK;
}
