/*
 * gadtools.library from LIBS:, on screens in RAM bitmaps: mono
 * 1024x800, colour 640x400 and the 1.x colour look.  On each, a window
 * with a gadget of every kind and a menu strip, made through the
 * varargs calls; checks the pixels of the bevels and labels, then drives
 * the gadgets and menus with events written to input.device and reads
 * the results through GT_GetIMsg.
 */
#include <exec/exec.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <devices/rawkeycodes.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <graphics/gfxdriver.h>
#include <graphics/rastport.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <intuition/preferences.h>
#include <libraries/gadtools.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <proto/alib.h>

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *GadToolsBase;

static int fails;
static char *look;

static void check(int ok, char *what)
{
	if (!ok) {
		xKPrintf("FAIL gadtools: %s: %s\n", look, what);
		fails++;
	}
}

/* Events through input.device, which runs intuition's handler */
static struct MsgPort *port;
static struct IOStdReq *io;
static struct InputEvent ev;

static void event(UBYTE class, UWORD code, WORD x, WORD y)
{
	ev.ie_NextEvent = NULL;
	ev.ie_Class = class;
	ev.ie_SubClass = 0;
	ev.ie_Code = code;
	ev.ie_Qualifier = 0;
	ev.ie_X = x;
	ev.ie_Y = y;
	io->io_Command = IND_WRITEEVENT;
	io->io_Data = &ev;
	io->io_Length = sizeof(ev);
	DoIO((struct IORequest *)io);
}

static void mouse(WORD x, WORD y)
{
	event(IECLASS_RAWMOUSE, IECODE_NOBUTTON, x, y);
}

static void click(WORD x, WORD y)
{
	mouse(x, y);
	event(IECLASS_RAWMOUSE, IECODE_LBUTTON, x, y);
	event(IECLASS_RAWMOUSE, IECODE_LBUTTON | IECODE_UP_PREFIX, x, y);
}

static void key(UWORD code)
{
	event(IECLASS_RAWKEY, code, 0, 0);
	event(IECLASS_RAWKEY, code | IECODE_UP_PREFIX, 0, 0);
}

static struct Screen *scr;
static struct Window *win;
static struct DrawInfo *dri;

/* The next message of a class through GT_GetIMsg, others dropped */
static UWORD msgcode;
static struct Gadget *msggad;

static BOOL gtmsg(ULONG class)
{
	struct IntuiMessage *im;
	ULONG c;

	while ((im = GT_GetIMsg(win->UserPort)) != NULL) {
		c = im->Class;
		msgcode = im->Code;
		msggad = (struct Gadget *)im->IAddress;
		GT_ReplyIMsg(im);
		if (c == class)
			return TRUE;
	}
	return FALSE;
}

/* A message the menu task sends: wait up to 1 s */
static BOOL gtlate(ULONG class)
{
	int i;

	for (i = 0; i < 50; i++) {
		if (gtmsg(class))
			return TRUE;
		TimeDelay(UNIT_VBLANK, 0, 20000);
	}
	return FALSE;
}

static void flush(void)
{
	while (gtmsg(0))
		;
}

/* The gadgets, by ID */
enum { G_BUTTON = 1, G_CHECKBOX, G_INTEGER, G_STRING, G_NUMBER, G_TEXT,
	G_CYCLE, G_SLIDER, G_SCROLLER, G_MX, G_PALETTE, G_LISTVIEW, G_GENERIC,
	NGADS };

static struct Gadget *gads[NGADS];

/* Pixels of pen p in a box of the window */
static WORD count(UWORD p, WORD x0, WORD y0, WORD x1, WORD y1)
{
	WORD x, y, n = 0;

	for (y = y0; y <= y1; y++)
		for (x = x0; x <= x1; x++)
			n += ReadPixel(win->RPort, x, y) == p;
	return n;
}

#define PEN(p)	(dri->dri_Pens[p])

/* The screen position of a point of a gadget */
#define GX(g, dx)	(win->LeftEdge + (g)->LeftEdge + (dx))
#define GY(g, dy)	(win->TopEdge + (g)->TopEdge + (dy))

static STRPTR cyclabels[] = { "One", "Two", "Three", NULL };
static STRPTR mxlabels[] = { "Alpha", "Beta", "Gamma", NULL };
static struct List lvlist;
#define NLV	12
static struct Node lvnodes[NLV];
static STRPTR lvnames[NLV] = { "Zero", "One", "Two", "Three", "Four", "Five",
	"Six", "Seven", "Eight", "Nine", "Ten", "Eleven" };

static struct NewMenu newmenu[] = {
	{ NM_TITLE, "Project", NULL, 0, 0, NULL },
	{ NM_ITEM, "Open", "O", 0, 0, (APTR)11 },
	{ NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL },
	{ NM_ITEM, "Quit", "Q", 0, 0, (APTR)12 },
	{ NM_TITLE, "Edit", NULL, 0, 0, NULL },
	{ NM_ITEM, "Cut", NULL, 0, 0, (APTR)21 },
	{ NM_ITEM, "Toggle", NULL, CHECKIT | MENUTOGGLE, 0, (APTR)22 },
	{ NM_ITEM, "More", NULL, 0, 0, NULL },
	{ NM_SUB, "One", NULL, 0, 0, (APTR)231 },
	{ NM_SUB, "Two", NULL, 0, 0, (APTR)232 },
	{ NM_END, NULL, NULL, 0, 0, NULL }
};

static struct Gadget *glist;
static struct Menu *menus;
static APTR vi;
static struct NewGadget ng;

/* ng of gadget id: its label, flags, position and size */
static struct NewGadget *at(UWORD id, char *label, ULONG flags, WORD x,
    WORD y, WORD w, WORD h)
{
	ng.ng_LeftEdge = scr->WBorLeft + x;
	ng.ng_TopEdge = scr->WBorTop + scr->Font->ta_YSize + 1 + y;
	ng.ng_Width = w;
	ng.ng_Height = h;
	ng.ng_GadgetText = label;
	ng.ng_TextAttr = NULL;
	ng.ng_GadgetID = id;
	ng.ng_Flags = flags;
	ng.ng_VisualInfo = vi;
	ng.ng_UserData = NULL;
	return &ng;
}

/* The gadget CreateGadget made from ng */
static struct Gadget *made(struct Gadget *g)
{
	check(g != NULL, ng.ng_GadgetText ? ng.ng_GadgetText : "unlabelled gadget");
	gads[ng.ng_GadgetID] = g;
	return g;
}

static BOOL build(void)
{
	struct Gadget *g;
	ULONG depth;
	WORD i;

	NewList(&lvlist);
	for (i = 0; i < NLV; i++) {
		lvnodes[i].ln_Name = lvnames[i];
		AddTail(&lvlist, &lvnodes[i]);
	}
	depth = GetBitMapAttr(scr->RastPort.BitMap, BMA_DEPTH);
	if (depth > 2)
		depth = 2;

	vi = GetVisualInfo(scr, TAG_DONE);
	check(vi != NULL, "GetVisualInfo");
	if (vi == NULL)
		return FALSE;
	g = CreateContext(&glist);
	check(g != NULL, "CreateContext");
	g = made(CreateGadget(BUTTON_KIND, g, at(G_BUTTON, "_Button", 0, 90, 4, 80, 14),
	    GT_Underscore, (ULONG)'_', TAG_DONE));
	g = made(CreateGadget(CHECKBOX_KIND, g, at(G_CHECKBOX, "Check", 0, 90, 22, 0, 0),
	    TAG_DONE));
	g = made(CreateGadget(INTEGER_KIND, g, at(G_INTEGER, "Integer", 0, 90, 38, 80, 14),
	    GTIN_Number, 7L, GTIN_MaxChars, 6L, TAG_DONE));
	g = made(CreateGadget(STRING_KIND, g, at(G_STRING, "String", 0, 90, 56, 120, 14),
	    GTST_String, "abc", GTST_MaxChars, 30L, TAG_DONE));
	g = made(CreateGadget(NUMBER_KIND, g, at(G_NUMBER, "Number", 0, 90, 74, 80, 12),
	    GTNM_Number, 42L, GTNM_Border, (ULONG)TRUE, TAG_DONE));
	g = made(CreateGadget(TEXT_KIND, g, at(G_TEXT, "Text", 0, 90, 90, 120, 12),
	    GTTX_Text, "hello", GTTX_Border, (ULONG)TRUE, TAG_DONE));
	g = made(CreateGadget(CYCLE_KIND, g, at(G_CYCLE, "Cycle", 0, 90, 106, 120, 14),
	    GTCY_Labels, cyclabels, TAG_DONE));
	g = made(CreateGadget(SLIDER_KIND, g, at(G_SLIDER, "Slider", 0, 90, 124, 100, 11),
	    GTSL_Min, 0L, GTSL_Max, 10L, GTSL_Level, 3L, GTSL_MaxLevelLen, 2L,
	    GTSL_LevelFormat, "%2ld", GTSL_LevelPlace, (ULONG)PLACETEXT_RIGHT,
	    TAG_DONE));
	g = made(CreateGadget(SCROLLER_KIND, g, at(G_SCROLLER, "Scroller", 0, 90, 140, 120, 12),
	    GTSC_Total, 20L, GTSC_Visible, 5L, GTSC_Top, 0L, GTSC_Arrows, 16L,
	    TAG_DONE));
	g = made(CreateGadget(MX_KIND, g, at(G_MX, NULL, PLACETEXT_RIGHT, 260, 4, 0, 0),
	    GTMX_Labels, mxlabels, GTMX_Spacing, 2L, TAG_DONE));
	g = made(CreateGadget(PALETTE_KIND, g, at(G_PALETTE, "Palette", PLACETEXT_ABOVE, 260, 50, 100, 20),
	    GTPA_Depth, depth, GTPA_Color, 1L, GTPA_IndicatorWidth, 16L,
	    TAG_DONE));
	g = made(CreateGadget(LISTVIEW_KIND, g, at(G_LISTVIEW, "List", PLACETEXT_ABOVE, 260, 90, 120, 70),
	    GTLV_Labels, &lvlist, GTLV_ShowSelected, NULL, GTLV_Selected, ~0L,
	    TAG_DONE));
	g = made(CreateGadget(GENERIC_KIND, g, at(G_GENERIC, NULL, 0, 10, 160, 60, 12),
	    TAG_DONE));
	if (g) {
		/* a boolean gadget the program draws itself */
		g->GadgetType |= GTYP_BOOLGADGET;
		g->Activation = GACT_RELVERIFY;
		g->Flags |= GFLG_GADGHCOMP;
	}
	if (g == NULL)
		return FALSE;

	menus = CreateMenus(newmenu, TAG_DONE);
	check(menus != NULL, "CreateMenus");
	if (menus == NULL)
		return FALSE;
	check(LayoutMenus(menus, vi, GTMN_NewLookMenus, (ULONG)TRUE, TAG_DONE),
	    "LayoutMenus");
	return TRUE;
}

#define WIDCMP	(BUTTONIDCMP | CHECKBOXIDCMP | INTEGERIDCMP | STRINGIDCMP | \
	CYCLEIDCMP | SLIDERIDCMP | SCROLLERIDCMP | ARROWIDCMP | MXIDCMP | \
	PALETTEIDCMP | LISTVIEWIDCMP | IDCMP_MENUPICK | IDCMP_REFRESHWINDOW)

static BOOL openwin(void)
{
	struct TagItem t[12];

	t[0].ti_Tag = WA_Left;		t[0].ti_Data = 0;
	t[1].ti_Tag = WA_Top;		t[1].ti_Data = scr->BarHeight + 1;
	t[2].ti_Tag = WA_Width;		t[2].ti_Data = 420;
	t[3].ti_Tag = WA_Height;	t[3].ti_Data = 200;
	t[4].ti_Tag = WA_Title;		t[4].ti_Data = (IPTR)"GadTools";
	t[5].ti_Tag = WA_CustomScreen;	t[5].ti_Data = (IPTR)scr;
	t[6].ti_Tag = WA_IDCMP;		t[6].ti_Data = WIDCMP;
	t[7].ti_Tag = WA_Gadgets;	t[7].ti_Data = (IPTR)glist;
	t[8].ti_Tag = WA_DragBar;	t[8].ti_Data = TRUE;
	t[9].ti_Tag = WA_Activate;	t[9].ti_Data = TRUE;
	t[10].ti_Tag = WA_SmartRefresh;	t[10].ti_Data = TRUE;
	t[11].ti_Tag = TAG_DONE;
	win = OpenWindowTagList(NULL, t);
	check(win != NULL, "OpenWindowTagList");
	if (win == NULL)
		return FALSE;
	GT_RefreshWindow(win, NULL);
	check(SetMenuStrip(win, menus), "SetMenuStrip");
	return TRUE;
}

/* The bevels and labels */
static void pixels(void)
{
	struct Gadget *g;
	WORD l, t, r, b;

	/* button: raised bevel, label inside */
	g = gads[G_BUTTON];
	l = g->LeftEdge;
	t = g->TopEdge;
	r = l + g->Width - 1;
	b = t + g->Height - 1;
	check(ReadPixel(win->RPort, l, t + 2) == PEN(SHINEPEN) &&
	    ReadPixel(win->RPort, l + 2, t) == PEN(SHINEPEN) &&
	    ReadPixel(win->RPort, r, t + 2) == PEN(SHADOWPEN) &&
	    ReadPixel(win->RPort, l + 2, b) == PEN(SHADOWPEN), "button bevel");
	check(count(PEN(TEXTPEN), l + 2, t + 2, r - 2, b - 2) > 15,
	    "button label");
	/* "_Button": "Button", then an underlined "B" */
	check(g->GadgetText && g->GadgetText->NextText &&
	    g->GadgetText->NextText->IText[0] == 'B' &&
	    (g->GadgetText->NextText->ITextFont->ta_Style & FSF_UNDERLINED),
	    "button underscore");

	/* labels left of the gadgets */
	g = gads[G_CHECKBOX];
	check(count(PEN(TEXTPEN), g->LeftEdge - 50, g->TopEdge,
	    g->LeftEdge - 4, g->TopEdge + g->Height - 1) > 15,
	    "checkbox label");
	check(g->Width == CHECKBOX_WIDTH && g->Height == CHECKBOX_HEIGHT,
	    "checkbox size");
	check(ReadPixel(win->RPort, g->LeftEdge, g->TopEdge + 3) ==
	    PEN(SHINEPEN) && ReadPixel(win->RPort, g->LeftEdge + g->Width - 1,
	    g->TopEdge + 3) == PEN(SHADOWPEN), "checkbox bevel");

	/* string: the ridge around the field */
	g = gads[G_STRING];
	l = g->LeftEdge - 4;
	t = g->TopEdge - 2;
	r = g->LeftEdge + g->Width + 3;
	b = g->TopEdge + g->Height + 1;
	check(ReadPixel(win->RPort, l, t + 4) == PEN(SHINEPEN) &&
	    ReadPixel(win->RPort, r, t + 4) == PEN(SHADOWPEN) &&
	    ReadPixel(win->RPort, l + 1, t + 4) == PEN(SHADOWPEN) &&
	    ReadPixel(win->RPort, r - 1, t + 4) == PEN(SHINEPEN), "string ridge");
	check(count(PEN(TEXTPEN), g->LeftEdge, g->TopEdge, g->LeftEdge + 30,
	    g->TopEdge + g->Height - 1) > 10, "string text");
	check(count(PEN(TEXTPEN), l - 50, t, l - 1, b) > 15, "string label");

	/* text: recessed border, the text inside */
	g = gads[G_TEXT];
	check(ReadPixel(win->RPort, g->LeftEdge, g->TopEdge + 3) ==
	    PEN(SHADOWPEN) && ReadPixel(win->RPort, g->LeftEdge + g->Width - 1,
	    g->TopEdge + 3) == PEN(SHINEPEN), "text border");
	check(count(PEN(TEXTPEN), g->LeftEdge + 2, g->TopEdge + 2,
	    g->LeftEdge + 50, g->TopEdge + g->Height - 3) > 15, "text");
	check(count(PEN(TEXTPEN), g->LeftEdge + 2, gads[G_NUMBER]->TopEdge + 2,
	    g->LeftEdge + 78, gads[G_NUMBER]->TopEdge +
	    gads[G_NUMBER]->Height - 3) > 8, "number");

	/* cycle: bevel and its first label */
	g = gads[G_CYCLE];
	check(ReadPixel(win->RPort, g->LeftEdge, g->TopEdge + 3) ==
	    PEN(SHINEPEN) && ReadPixel(win->RPort, g->LeftEdge + g->Width - 1,
	    g->TopEdge + 3) == PEN(SHADOWPEN), "cycle bevel");
	check(count(PEN(TEXTPEN), g->LeftEdge + 20, g->TopEdge + 2,
	    g->LeftEdge + g->Width - 3, g->TopEdge + g->Height - 3) > 10,
	    "cycle label");

	/* MX: the labels right of the ticks */
	g = gads[G_MX];
	check(count(PEN(TEXTPEN), g->LeftEdge + g->Width + 2, g->TopEdge,
	    g->LeftEdge + g->Width + 50, g->TopEdge + g->Height - 1) > 30,
	    "MX labels");

	/* listview: the label above, the list in its bevel */
	g = gads[G_LISTVIEW];
	check(count(PEN(TEXTPEN), g->LeftEdge, g->TopEdge - 12, g->LeftEdge +
	    g->Width - 1, g->TopEdge - 2) > 10, "listview label");
	check(count(PEN(TEXTPEN), g->LeftEdge + 4, g->TopEdge + 3,
	    g->LeftEdge + 40, g->TopEdge + 30) > 30, "listview items");

	/* DrawBevelBox: recessed */
	DrawBevelBox(win->RPort, 300, 170, 60, 20, GT_VisualInfo, vi,
	    GTBB_Recessed, (ULONG)TRUE, TAG_DONE);
	check(ReadPixel(win->RPort, 300, 180) == PEN(SHADOWPEN) &&
	    ReadPixel(win->RPort, 359, 180) == PEN(SHINEPEN) &&
	    ReadPixel(win->RPort, 330, 170) == PEN(SHADOWPEN) &&
	    ReadPixel(win->RPort, 330, 189) == PEN(SHINEPEN), "DrawBevelBox");
}

static LONG getattr(UWORD id, Tag tag)
{
	LONG v = -1;

	GT_GetGadgetAttrs(gads[id], win, NULL, tag, &v, TAG_DONE);
	return v;
}

static void setattr(UWORD id, Tag tag, IPTR data)
{
	GT_SetGadgetAttrs(gads[id], win, NULL, tag, data, TAG_DONE);
}

static int strcmp(const char *a, const char *b)
{
	while (*a && *a == *b)
		a++, b++;
	return *a - *b;
}

/* Clicks and keys, and what GT_GetIMsg reports */
static void input(void)
{
	struct IntuiMessage *im, *gm, *held[8];
	struct Gadget *g, *h, *a;
	BOOL ok;
	WORD n;
	STRPTR s;
	WORD x;
	LONG tot, vis;

	flush();

	g = gads[G_BUTTON];
	click(GX(g, 10), GY(g, 5));
	check(gtmsg(IDCMP_GADGETUP) && msggad == g, "button GADGETUP");

	g = gads[G_CHECKBOX];
	click(GX(g, 5), GY(g, 5));
	check(gtmsg(IDCMP_GADGETUP) && msggad == g && msgcode == TRUE,
	    "checkbox GADGETUP");
	check(getattr(G_CHECKBOX, GTCB_Checked) == TRUE, "GTCB_Checked");
	check(count(PEN(TEXTPEN), g->LeftEdge + 4, g->TopEdge + 2,
	    g->LeftEdge + g->Width - 5, g->TopEdge + g->Height - 3) > 5,
	    "checkmark");

	g = gads[G_CYCLE];
	click(GX(g, 30), GY(g, 5));
	check(gtmsg(IDCMP_GADGETUP) && msggad == g && msgcode == 1,
	    "cycle GADGETUP");
	check(getattr(G_CYCLE, GTCY_Active) == 1, "GTCY_Active");

	/* the same through GT_FilterIMsg and GT_PostFilterIMsg */
	click(GX(g, 30), GY(g, 5));
	ok = FALSE;
	while ((im = (struct IntuiMessage *)GetMsg(win->UserPort)) != NULL) {
		gm = GT_FilterIMsg(im);
		if (gm && gm->Class == IDCMP_GADGETUP)
			ok = gm->Code == 2 && gm->IAddress == g;
		if (gm)
			check(GT_PostFilterIMsg(gm) == im, "GT_PostFilterIMsg");
		ReplyMsg((struct Message *)im);
	}
	check(ok, "GT_FilterIMsg");

	/* two messages held at once: checkbox off and on again */
	g = gads[G_CHECKBOX];
	click(GX(g, 5), GY(g, 5));
	click(GX(g, 5), GY(g, 5));
	n = 0;
	while (n < 8 && (held[n] = GT_GetIMsg(win->UserPort)) != NULL)
		if (held[n]->Class == IDCMP_GADGETUP)
			n++;
		else
			GT_ReplyIMsg(held[n]);
	check(n == 2 && held[0]->Code == FALSE && held[1]->Code == TRUE &&
	    held[0]->IAddress == g, "held messages");
	while (n > 0)
		GT_ReplyIMsg(held[--n]);

	g = gads[G_MX];
	click(GX(g, 5), GY(g, g->Height / 2));
	check(gtmsg(IDCMP_GADGETDOWN) && msggad == g && msgcode == 1,
	    "MX GADGETDOWN");
	check(getattr(G_MX, GTMX_Active) == 1, "GTMX_Active");

	/* slider: a click right of the knob steps up */
	g = gads[G_SLIDER];
	click(GX(g, g->Width - 3), GY(g, 5));
	check(gtmsg(IDCMP_GADGETUP) && msggad == g && msgcode == 4,
	    "slider GADGETUP");
	flush();
	check(getattr(G_SLIDER, GTSL_Level) == 4, "GTSL_Level");

	/* scroller: the down arrow, then a click in the container */
	g = gads[G_SCROLLER];
	x = g->Width + 16 + 8;
	click(GX(g, x), GY(g, 5));
	check(gtmsg(IDCMP_GADGETDOWN) && msggad == g && msgcode == 1,
	    "scroller arrow");
	flush();
	click(GX(g, g->Width - 4), GY(g, 5));
	check(gtmsg(IDCMP_GADGETUP) && msggad == g && msgcode > 1,
	    "scroller GADGETUP");
	check(getattr(G_SCROLLER, GTSC_Top) == msgcode, "GTSC_Top");

	/* listview: the third line */
	g = gads[G_LISTVIEW];
	click(GX(g, 20), GY(g, 4 + 2 * dri->dri_Font->tf_YSize + 2));
	check(gtmsg(IDCMP_GADGETUP) && msggad == g && msgcode == 2,
	    "listview GADGETUP");
	check(getattr(G_LISTVIEW, GTLV_Selected) == 2, "GTLV_Selected");

	/* its down arrow, the lowest gadget right of it, scrolls by one */
	a = NULL;
	for (h = win->FirstGadget; h; h = h->NextGadget)
		if (h->LeftEdge > g->LeftEdge + g->Width && h->TopEdge > g->TopEdge &&
		    h->TopEdge < g->TopEdge + g->Height &&
		    (a == NULL || h->TopEdge > a->TopEdge))
			a = h;
	check(a != NULL, "listview arrow");
	if (a) {
		click(GX(a, 4), GY(a, 4));
		flush();
		check(getattr(G_LISTVIEW, GTLV_Top) == 1, "listview scrolled");
	}
	setattr(G_LISTVIEW, GTLV_MakeVisible, NLV - 1);
	check(getattr(G_LISTVIEW, GTLV_Top) > 1, "GTLV_MakeVisible");

	/* a GENERIC_KIND gadget reaches the program unchanged */
	g = gads[G_GENERIC];
	check((g->GadgetType & GTYP_GADTOOLS) &&
	    (gads[G_BUTTON]->GadgetType & GTYP_GADTOOLS), "GTYP_GADTOOLS");
	click(GX(g, 10), GY(g, 5));
	check(gtmsg(IDCMP_GADGETUP) && msggad == g, "generic GADGETUP");

	/* palette: colour 0, right of the indicator */
	g = gads[G_PALETTE];
	click(GX(g, 30), GY(g, g->Height / 2));
	check(gtmsg(IDCMP_GADGETUP) && msggad == g && msgcode == 0,
	    "palette GADGETUP");
	check(getattr(G_PALETTE, GTPA_Color) == 0, "GTPA_Color");

	/* string: append "xy" */
	g = gads[G_STRING];
	click(GX(g, g->Width - 4), GY(g, 5));
	key(RAWKEY_X);
	key(RAWKEY_Y);
	key(RAWKEY_RETURN);
	check(gtmsg(IDCMP_GADGETUP) && msggad == g, "string GADGETUP");
	s = (STRPTR)getattr(G_STRING, GTST_String);
	check(s != (STRPTR)-1 && strcmp(s, "abcxy") == 0, "GTST_String");

	/* integer: 7, then "12" */
	g = gads[G_INTEGER];
	click(GX(g, g->Width - 4), GY(g, 5));
	key(RAWKEY_1);
	key(RAWKEY_2);
	key(RAWKEY_RETURN);
	check(gtmsg(IDCMP_GADGETUP) && msggad == g, "integer GADGETUP");
	check(getattr(G_INTEGER, GTIN_Number) == 712, "GTIN_Number");

	/* the read-only kinds, set from the program */
	setattr(G_NUMBER, GTNM_Number, 1234);
	check(getattr(G_NUMBER, GTNM_Number) == 1234, "GTNM_Number");
	setattr(G_TEXT, GTTX_Text, (IPTR)"bye");
	s = (STRPTR)getattr(G_TEXT, GTTX_Text);
	check(s != (STRPTR)-1 && strcmp(s, "bye") == 0, "GTTX_Text");
	setattr(G_SLIDER, GTSL_Level, 9);
	check(getattr(G_SLIDER, GTSL_Level) == 9, "GT_SetGadgetAttrs");
	tot = vis = -1;
	check(GT_GetGadgetAttrs(gads[G_SCROLLER], win, NULL, GTSC_Total, &tot,
	    GTSC_Visible, &vis, TAG_DONE) == 2 && tot == 20 && vis == 5,
	    "GT_GetGadgetAttrs, two tags");

	/* a disabled button sends nothing */
	setattr(G_BUTTON, GA_Disabled, TRUE);
	g = gads[G_BUTTON];
	click(GX(g, 10), GY(g, 5));
	check(!gtmsg(IDCMP_GADGETUP), "disabled button");
	setattr(G_BUTTON, GA_Disabled, FALSE);
}

/* Press the menu button on a title, move to an item, release */
static UWORD pick(struct Menu *m, struct MenuItem *it, struct MenuItem *sub)
{
	WORD x, y, tx;
	int n;

	tx = m->LeftEdge + 4;
	mouse(tx, 2);
	event(IECLASS_RAWMOUSE, IECODE_RBUTTON, tx, 2);
	TimeDelay(UNIT_VBLANK, 0, 60000);
	x = m->LeftEdge + it->LeftEdge + it->Width / 2;
	y = scr->BarHeight + 1 + it->TopEdge + it->Height / 2;
	mouse(x, y);
	mouse(x + 1, y);
	TimeDelay(UNIT_VBLANK, 0, 100000);
	if (sub) {
		/* along the item into its sub-menu */
		x = m->LeftEdge + it->LeftEdge + it->Width - 4;
		mouse(x, y);
		TimeDelay(UNIT_VBLANK, 0, 100000);
		x = m->LeftEdge + it->LeftEdge + sub->LeftEdge + sub->Width / 2;
		y = scr->BarHeight + 1 + it->TopEdge + sub->TopEdge +
		    sub->Height / 2;
		mouse(x, y);
		mouse(x + 1, y);
		TimeDelay(UNIT_VBLANK, 0, 100000);
	}
	if (m == menus && it->NextItem == NULL) {
		/* the title bar shows the menu titles while the button is down */
		n = 0;
		for (x = m->LeftEdge; x < m->LeftEdge + m->Width; x++)
			n += ReadPixel(&scr->RastPort, x, scr->BarHeight / 2) !=
			    ReadPixel(&scr->RastPort, m->LeftEdge, 1);
		check(n > 10, "menu title");
	}
	event(IECLASS_RAWMOUSE, IECODE_RBUTTON | IECODE_UP_PREFIX, x, y);
	if (!gtlate(IDCMP_MENUPICK))
		return MENUNULL;
	return msgcode;
}

static void menuchecks(void)
{
	struct MenuItem *it, *quit, *toggle;
	UWORD code;
	WORD top;

	/* Project: Open, a bar, Quit, with the user data after each */
	it = menus->FirstItem;
	check(it && GTMENUITEM_USERDATA(it) == (APTR)11, "menu user data");
	check(GTMENU_USERDATA(menus) == NULL, "menu title user data");
	quit = it && it->NextItem ? it->NextItem->NextItem : NULL;
	check(quit != NULL && GTMENUITEM_USERDATA(quit) == (APTR)12 &&
	    (quit->Flags & COMMSEQ) && quit->Command == 'Q', "Quit item");
	check(it->NextItem && !(it->NextItem->Flags & ITEMTEXT) &&
	    it->NextItem->Height < it->Height, "bar label");
	check(quit && quit->TopEdge > it->TopEdge + it->Height,
	    "LayoutMenus item positions");
	check(menus->NextMenu && menus->NextMenu->LeftEdge >
	    menus->LeftEdge + menus->Width - 8, "LayoutMenus title positions");
	toggle = menus->NextMenu ? menus->NextMenu->FirstItem->NextItem : NULL;
	check(toggle && (toggle->Flags & CHECKIT) && (toggle->Flags & MENUTOGGLE),
	    "CHECKIT item");
	check(toggle && toggle->NextItem && toggle->NextItem->SubItem &&
	    toggle->NextItem->SubItem->NextItem &&
	    GTMENUITEM_USERDATA(toggle->NextItem->SubItem->NextItem) ==
	    (APTR)232, "sub-items");
	if (quit == NULL || toggle == NULL)
		return;
	top = quit->TopEdge;
	check(LayoutMenuItems(menus->FirstItem, vi, GTMN_Menu, menus, TAG_DONE) &&
	    quit->TopEdge == top, "LayoutMenuItems");

	flush();
	code = pick(menus, quit, NULL);
	check(code != MENUNULL && MENUNUM(code) == 0 && ITEMNUM(code) == 2,
	    "MENUPICK Quit");
	check(code != MENUNULL &&
	    GTMENUITEM_USERDATA(ItemAddress(menus, code)) == (APTR)12,
	    "ItemAddress user data");
	code = pick(menus->NextMenu, toggle, NULL);
	check(code != MENUNULL && MENUNUM(code) == 1 && ITEMNUM(code) == 1 &&
	    (toggle->Flags & CHECKED), "MENUPICK toggle");
	code = pick(menus->NextMenu, toggle->NextItem,
	    toggle->NextItem->SubItem->NextItem);
	check(code != MENUNULL && MENUNUM(code) == 1 && ITEMNUM(code) == 2 &&
	    SUBNUM(code) == 1, "MENUPICK sub-item");
}

static void teardown(void)
{
	if (win) {
		ClearMenuStrip(win);
		CloseWindow(win);
		win = NULL;
	}
	FreeMenus(menus);
	menus = NULL;
	FreeGadgets(glist);
	glist = NULL;
	FreeVisualInfo(vi);
	vi = NULL;
}

/* A window of every kind on scr */
static void gadgets(char *name)
{
	look = name;
	dri = GetScreenDrawInfo(scr);
	if (build() && openwin()) {
		pixels();
		input();
		menuchecks();
		/* refreshing after damage */
		GT_BeginRefresh(win);
		GT_EndRefresh(win, TRUE);
	}
	teardown();
	FreeScreenDrawInfo(scr, dri);
}

static struct TagItem chunkytags[] = {
	{ BMATags_GfxFormat, GDF_CHUNKY4 }, { TAG_DONE, 0 }
};

static struct Screen *openscreen(WORD w, WORD h, WORD d, struct BitMap *bm,
    char *title)
{
	struct TagItem t[7];

	t[0].ti_Tag = SA_Width;		t[0].ti_Data = w;
	t[1].ti_Tag = SA_Height;	t[1].ti_Data = h;
	t[2].ti_Tag = SA_Depth;		t[2].ti_Data = d;
	t[3].ti_Tag = SA_Title;		t[3].ti_Data = (IPTR)title;
	t[4].ti_Tag = SA_Type;		t[4].ti_Data = CUSTOMSCREEN;
	t[5].ti_Tag = bm ? SA_BitMap : TAG_IGNORE; t[5].ti_Data = (IPTR)bm;
	t[6].ti_Tag = TAG_DONE;
	return OpenScreenTagList(NULL, t);
}

static void colourlook(char *name)
{
	struct BitMap *bm;

	bm = AllocBitMap(640, 400, 4, BMF_CLEAR | BMF_RTGTAGS | BMF_RTGCHECK |
	    BMF_FRIENDISTAG, (struct BitMap *)chunkytags);
	look = name;
	check(bm != NULL, "chunky bitmap");
	if (bm == NULL)
		return;
	scr = openscreen(640, 400, 4, bm, name);
	check(scr != NULL, "screen");
	if (scr) {
		gadgets(name);
		CloseScreen(scr);
	}
	FreeBitMap(bm);
}

/* The task's stack: the pool clears a new stack; count above the last zero */
static void stackuse(char *name)
{
	struct Task *t;
	UBYTE *p, *top;

	t = FindTask(name);
	if (t == NULL)
		return;
	p = (UBYTE *)t->tc_SPLower + 2;
	top = (UBYTE *)t->tc_SPUpper;
	while (p < top && *p == 0)
		p++;
	xKPrintf("gadtools: %s stack %d of %d bytes used\n", name,
	    (int)(top - p), (int)(top - (UBYTE *)t->tc_SPLower));
}

int main(void)
{
	static struct Preferences saved, prefs;

	port = CreateMsgPort();
	io = (struct IOStdReq *)CreateIORequest(port, sizeof(struct IOStdReq));
	if (OpenDevice("input.device", 0, (struct IORequest *)io, 0) != 0) {
		KPutStr("FAIL gadtools: input.device\n");
		return 1;
	}
	GfxBase = (struct GfxBase *)OpenLibrary(GRAPHICSNAME, 39L);
	IntuitionBase = (struct IntuitionBase *)OpenLibrary(INTUITIONNAME, 39L);
	GadToolsBase = OpenLibrary(GADTOOLSNAME, 39L);
	if (GfxBase == NULL || IntuitionBase == NULL || GadToolsBase == NULL) {
		KPutStr("FAIL gadtools: OpenLibrary\n");
		return 1;
	}

	look = "mono";
	scr = openscreen(1024, 800, 1, NULL, "Mono");
	check(scr != NULL, "screen");
	if (scr) {
		gadgets("mono");
		CloseScreen(scr);
	}

	colourlook("colour");

	GetPrefs(&saved, sizeof(saved));
	prefs = saved;
	prefs.color0 = 0x05A;
	prefs.color1 = 0xFFF;
	prefs.color2 = 0x002;
	prefs.color3 = 0xF80;
	prefs.EnableCLI |= OLD_LOOK;
	SetPrefs(&prefs, sizeof(prefs), FALSE);
	colourlook("1.x");
	SetPrefs(&saved, sizeof(saved), FALSE);

	stackuse("input.device");
	stackuse("Intuition menu handler");
	CloseLibrary(GadToolsBase);
	CloseLibrary((struct Library *)IntuitionBase);
	CloseLibrary((struct Library *)GfxBase);
	CloseDevice((struct IORequest *)io);
	if (fails != 0)
		return 1;
	KPutStr("PASS gadtools\n");
	return 0;
}
