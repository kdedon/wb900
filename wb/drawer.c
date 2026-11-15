/* SPDX-License-Identifier: MIT */
/*
 * Workbench's windows: the backdrop with the disks and the drawer
 * windows, their icons, and drawing them.  Icons are kept in their
 * window's list in drawing order; a window's inner area starts at 0, 0
 * (drawer windows are GimmeZeroZero, the backdrop has no border).  The
 * backdrop has the disks and the AppIcons, and drawer windows scroll.
 */
#include "workbench.h"

#define IDCMPFLAGS (IDCMP_CLOSEWINDOW | IDCMP_MOUSEBUTTONS | \
	IDCMP_MOUSEMOVE | IDCMP_MENUPICK | IDCMP_REFRESHWINDOW | \
	IDCMP_NEWSIZE | IDCMP_ACTIVEWINDOW | IDCMP_GADGETDOWN | IDCMP_GADGETUP)

/* Columns of the icon grid */
#define COLW	72
#define GAP	6

static struct TagItem drawtags[] = {
	{ ICONDRAWA_Frameless, TRUE }, { ICONDRAWA_EraseBackground, FALSE },
	{ TAG_DONE, 0 }
};

static STRPTR dupstr(CONST_STRPTR s)
{
	STRPTR d = AllocVec(strlen(s) + 1, MEMF_ANY);

	if (d != NULL)
		strcpy(d, s);
	return d;
}

/* The label: the name, without a volume's colon */
static void label(struct WBIcon *in, char *buf)
{
	WORD n;

	strcpy(buf, in->in_Name);
	n = strlen(buf);
	if (n > 0 && buf[n - 1] == ':')
		buf[n - 1] = 0;
}

static struct WBIcon *newicon(struct WBWin *ww, CONST_STRPTR name,
    struct DiskObject *dobj)
{
	struct WBIcon *in;
	char lbl[NAMELEN];

	if (dobj == NULL || (in = AllocVec(sizeof(*in), MEMF_CLEAR)) == NULL) {
		FreeDiskObject(dobj);
		return NULL;
	}
	in->in_Obj = dobj;
	strncpy(in->in_Name, name, NAMELEN - 1);
	label(in, lbl);
	GetIconRectangleA(ww->ww_Win->RPort, dobj, lbl, &in->in_Box, drawtags);
	in->in_X = dobj->do_CurrentX;
	in->in_Y = dobj->do_CurrentY;
	AddTail((struct List *)&ww->ww_Icons, (struct Node *)in);
	return in;
}

void freeicons(struct WBWin *ww)
{
	struct WBIcon *in;

	while ((in = (struct WBIcon *)RemHead((struct List *)&ww->ww_Icons))
	    != NULL) {
		FreeDiskObject(in->in_Obj);
		FreeVec(in);
	}
}

/* The window's icons in rows, or for the backdrop in a column on the
   right; all of them, or only those without a position */
static void arrange(struct WBWin *ww, BOOL all)
{
	struct Window *w = ww->ww_Win;
	WORD iw = w->GZZWidth, x = GAP, y = GAP, rowh = 0, bw, bh;
	struct WBIcon *in;

	FOREACH(struct WBIcon *, in, &ww->ww_Icons) {
		if (!all && in->in_Obj->do_CurrentX != NO_ICON_POSITION &&
		    in->in_Obj->do_CurrentY != NO_ICON_POSITION)
			continue;
		bw = in->in_Box.MaxX - in->in_Box.MinX + 1;
		bh = in->in_Box.MaxY - in->in_Box.MinY + 1;
		if (ww == root) {
			in->in_X = iw - GAP - COLW / 2 - bw / 2 - in->in_Box.MinX;
			in->in_Y = y;
			y += bh + GAP;
			continue;
		}
		if (x + bw > iw && x > GAP) {
			x = GAP;
			y += rowh + GAP;
			rowh = 0;
		}
		in->in_X = x - in->in_Box.MinX;
		in->in_Y = y;
		x += (bw > COLW ? bw : COLW) + GAP;
		if (bh > rowh)
			rowh = bh;
	}
}

/* A directory entry and whether it has an icon */
struct entry {
	struct entry *e_Next;
	BOOL	 e_Info;
	char	 e_Name[NAMELEN];
};

static BOOL isinfo(char *name)
{
	WORD n = strlen(name);

	return n > 5 && Stricmp(name + n - 5, ".info") == 0;
}

/* The entry of name other than not */
static struct entry *findentry(struct entry *l, char *name, struct entry *not)
{
	for (; l != NULL; l = l->e_Next)
		if (l != not && Stricmp(l->e_Name, name) == 0)
			return l;
	return NULL;
}

/* The objects of a drawer: those with an icon, or all of them */
static void scandir(struct WBWin *ww)
{
	struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
	struct entry *list = NULL, *e, *f;
	char path[PATHLEN];
	BPTR lock;
	WORD n;

	if (fib == NULL)
		return;
	if ((lock = Lock(ww->ww_Path, ACCESS_READ)) != NULL &&
	    Examine(lock, fib)) {
		while (ExNext(lock, fib)) {
			if ((e = AllocVec(sizeof(*e), MEMF_CLEAR)) == NULL)
				break;
			strncpy(e->e_Name, fib->fib_FileName, NAMELEN - 1);
			e->e_Next = list;
			list = e;
		}
	}
	UnLock(lock);
	FreeDosObject(DOS_FIB, fib);

	/* An object's icon file marks it; Disk.info is the volume's */
	for (e = list; e != NULL; e = e->e_Next) {
		if (!isinfo(e->e_Name))
			continue;
		n = strlen(e->e_Name) - 5;
		e->e_Name[n] = 0;
		if ((f = findentry(list, e->e_Name, e)) != NULL) {
			f->e_Info = TRUE;
			e->e_Name[0] = 0;
		} else if (Stricmp(e->e_Name, "Disk") == 0 &&
		    ww->ww_Path[strlen(ww->ww_Path) - 1] == ':') {
			e->e_Name[0] = 0;
		} else {
			e->e_Info = TRUE;	/* an icon without its object */
		}
	}
	while ((e = list) != NULL) {
		list = e->e_Next;
		if (e->e_Name[0] && (e->e_Info || ww->ww_ShowAll)) {
			strcpy(path, ww->ww_Path);
			AddPart(path, e->e_Name, PATHLEN);
			newicon(ww, e->e_Name, e->e_Info ? GetDiskObject(path) :
			    GetDiskObjectNew(path));
		}
		FreeVec(e);
	}
}

/* The disks: a device that is a file system shows its volume */
static void scanvolumes(struct WBWin *ww)
{
	char names[8][NAMELEN], path[PATHLEN];
	struct DosList *dl;
	struct DiskObject *dobj;
	WORD n = 0, i;
	BPTR lock;

	dl = LockDosList(LDF_DEVICES | LDF_READ);
	while (n < 8 && (dl = NextDosEntry(dl, LDF_DEVICES)) != NULL) {
		/* the console is no file system; asking would start it */
		if (Stricmp(dl->dol_Name, "CON") == 0)
			continue;
		strncpy(names[n], dl->dol_Name, NAMELEN - 2);
		strcat(names[n++], ":");
	}
	UnLockDosList(LDF_DEVICES | LDF_READ);

	for (i = 0; i < n; i++) {
		if (!IsFileSystem(names[i]) ||
		    (lock = Lock(names[i], ACCESS_READ)) == NULL)
			continue;
		if (NameFromLock(lock, path, NAMELEN) &&
		    (dobj = GetDiskObjectNew(path)) != NULL) {
			dobj->do_Type = WBDISK;
			newicon(ww, path, dobj);
		}
		UnLock(lock);
	}
	appicons();
}

void scan(struct WBWin *ww)
{
	if (ww == root)
		scanvolumes(ww);
	else
		scandir(ww);
	arrange(ww, FALSE);
}

void cleanup(struct WBWin *ww)
{
	ww->ww_OffX = ww->ww_OffY = 0;
	arrange(ww, TRUE);
	redraw(ww);
}

/* Prop gadgets in the right and bottom borders */
static void addscrollers(struct WBWin *ww)
{
	struct Window *w = ww->ww_Win;
	struct Gadget *g;
	WORD i;

	for (i = HSCROLL; i <= VSCROLL; i++) {
		g = &ww->ww_Scroll[i];
		g->GadgetType = GTYP_PROPGADGET | GTYP_GZZGADGET;
		g->Activation = GACT_IMMEDIATE | GACT_RELVERIFY | GACT_FOLLOWMOUSE;
		g->Flags = GFLG_GADGHNONE;
		g->GadgetRender = (APTR)&ww->ww_Knob[i];
		g->SpecialInfo = (APTR)&ww->ww_Prop[i];
		ww->ww_Prop[i].Flags = AUTOKNOB | PROPNEWLOOK |
		    (i == HSCROLL ? FREEHORIZ : FREEVERT);
		ww->ww_Prop[i].HorizBody = ww->ww_Prop[i].VertBody = MAXBODY;
	}
	g = &ww->ww_Scroll[HSCROLL];
	g->NextGadget = &ww->ww_Scroll[VSCROLL];
	g->Activation |= GACT_BOTTOMBORDER;
	g->Flags |= GFLG_RELBOTTOM | GFLG_RELWIDTH;
	g->LeftEdge = w->BorderLeft;
	g->TopEdge = 3 - w->BorderBottom;
	g->Width = -w->BorderLeft - w->BorderRight - 2;
	g->Height = w->BorderBottom - 4;
	g = &ww->ww_Scroll[VSCROLL];
	g->Activation |= GACT_RIGHTBORDER;
	g->Flags |= GFLG_RELRIGHT | GFLG_RELHEIGHT;
	g->LeftEdge = 4 - w->BorderRight;
	g->TopEdge = w->BorderTop + 1;
	g->Width = w->BorderRight - 6;
	g->Height = -w->BorderTop - w->BorderBottom - 2;
	AddGList(w, &ww->ww_Scroll[HSCROLL], -1, 2, NULL);
	RefreshGList(&ww->ww_Scroll[HSCROLL], w, NULL, 2);
}

struct WBWin *openwin(STRPTR path, struct DiskObject *dobj)
{
	struct WBWin *ww;
	struct NewWindow *nw = NULL;
	struct TagItem t[22];
	WORD i = 0;

	if ((ww = AllocVec(sizeof(*ww), MEMF_CLEAR)) == NULL)
		return NULL;
	NewList((struct List *)&ww->ww_Icons);
	if (path != NULL && (ww->ww_Path = dupstr(path)) == NULL) {
		FreeVec(ww);
		return NULL;
	}
	if (path == NULL && wbbackdrop) {
		root = ww;
		t[i].ti_Tag = WA_Top;		t[i++].ti_Data = scr->BarHeight + 1;
		t[i].ti_Tag = WA_Width;		t[i++].ti_Data = scr->Width;
		t[i].ti_Tag = WA_Height;
		t[i++].ti_Data = scr->Height - scr->BarHeight - 1;
		t[i].ti_Tag = WA_Backdrop;	t[i++].ti_Data = TRUE;
		t[i].ti_Tag = WA_Borderless;	t[i++].ti_Data = TRUE;
		/* the screen's background */
		t[i].ti_Tag = WA_BackFill;
		t[i++].ti_Data = (IPTR)scr->LayerInfo.BlankHook;
	} else if (path == NULL) {
		/* the disks in a window of their own */
		root = ww;
		strcpy(ww->ww_Title, "Workbench");
		t[i].ti_Tag = WA_Top;		t[i++].ti_Data = scr->BarHeight + 1;
		t[i].ti_Tag = WA_Width;		t[i++].ti_Data = scr->Width / 2;
		t[i].ti_Tag = WA_Height;	t[i++].ti_Data = scr->Height / 2;
		t[i].ti_Tag = WA_Title;		t[i++].ti_Data = (IPTR)ww->ww_Title;
		t[i].ti_Tag = WA_DragBar;	t[i++].ti_Data = TRUE;
		t[i].ti_Tag = WA_DepthGadget;	t[i++].ti_Data = TRUE;
		t[i].ti_Tag = WA_SizeGadget;	t[i++].ti_Data = TRUE;
		t[i].ti_Tag = WA_SizeBRight;	t[i++].ti_Data = TRUE;
		t[i].ti_Tag = WA_SizeBBottom;	t[i++].ti_Data = TRUE;
		t[i].ti_Tag = WA_GimmeZeroZero;	t[i++].ti_Data = TRUE;
	} else {
		strncpy(ww->ww_Title, FilePart(path), NAMELEN - 1);
		if (ww->ww_Title[0] == 0) {
			/* a volume: its name */
			strncpy(ww->ww_Title, path, NAMELEN - 1);
			ww->ww_Title[strlen(ww->ww_Title) - 1] = 0;
		}
		if (dobj != NULL && dobj->do_DrawerData != NULL) {
			nw = &dobj->do_DrawerData->dd_NewWindow;
			ww->ww_ShowAll = dobj->do_DrawerData->dd_Flags ==
			    DDFLAGS_SHOWALL;
			ww->ww_OffX = dobj->do_DrawerData->dd_CurrentX;
			ww->ww_OffY = dobj->do_DrawerData->dd_CurrentY;
		}
		t[i].ti_Tag = WA_Left;
		t[i++].ti_Data = nw && nw->LeftEdge < scr->Width - 80 ?
		    nw->LeftEdge : 40;
		t[i].ti_Tag = WA_Top;
		t[i++].ti_Data = nw && nw->TopEdge < scr->Height - 40 ?
		    nw->TopEdge : 40;
		t[i].ti_Tag = WA_Width;
		t[i++].ti_Data = nw && nw->Width >= 80 ? nw->Width : 360;
		t[i].ti_Tag = WA_Height;
		t[i++].ti_Data = nw && nw->Height >= 40 ? nw->Height : 160;
		t[i].ti_Tag = WA_Title;		t[i++].ti_Data = (IPTR)ww->ww_Title;
		t[i].ti_Tag = WA_CloseGadget;	t[i++].ti_Data = TRUE;
		t[i].ti_Tag = WA_DragBar;	t[i++].ti_Data = TRUE;
		t[i].ti_Tag = WA_DepthGadget;	t[i++].ti_Data = TRUE;
		t[i].ti_Tag = WA_SizeGadget;	t[i++].ti_Data = TRUE;
		t[i].ti_Tag = WA_SizeBRight;	t[i++].ti_Data = TRUE;
		t[i].ti_Tag = WA_SizeBBottom;	t[i++].ti_Data = TRUE;
		t[i].ti_Tag = WA_GimmeZeroZero;	t[i++].ti_Data = TRUE;
		t[i].ti_Tag = WA_Activate;	t[i++].ti_Data = TRUE;
	}
	if (ww != root || !wbbackdrop) {
		t[i].ti_Tag = WA_MinWidth;	t[i++].ti_Data = 96;
		t[i].ti_Tag = WA_MinHeight;	t[i++].ti_Data = 64;
		t[i].ti_Tag = WA_MaxWidth;	t[i++].ti_Data = (ULONG)~0;
		t[i].ti_Tag = WA_MaxHeight;	t[i++].ti_Data = (ULONG)~0;
	}
	t[i].ti_Tag = WA_CustomScreen;	t[i++].ti_Data = (IPTR)scr;
	t[i].ti_Tag = WA_SimpleRefresh;	t[i++].ti_Data = TRUE;
	t[i].ti_Tag = WA_ReportMouse;	t[i++].ti_Data = TRUE;
	t[i].ti_Tag = WA_NewLookMenus;	t[i++].ti_Data = TRUE;
	t[i].ti_Tag = TAG_DONE;
	if ((ww->ww_Win = OpenWindowTagList(NULL, t)) == NULL) {
		if (ww == root)
			root = NULL;
		FreeVec(ww->ww_Path);
		FreeVec(ww);
		return NULL;
	}
	ww->ww_Win->UserPort = idport;
	ModifyIDCMP(ww->ww_Win, IDCMPFLAGS);
	if (!(ww->ww_Win->Flags & WFLG_BACKDROP))
		addscrollers(ww);
	windowmenus(ww->ww_Win);
	AddTail((struct List *)&wins, (struct Node *)ww);
	scan(ww);
	redraw(ww);
	return ww;
}

void closewin(struct WBWin *ww)
{
	struct Window *w = ww->ww_Win;
	struct IntuiMessage *im, *succ;

	freeicons(ww);
	/* the shared port's messages for the window go first */
	Forbid();
	for (im = (struct IntuiMessage *)idport->mp_MsgList.lh_Head;
	    (succ = (struct IntuiMessage *)im->ExecMessage.mn_Node.ln_Succ)
	    != NULL; im = succ) {
		if (im->IDCMPWindow == w) {
			Remove((struct Node *)im);
			ReplyMsg((struct Message *)im);
		}
	}
	w->UserPort = NULL;
	ModifyIDCMP(w, 0);
	Permit();
	ClearMenuStrip(w);
	CloseWindow(w);
	Remove((struct Node *)ww);
	if (ww == root)
		root = NULL;
	FreeVec(ww->ww_Path);
	FreeVec(ww);
}

struct WBWin *findwin(struct Window *w)
{
	struct WBWin *ww;

	FOREACHWIN(ww)
		if (ww->ww_Win == w)
			return ww;
	return NULL;
}

struct WBWin *findpath(STRPTR path)
{
	struct WBWin *ww;

	FOREACHWIN(ww)
		if (ww->ww_Path != NULL && Stricmp(ww->ww_Path, path) == 0)
			return ww;
	return NULL;
}

BOOL fullpath(struct WBWin *ww, struct WBIcon *in, STRPTR buf)
{
	if (ww == root) {
		strcpy(buf, in->in_Name);
		return TRUE;
	}
	strcpy(buf, ww->ww_Path);
	return AddPart(buf, in->in_Name, PATHLEN);
}

void drawicon(struct WBWin *ww, struct WBIcon *in)
{
	char lbl[NAMELEN];

	label(in, lbl);
	DrawIconStateA(ww->ww_Win->RPort, in->in_Obj, lbl,
	    in->in_X - ww->ww_OffX, in->in_Y - ww->ww_OffY,
	    in->in_Selected ? IDS_SELECTED : IDS_NORMAL, drawtags);
}

/* The icon's box in the window */
static void box(struct WBWin *ww, struct WBIcon *in, struct Rectangle *r)
{
	r->MinX = in->in_X - ww->ww_OffX + in->in_Box.MinX;
	r->MinY = in->in_Y - ww->ww_OffY + in->in_Box.MinY;
	r->MaxX = in->in_X - ww->ww_OffX + in->in_Box.MaxX;
	r->MaxY = in->in_Y - ww->ww_OffY + in->in_Box.MaxY;
}

static BOOL overlaps(struct WBWin *ww, struct WBIcon *in, struct Rectangle *r)
{
	struct Rectangle b;

	box(ww, in, &b);
	return b.MinX <= r->MaxX && b.MaxX >= r->MinX &&
	    b.MinY <= r->MaxY && b.MaxY >= r->MinY;
}

/* The icons in r */
static void drawrect(struct WBWin *ww, struct Rectangle *r)
{
	struct WBIcon *in;

	FOREACH(struct WBIcon *, in, &ww->ww_Icons)
		if (overlaps(ww, in, r))
			drawicon(ww, in);
}

/* The background of r: the backdrop's is the screen's */
static void erase(struct WBWin *ww, struct Rectangle *r)
{
	if (r->MaxX >= 0 && r->MaxY >= 0)
		EraseRect(ww->ww_Win->RPort, r->MinX < 0 ? 0 : r->MinX,
		    r->MinY < 0 ? 0 : r->MinY, r->MaxX, r->MaxY);
}

void redraw(struct WBWin *ww)
{
	struct Window *w = ww->ww_Win;
	struct WBIcon *in;

	EraseRect(w->RPort, 0, 0, w->GZZWidth - 1, w->GZZHeight - 1);
	FOREACH(struct WBIcon *, in, &ww->ww_Icons)
		drawicon(ww, in);
	setscroll(ww);
}

/* A scroller's knob for pos of total, of which view shows */
static void setprop(struct WBWin *ww, WORD i, LONG pos, LONG total, LONG view)
{
	struct PropInfo *pi = &ww->ww_Prop[i];
	UWORD body = MAXBODY, pot = 0;

	if (total > view) {
		body = (ULONG)view * MAXBODY / total;
		pot = (ULONG)pos * MAXPOT / (total - view);
	}
	if (i == HSCROLL && (pi->HorizPot != pot || pi->HorizBody != body))
		NewModifyProp(&ww->ww_Scroll[i], ww->ww_Win, NULL, pi->Flags,
		    pot, 0, body, MAXBODY, 1);
	if (i == VSCROLL && (pi->VertPot != pot || pi->VertBody != body))
		NewModifyProp(&ww->ww_Scroll[i], ww->ww_Win, NULL, pi->Flags,
		    0, pot, MAXBODY, body, 1);
}

/* The drawer's extent and the scrollers that show it */
void setscroll(struct WBWin *ww)
{
	struct Window *w = ww->ww_Win;
	struct Rectangle *v = &ww->ww_Virt;
	struct WBIcon *in;
	WORD x0, y0, x1, y1;

	v->MinX = ww->ww_OffX;
	v->MinY = ww->ww_OffY;
	v->MaxX = ww->ww_OffX + w->GZZWidth - 1;
	v->MaxY = ww->ww_OffY + w->GZZHeight - 1;
	FOREACH(struct WBIcon *, in, &ww->ww_Icons) {
		x0 = in->in_X + in->in_Box.MinX - GAP;
		y0 = in->in_Y + in->in_Box.MinY - GAP;
		x1 = in->in_X + in->in_Box.MaxX + GAP;
		y1 = in->in_Y + in->in_Box.MaxY + GAP;
		if (x0 < v->MinX)
			v->MinX = x0;
		if (y0 < v->MinY)
			v->MinY = y0;
		if (x1 > v->MaxX)
			v->MaxX = x1;
		if (y1 > v->MaxY)
			v->MaxY = y1;
	}
	if (ww->ww_Scroll[HSCROLL].SpecialInfo == NULL)
		return;
	setprop(ww, HSCROLL, ww->ww_OffX - v->MinX, v->MaxX - v->MinX + 1,
	    w->GZZWidth);
	setprop(ww, VSCROLL, ww->ww_OffY - v->MinY, v->MaxY - v->MinY + 1,
	    w->GZZHeight);
}

/*
 * Show the drawer from x, y: what stays is moved, the icons in the strips
 * that come in are drawn, and parts that were hidden are repaired
 */
static void scrollto(struct WBWin *ww, WORD x, WORD y)
{
	struct Window *w = ww->ww_Win;
	WORD dx = x - ww->ww_OffX, dy = y - ww->ww_OffY;
	struct Rectangle r;

	if (dx == 0 && dy == 0)
		return;
	ww->ww_OffX = x;
	ww->ww_OffY = y;
	ScrollRasterBF(w->RPort, dx, dy, 0, 0, w->GZZWidth - 1,
	    w->GZZHeight - 1);
	r.MinY = 0;
	r.MaxY = w->GZZHeight - 1;
	if (dx > 0) {
		r.MinX = w->GZZWidth - dx;
		r.MaxX = w->GZZWidth - 1;
		drawrect(ww, &r);
	} else if (dx < 0) {
		r.MinX = 0;
		r.MaxX = -dx - 1;
		drawrect(ww, &r);
	}
	r.MinX = 0;
	r.MaxX = w->GZZWidth - 1;
	if (dy > 0) {
		r.MinY = w->GZZHeight - dy;
		r.MaxY = w->GZZHeight - 1;
		drawrect(ww, &r);
	} else if (dy < 0) {
		r.MinY = 0;
		r.MaxY = -dy - 1;
		drawrect(ww, &r);
	}
	if (w->WLayer->Flags & LAYERREFRESH)
		refresh(ww);
}

/* Follow the scrollers' knobs over the extent setscroll() found */
void scroll(struct WBWin *ww)
{
	struct Window *w = ww->ww_Win;
	struct Rectangle *v = &ww->ww_Virt;
	LONG sw = v->MaxX - v->MinX + 1 - w->GZZWidth;
	LONG sh = v->MaxY - v->MinY + 1 - w->GZZHeight;
	WORD x = ww->ww_OffX, y = ww->ww_OffY;

	if (sw > 0)
		x = v->MinX + (ULONG)ww->ww_Prop[HSCROLL].HorizPot * sw / MAXPOT;
	if (sh > 0)
		y = v->MinY + (ULONG)ww->ww_Prop[VSCROLL].VertPot * sh / MAXPOT;
	scrollto(ww, x, y);
}

/* Only the icons in the damage */
void refresh(struct WBWin *ww)
{
	struct Window *w = ww->ww_Win;
	struct Rectangle r;

	BeginRefresh(w);
	r = w->WLayer->DamageList->bounds;
	drawrect(ww, &r);
	EndRefresh(w, TRUE);
}

struct WBIcon *hit(struct WBWin *ww, WORD x, WORD y)
{
	struct WBIcon *in;
	struct Rectangle r;

	r.MinX = r.MaxX = x;
	r.MinY = r.MaxY = y;
	for (in = (struct WBIcon *)ww->ww_Icons.mlh_TailPred;
	    in->in_Node.mln_Pred != NULL;
	    in = (struct WBIcon *)in->in_Node.mln_Pred)
		if (overlaps(ww, in, &r))
			return in;
	return NULL;
}

void selecticon(struct WBWin *ww, struct WBIcon *in, BOOL sel)
{
	if (in->in_Selected != sel) {
		in->in_Selected = sel;
		drawicon(ww, in);
	}
}

/* Deselect the icons of every window but keep */
void deselectall(struct WBWin *keep)
{
	struct WBWin *ww;
	struct WBIcon *in;

	FOREACHWIN(ww)
		if (ww != keep)
			FOREACH(struct WBIcon *, in, &ww->ww_Icons)
				selecticon(ww, in, FALSE);
}

/* Select the icons inside r */
void lasso(struct WBWin *ww, struct Rectangle *r)
{
	struct WBIcon *in;

	FOREACH(struct WBIcon *, in, &ww->ww_Icons)
		if (overlaps(ww, in, r))
			selecticon(ww, in, TRUE);
}

/* The selected icons' boxes, dx, dy away, complemented on the screen */
void outline(struct WBWin *ww, WORD dx, WORD dy)
{
	struct RastPort *rp = &scr->RastPort;
	struct Window *w = ww->ww_Win;
	struct WBIcon *in;
	struct Rectangle r;
	WORD x0, y0, x1, y1;

	SetDrMd(rp, COMPLEMENT);
	FOREACH(struct WBIcon *, in, &ww->ww_Icons) {
		if (!in->in_Selected)
			continue;
		box(ww, in, &r);
		x0 = w->LeftEdge + w->BorderLeft + r.MinX + dx;
		y0 = w->TopEdge + w->BorderTop + r.MinY + dy;
		x1 = x0 + r.MaxX - r.MinX;
		y1 = y0 + r.MaxY - r.MinY;
		Move(rp, x0, y0);
		Draw(rp, x1, y0);
		Draw(rp, x1, y1);
		Draw(rp, x0, y1);
		Draw(rp, x0, y0 + 1);
	}
	SetDrMd(rp, JAM1);
}

/*
 * Move the selected icons by dx, dy: erase each and draw what it covered.
 * A moved icon has a position, which Snapshot saves.
 */
void moveicons(struct WBWin *ww, WORD dx, WORD dy)
{
	struct WBIcon *in;
	struct Rectangle r;

	FOREACH(struct WBIcon *, in, &ww->ww_Icons) {
		if (!in->in_Selected)
			continue;
		box(ww, in, &r);
		in->in_X += dx;
		in->in_Y += dy;
		in->in_Obj->do_CurrentX = in->in_X;
		in->in_Obj->do_CurrentY = in->in_Y;
		erase(ww, &r);
		drawrect(ww, &r);
	}
	FOREACH(struct WBIcon *, in, &ww->ww_Icons)
		if (in->in_Selected)
			drawicon(ww, in);
	setscroll(ww);
}

/*
 * The backdrop's AppIcons as workbench.library lists them: the new ones
 * go below the icons in its column, the removed ones are erased
 */
void appicons(void)
{
	static char text[PATHLEN];
	struct DiskObject *d = NULL;
	struct WBIcon *in, *next;
	struct Rectangle r;
	WORD y = GAP, iw, bw;

	if (root == NULL)
		return;
	FOREACH(struct WBIcon *, in, &root->ww_Icons) {
		in->in_Mark = in->in_App != NULL;
		if (in->in_Y + in->in_Box.MaxY + GAP > y)
			y = in->in_Y + in->in_Box.MaxY + GAP;
	}
	while ((d = GetNextAppIcon(d, text)) != NULL) {
		FOREACH(struct WBIcon *, in, &root->ww_Icons)
			if (in->in_App == d)
				break;
		if (in->in_Node.mln_Succ != NULL)
			in->in_Mark = 0;
		else if ((in = newicon(root, text, DupDiskObjectA(d, NULL))) !=
		    NULL) {
			in->in_App = d;
			in->in_Mark = 2;
		}
	}
	iw = root->ww_Win->GZZWidth;
	for (in = (struct WBIcon *)root->ww_Icons.mlh_Head;
	    (next = (struct WBIcon *)in->in_Node.mln_Succ) != NULL; in = next) {
		if (in->in_Mark == 1) {
			box(root, in, &r);
			Remove((struct Node *)in);
			FreeDiskObject(in->in_Obj);
			FreeVec(in);
			erase(root, &r);
			drawrect(root, &r);
		} else if (in->in_Mark == 2) {
			if (in->in_Obj->do_CurrentX == NO_ICON_POSITION ||
			    in->in_Obj->do_CurrentY == NO_ICON_POSITION) {
				bw = in->in_Box.MaxX - in->in_Box.MinX + 1;
				in->in_X = iw - GAP - COLW / 2 - bw / 2 -
				    in->in_Box.MinX;
				in->in_Y = y;
				y += in->in_Box.MaxY - in->in_Box.MinY + 1 + GAP;
			}
			drawicon(root, in);
		}
	}
	setscroll(root);
}

/* Take the icon out of the window */
void removeicon(struct WBWin *ww, struct WBIcon *in)
{
	struct Rectangle r;

	box(ww, in, &r);
	Remove((struct Node *)in);
	FreeDiskObject(in->in_Obj);
	FreeVec(in);
	erase(ww, &r);
	drawrect(ww, &r);
}

/*
 * The object name, new in the drawer: its icon at x, y, or for
 * NO_ICON_POSITION where its .info says or below the others
 */
void addicon(struct WBWin *ww, STRPTR name, LONG x, LONG y)
{
	char path[PATHLEN];
	struct DiskObject *dobj;
	struct WBIcon *in;
	WORD y0 = GAP;

	FOREACH(struct WBIcon *, in, &ww->ww_Icons)
		if (Stricmp(in->in_Name, name) == 0) {
			removeicon(ww, in);
			break;
		}
	strcpy(path, ww->ww_Path);
	if (!AddPart(path, name, PATHLEN))
		return;
	if ((dobj = GetDiskObject(path)) == NULL && ww->ww_ShowAll)
		dobj = GetDiskObjectNew(path);
	if (dobj == NULL)
		return;
	if (x != NO_ICON_POSITION) {
		dobj->do_CurrentX = x;
		dobj->do_CurrentY = y;
	}
	FOREACH(struct WBIcon *, in, &ww->ww_Icons)
		if (in->in_Y + in->in_Box.MaxY + GAP > y0)
			y0 = in->in_Y + in->in_Box.MaxY + GAP;
	if ((in = newicon(ww, name, dobj)) == NULL)
		return;
	if (dobj->do_CurrentX == NO_ICON_POSITION ||
	    dobj->do_CurrentY == NO_ICON_POSITION) {
		in->in_X = GAP - in->in_Box.MinX;
		in->in_Y = y0;
	}
	drawicon(ww, in);
	setscroll(ww);
}

/* An object changed: look at its drawer again */
void update(STRPTR name)
{
	char dir[PATHLEN];
	struct WBWin *ww;
	WORD n;

	n = PathPart(name) - name;
	if (n >= PATHLEN)
		return;
	CopyMem(name, dir, n);
	dir[n] = 0;
	if ((ww = findpath(dir)) != NULL) {
		freeicons(ww);
		scan(ww);
		redraw(ww);
	}
}
