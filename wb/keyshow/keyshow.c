/*
    Copyright (C) 2012-2026, The AROS Development Team. All rights reserved.

    Desc: KeyShow: the C900 keyboard with the current keymap's characters,
          on intuition and gadtools.  Pressed keys are highlighted, and
          Shift, Alt and Ctrl show the characters they give.
*/

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <devices/rawkeycodes.h>
#include <devices/keyboard.h>
#include <devices/keymap.h>
#include <intuition/intuition.h>
#include <libraries/gadtools.h>
#include <workbench/startup.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/gadtools.h>
#include <proto/keymap.h>

const char *version = "$VER: KeyShow 1.7 (15.11.2026)";

struct DosLibrary *DOSBase;
struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *GadToolsBase;
struct Library *KeymapBase;

#define KEYCOUNT    0x68
#define NQUALS      7

/*
 * A key's characters by qualifier, in KCF bits: alone, shift, alt,
 * shift_alt, ctrl, ctrl_shift, ctrl_alt.
 */
struct Key
{
    TEXT label[NQUALS][3];
};

// copied from rom/keymap/defaultkeymap.c
static const BYTE keymapstr_table[8][8] =
{
    {0, 0, 0, 0, 0, 0, 0, 0},   /* KCF_NOQUAL                   == 0 */
    {0, 1, 0, 1, 0, 1, 0, 1},   /* KCF_SHIFT                    == 1 */
    {0, 0, 1, 1, 0, 0, 1, 1},   /* KCF_ALT                      == 2 */
    {0, 1, 2, 3, 0, 1, 2, 3},   /* KCF_SHIFT|KCF_ALT            == 3 */
    {0, 0, 0, 0, 1, 1, 1, 1},   /* KCF_CONTROL                  == 4 */
    {0, 1, 0, 1, 2, 3, 2, 3},   /* KCF_SHIFT|KCF_CONTROL        == 5 */
    {0, 0, 1, 1, 2, 2, 3, 3},   /* KCF_ALT|KCF_CONTROL          == 6 */
    {0, 1, 2, 3, 4, 5, 6, 7}    /* KCF_SHIFT|KCF_ALT|KCF_CONTROL == KC__VANILLA == 7 */
};

// TODO: is this order always the same?
static const char deadkey_table[] = " \xB4`^~\"\xB0";

static struct Key keys[KEYCOUNT];

static void set_char(STRPTR buffer, UBYTE c)
{
    if (c > 31)
    {
        buffer[0] = c;
        buffer[1] = 0;
    }
}

static void parse_normal_key(struct Key *key, UBYTE type, IPTR value)
{
    set_char(key->label[KC_NOQUAL], value & 0xff);
    switch (type)
    {
        case KCF_SHIFT:
            set_char(key->label[KCF_SHIFT], (value >> 8) & 0xff);
            break;
        case KCF_ALT:
            set_char(key->label[KCF_ALT], (value >> 8) & 0xff);
            break;
        case KCF_CONTROL:
            set_char(key->label[KCF_CONTROL], (value >> 8) & 0xff);
            break;
        case KCF_ALT + KCF_SHIFT:
            set_char(key->label[KCF_SHIFT], (value >> 8) & 0xff);
            set_char(key->label[KCF_ALT], (value >> 16) & 0xff);
            set_char(key->label[KCF_ALT + KCF_SHIFT], (value >> 24) & 0xff);
            break;
        case KCF_CONTROL + KCF_ALT:
            set_char(key->label[KCF_ALT], (value >> 8) & 0xff);
            set_char(key->label[KCF_CONTROL], (value >> 16) & 0xff);
            set_char(key->label[KCF_CONTROL + KCF_ALT], (value >> 24) & 0xff);
            break;
        case KCF_CONTROL + KCF_SHIFT:
            set_char(key->label[KCF_SHIFT], (value >> 8) & 0xff);
            set_char(key->label[KCF_CONTROL], (value >> 16) & 0xff);
            set_char(key->label[KCF_CONTROL + KCF_SHIFT], (value >> 24) & 0xff);
            break;
        case KC_VANILLA:
            set_char(key->label[KCF_SHIFT], (value >> 8) & 0xff);
            set_char(key->label[KCF_ALT], (value >> 16) & 0xff);
            set_char(key->label[KCF_ALT + KCF_SHIFT], (value >> 24) & 0xff);
            if (value & 0xff)
            {
                key->label[KCF_CONTROL][0] = '^';
                key->label[KCF_CONTROL][1] = value & 0xff;
                key->label[KCF_CONTROL][2] = 0;
            }
            break;
    }
}

static UBYTE set_string_key(UBYTE type, IPTR value, BYTE qual)
{
    BYTE idx;

    idx = keymapstr_table[type][qual];

    if (idx != -1)
    {
        UBYTE *str_descrs = (UBYTE *)value;
        UBYTE len, offset;

        /* Since each string descriptor uses two bytes we multiply by 2 */
        idx *= 2;

        /* Get string info from string descriptor table */
        len    = str_descrs[idx];
        offset = str_descrs[idx + 1];

        /* Return char only if len is 1 */
        if (len == 1 && str_descrs[offset] > 31)
        {
            return str_descrs[offset];
        }
    } /* if (idx != -1) */

    return 0;
}

static void parse_string_key(struct Key *key, UBYTE type, IPTR value)
{
    UBYTE q;

    for (q = 0; q < NQUALS; q++)
        set_char(key->label[q], set_string_key(type, value, q));
}

static void set_dead_key(STRPTR buffer, UBYTE type, IPTR value, BYTE qual)
{
    BYTE idx;
    UBYTE deadidx;

    /* Use keymap_str table to get idx to right key descriptor */
    idx = keymapstr_table[type & KC_VANILLA][qual];
    if (idx != -1)
    {
        UBYTE *dead_descr = (UBYTE *)value;

        switch (dead_descr[idx * 2])
        {
            case DPF_DEAD:
                // dead key
                deadidx = dead_descr[idx * 2 + 1];
                if (deadidx < 8)
                {
                    set_char(buffer, deadkey_table[deadidx]);
                }
                break;

            case DPF_MOD:
                // deadable key
                set_char(buffer, dead_descr[dead_descr[idx * 2 + 1]]);
                break;

            case 0:
                set_char(buffer, dead_descr[idx * 2 + 1]);
                break;

        }
    }
}

static void parse_dead_key(struct Key *key, UBYTE type, IPTR value)
{
    UBYTE q;

    for (q = 0; q < NQUALS; q++)
        set_dead_key(key->label[q], type, value, q);
}

static void read_keymap(void)
{
    struct KeyMap *km = AskKeyMapDefault();
    LONG i;
    IPTR value;
    UBYTE type, q;

    for (i = 0; i < KEYCOUNT; i++) /* 0x40 Lo, 0x28 Hi */
    {
        if (i < 0x40)
        {
            type = km->km_LoKeyMapTypes[i];
            value = km->km_LoKeyMap[i];
        }
        else
        {
            type = km->km_HiKeyMapTypes[i-0x40];
            value = km->km_HiKeyMap[i-0x40];
        }

        if (type & KCF_NOP)
            continue;
        if (type & KCF_STRING)
        {
            parse_string_key(&keys[i], type & KC_VANILLA, value);
        }
        else if (type & KCF_DEAD)
        {
            parse_dead_key(&keys[i], type & KC_VANILLA, value);
        }
        else
        {
            parse_normal_key(&keys[i], type & KC_VANILLA, value);
            /* A qualifier the key's type lacks gives what the key gives without it */
            for (q = 1; q < NQUALS; q++)
                if (q & ~type)
                    CopyMem(keys[i].label[q & type], keys[i].label[q], 3);
        }
    }
}

/* The keys whose labels the keymap does not give */
struct Fixed
{
    UBYTE code;
    const char *label;
};

static const struct Fixed fixed[] =
{
    {RAWKEY_ESCAPE, "Esc"}, {RAWKEY_F1, "F1"}, {RAWKEY_F2, "F2"}, {RAWKEY_F3, "F3"},
    {RAWKEY_F4, "F4"}, {RAWKEY_F5, "F5"}, {RAWKEY_F6, "F6"}, {RAWKEY_F7, "F7"},
    {RAWKEY_F8, "F8"}, {RAWKEY_F9, "F9"}, {RAWKEY_F10, "F10"}, {RAWKEY_F12, "F12"},
    {RAWKEY_F13, "F13"}, {RAWKEY_F14, "F14"}, {RAWKEY_F15, "F15"}, {RAWKEY_HELP, "Hlp"},
    {RAWKEY_BACKSPACE, "BS"}, {RAWKEY_TAB, "Tab"}, {RAWKEY_DELETE, "Del"},
    {RAWKEY_CONTROL, "Ctrl"}, {RAWKEY_RETURN, "Return"}, {RAWKEY_LSHIFT, "Shift"},
    {RAWKEY_RSHIFT, "Shift"}, {RAWKEY_CAPSLOCK, "Caps"}, {RAWKEY_LALT, "Alt"},
    {RAWKEY_SPACE, ""}, {RAWKEY_UP, "^"}, {RAWKEY_DOWN, "v"}, {RAWKEY_LEFT, "<"},
    {RAWKEY_RIGHT, ">"}, {RAWKEY_W1, "W1"}, {RAWKEY_W2, "W2"}, {RAWKEY_W3, "W3"},
    {RAWKEY_KP_ENTER, "Ent"}, {RAWKEY_KP_00, "00"}, {RAWKEY_KP_CE, "CE"},
    {0, NULL}
};

/*
 * The C900 keyboard: runs of keys with consecutive raw codes, in quarter
 * keys across and quarter rows down.
 */
struct Run
{
    UBYTE code, n, x, y, w, h;
};

static const struct Run runs[] =
{
    {RAWKEY_ESCAPE, 1, 0, 0, 4, 4}, {RAWKEY_F1, 10, 6, 0, 4, 4},
    {RAWKEY_F12, 1, 48, 0, 4, 4}, {RAWKEY_F13, 3, 52, 0, 4, 4},
    {RAWKEY_HELP, 1, 69, 0, 4, 4},

    {RAWKEY_TILDE, 1, 0, 5, 4, 4}, {RAWKEY_1, 13, 4, 5, 4, 4},
    {RAWKEY_BACKSPACE, 1, 56, 5, 4, 4},
    {RAWKEY_TAB, 1, 0, 9, 6, 4}, {RAWKEY_Q, 12, 6, 9, 4, 4},
    {RAWKEY_DELETE, 1, 54, 9, 6, 4},
    {RAWKEY_CONTROL, 1, 0, 13, 7, 4}, {RAWKEY_A, 11, 7, 13, 4, 4},
    {RAWKEY_RETURN, 1, 51, 13, 9, 4},
    {RAWKEY_LSHIFT, 1, 0, 17, 9, 4}, {RAWKEY_Z, 10, 9, 17, 4, 4},
    {RAWKEY_RSHIFT, 1, 49, 17, 11, 4},
    {RAWKEY_CAPSLOCK, 1, 0, 21, 6, 4}, {RAWKEY_LALT, 1, 6, 21, 6, 4},
    {RAWKEY_SPACE, 1, 18, 21, 36, 4},

    {RAWKEY_W1, 2, 61, 5, 4, 4}, {RAWKEY_W3, 1, 69, 5, 4, 4},
    {RAWKEY_UP, 1, 65, 17, 4, 4}, {RAWKEY_LEFT, 1, 61, 21, 4, 4},
    {RAWKEY_DOWN, 1, 65, 21, 4, 4}, {RAWKEY_RIGHT, 1, 69, 21, 4, 4},

    {RAWKEY_KP_CE, 1, 73, 5, 4, 4}, {RAWKEY_KP_DIVIDE, 2, 77, 5, 4, 4},
    {RAWKEY_KP_MINUS, 1, 85, 5, 4, 4},
    {RAWKEY_KP_7, 3, 73, 9, 4, 4}, {RAWKEY_KP_PLUS, 1, 85, 9, 4, 8},
    {RAWKEY_KP_4, 3, 73, 13, 4, 4},
    {RAWKEY_KP_1, 3, 73, 17, 4, 4}, {RAWKEY_KP_ENTER, 1, 85, 17, 4, 8},
    {RAWKEY_KP_0, 1, 73, 21, 4, 4}, {RAWKEY_KP_00, 1, 77, 21, 4, 4},
    {RAWKEY_KP_DECIMAL, 1, 81, 21, 4, 4},
    {0, 0, 0, 0, 0, 0}
};

#define LAYOUT_W    89
#define LAYOUT_H    25

static struct Screen *scr;
static struct DrawInfo *dri;
static APTR vi;
static struct Window *win;
static struct RastPort *rp;
static WORD unit, row, x0, y0;
static UBYTE pressed[16];
static UBYTE qual;

#define ISDOWN(c)   (pressed[(c) >> 3] & (1 << ((c) & 7)))

/* The key's run and its place in it, or NULL */
static const struct Run *find_key(UBYTE code, WORD *i)
{
    const struct Run *r;

    for (r = runs; r->n; r++)
        if (code >= r->code && code < r->code + r->n)
        {
            *i = code - r->code;
            return r;
        }
    return NULL;
}

static const char *label(UBYTE code)
{
    const struct Fixed *f;

    for (f = fixed; f->label; f++)
        if (f->code == code)
            return f->label;
    if (code >= KEYCOUNT)
        return "";
    return keys[code].label[qual];
}

static void draw_key(UBYTE code)
{
    struct TagItem t[3];
    struct TextExtent te;
    const struct Run *r;
    const char *s;
    WORD i, x, y, w, h, n, len;
    BOOL down = ISDOWN(code) != 0;

    if (!(r = find_key(code, &i)))
        return;
    x = x0 + (r->x + i * r->w) * unit / 4;
    y = y0 + r->y * row / 4;
    w = r->w * unit / 4 - 2;
    h = r->h * row / 4 - 2;

    SetAPen(rp, dri->dri_Pens[down ? FILLPEN : BACKGROUNDPEN]);
    RectFill(rp, x + 2, y + 1, x + w - 3, y + h - 2);
    t[0].ti_Tag = GT_VisualInfo;    t[0].ti_Data = (IPTR)vi;
    t[1].ti_Tag = down ? GTBB_Recessed : TAG_IGNORE; t[1].ti_Data = TRUE;
    t[2].ti_Tag = TAG_DONE;
    DrawBevelBoxA(rp, x, y, w, h, t);

    s = label(code);
    for (len = 0; s[len]; len++)
        ;
    n = TextFit(rp, (STRPTR)s, len, &te, NULL, 1, w - 2, h - 2);
    if (n > 0)
    {
        SetAPen(rp, dri->dri_Pens[down ? FILLTEXTPEN : TEXTPEN]);
        SetDrMd(rp, JAM1);
        Move(rp, x + (w - te.te_Width) / 2, y + (h - rp->TxHeight) / 2 + rp->TxBaseline);
        Text(rp, (STRPTR)s, n);
    }
}

static void draw_all(void)
{
    const struct Run *r;
    WORD i;

    for (r = runs; r->n; r++)
        for (i = 0; i < r->n; i++)
            draw_key(r->code + i);
}

/* Shift, Alt and Ctrl as KCF bits; all three show the plain keys */
static UBYTE quals(UWORD q)
{
    UBYTE k = 0;

    if (q & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) k |= KCF_SHIFT;
    if (q & (IEQUALIFIER_LALT | IEQUALIFIER_RALT)) k |= KCF_ALT;
    if (q & IEQUALIFIER_CONTROL) k |= KCF_CONTROL;
    return k == KC_VANILLA ? KC_NOQUAL : k;
}

static void set_quals(UWORD q)
{
    UBYTE old = qual;
    WORD i;
    const struct Fixed *f;

    if ((qual = quals(q)) == old)
        return;
    for (i = 0; i < KEYCOUNT; i++)
    {
        for (f = fixed; f->label && f->code != i; f++)
            ;
        if (!f->label && (keys[i].label[old][0] != keys[i].label[qual][0] ||
                          keys[i].label[old][1] != keys[i].label[qual][1]))
            draw_key(i);
    }
}

static void rawkey(UWORD code, UWORD q)
{
    UBYTE c = code & ~IECODE_UP_PREFIX;
    BOOL up = (code & IECODE_UP_PREFIX) != 0;

    if (c >= 0x78 || (q & IEQUALIFIER_REPEAT))
        return;
    if (!up == !ISDOWN(c))
    {
        if (up)
            pressed[c >> 3] &= ~(1 << (c & 7));
        else
            pressed[c >> 3] |= 1 << (c & 7);
        draw_key(c);
    }
    set_quals(q);
}

/* Key-ups go to the active window only */
static void release_all(void)
{
    WORD c;

    for (c = 0; c < 0x78; c++)
        if (ISDOWN(c))
        {
            pressed[c >> 3] &= ~(1 << (c & 7));
            draw_key(c);
        }
    set_quals(0);
}

static char *setup(void)
{
    struct TagItem t[13];
    struct TextFont *font;
    WORD w, h;

    if (!(IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 39)) ||
        !(GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 39)) ||
        !(GadToolsBase = OpenLibrary("gadtools.library", 39)) ||
        !(KeymapBase = OpenLibrary("keymap.library", 37)))
        return "Can't open keymap.library";
    read_keymap();
    if (!(scr = LockPubScreen(NULL)) || !(dri = GetScreenDrawInfo(scr)) ||
        !(vi = GetVisualInfoA(scr, NULL)))
        return "Can't lock the screen";

    font = dri->dri_Font;
    /* Three characters a key, narrower on a narrow screen */
    x0 = scr->WBorLeft + 2;
    y0 = scr->WBorTop + scr->Font->ta_YSize + 1 + 3;
    unit = font->tf_XSize * 3 + 4;
    if (x0 + LAYOUT_W * unit / 4 + 2 + scr->WBorRight > scr->Width)
        unit = (scr->Width - x0 - 2 - scr->WBorRight) * 4 / LAYOUT_W;
    row = (font->tf_YSize + 8) & ~3;
    w = x0 + LAYOUT_W * unit / 4 + 2 + scr->WBorRight;
    h = y0 + LAYOUT_H * row / 4 + 2 + scr->WBorBottom;

    t[0].ti_Tag = WA_Title;         t[0].ti_Data = (IPTR)"KeyShow";
    t[1].ti_Tag = WA_PubScreen;     t[1].ti_Data = (IPTR)scr;
    t[2].ti_Tag = WA_Width;         t[2].ti_Data = w;
    t[3].ti_Tag = WA_Height;        t[3].ti_Data = h;
    t[4].ti_Tag = WA_Left;          t[4].ti_Data = w < scr->Width ? (scr->Width - w) / 2 : 0;
    t[5].ti_Tag = WA_Top;           t[5].ti_Data = h < scr->Height ? (scr->Height - h) / 2 : 0;
    t[6].ti_Tag = WA_IDCMP;         t[6].ti_Data = IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW |
                                                   IDCMP_RAWKEY | IDCMP_INACTIVEWINDOW;
    t[7].ti_Tag = WA_DragBar;       t[7].ti_Data = TRUE;
    t[8].ti_Tag = WA_DepthGadget;   t[8].ti_Data = TRUE;
    t[9].ti_Tag = WA_CloseGadget;   t[9].ti_Data = TRUE;
    t[10].ti_Tag = WA_Activate;     t[10].ti_Data = TRUE;
    t[11].ti_Tag = WA_SimpleRefresh; t[11].ti_Data = TRUE;
    t[12].ti_Tag = TAG_DONE;
    if (!(win = OpenWindowTagList(NULL, t)))
        return "Can't open the window";
    rp = win->RPort;
    SetFont(rp, font);
    return NULL;
}

static void cleanup(char *msg)
{
    struct EasyStruct es;

    if (msg)
    {
        if (IntuitionBase && !((struct Process *)FindTask(NULL))->pr_CLI)
        {
            es.es_StructSize   = sizeof(es);
            es.es_Flags        = 0;
            es.es_Title        = "KeyShow";
            es.es_TextFormat   = msg;
            es.es_GadgetFormat = "OK";
            EasyRequestArgs(NULL, &es, NULL, NULL);
        }
        else
        {
            PutStr("KeyShow: ");
            PutStr(msg);
            PutStr("\n");
        }
    }
    if (win) CloseWindow(win);
    if (vi) FreeVisualInfo(vi);
    if (dri) FreeScreenDrawInfo(scr, dri);
    if (scr) UnlockPubScreen(NULL, scr);
    if (KeymapBase) CloseLibrary(KeymapBase);
    if (GadToolsBase) CloseLibrary(GadToolsBase);
    if (GfxBase) CloseLibrary((struct Library *)GfxBase);
    if (IntuitionBase) CloseLibrary((struct Library *)IntuitionBase);
}

static void handle_all(void)
{
    struct IntuiMessage *msg;
    ULONG sigs, class;
    UWORD code, q;

    draw_all();
    for (;;)
    {
        sigs = Wait((1L << win->UserPort->mp_SigBit) | SIGBREAKF_CTRL_C);
        if (sigs & SIGBREAKF_CTRL_C)
            return;
        while ((msg = GT_GetIMsg(win->UserPort)))
        {
            class = msg->Class;
            code = msg->Code;
            q = msg->Qualifier;
            GT_ReplyIMsg(msg);
            switch (class)
            {
                case IDCMP_CLOSEWINDOW:
                    return;
                case IDCMP_REFRESHWINDOW:
                    GT_BeginRefresh(win);
                    draw_all();
                    GT_EndRefresh(win, TRUE);
                    break;
                case IDCMP_RAWKEY:
                    rawkey(code, q);
                    break;
                case IDCMP_INACTIVEWINDOW:
                    release_all();
                    break;
            }
        }
    }
}

int main(void)
{
    struct Process *me = (struct Process *)FindTask(NULL);
    struct WBStartup *startup = NULL;
    char *err;

    if (!me->pr_CLI)
    {
        WaitPort(&me->pr_MsgPort);
        startup = (struct WBStartup *)GetMsg(&me->pr_MsgPort);
    }
    if (!(DOSBase = (struct DosLibrary *)OpenLibrary(DOSNAME, 36)))
        err = "";
    else
    {
        if (!(err = setup()))
            handle_all();
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
