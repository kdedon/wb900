/* SPDX-License-Identifier: MIT */
/*
 * Workbench's windows: the backdrop with the disks and the drawer
 * windows, their icons, and drawing them.  Icons are kept in their
 * window's list in drawing order; a window's inner area starts at 0, 0
 * (drawer windows are GimmeZeroZero, the backdrop has no border).
 */
#include "workbench.h"

#define IDCMPFLAGS (IDCMP_CLOSEWINDOW | IDCMP_MOUSEBUTTONS | \
	IDCMP_MOUSEMOVE | IDCMP_MENUPICK | IDCMP_REFRESHWINDOW | \
	IDCMP_NEWSIZE | IDCMP_ACTIVEWINDOW)

/* Columns of the icon grid */
#define COLW	72
#define GAP	6

static struct TagItem drawtags[] = {
	{ ICONDRAWA_Frameless, TRUE }, { ICONDRAWA_EraseBackground, TRUE },
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
	arrange(ww, TRUE);
	redraw(ww);
}

struct WBWin *openwin(STRPTR path, struct DiskObject *dobj)
{
	struct WBWin *ww;
	struct NewWindow *nw = NULL;
	struct TagItem t[16];
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
		t[i].ti_Tag = WA_GimmeZeroZero;	t[i++].ti_Data = TRUE;
		t[i].ti_Tag = WA_Activate;	t[i++].ti_Data = TRUE;
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
	DrawIconStateA(ww->ww_Win->RPort, in->in_Obj, lbl, in->in_X, in->in_Y,
	    in->in_Selected ? IDS_SELECTED : IDS_NORMAL, drawtags);
}

static BOOL overlaps(struct WBIcon *in, struct Rectangle *r)
{
	return in->in_X + in->in_Box.MinX <= r->MaxX &&
	    in->in_X + in->in_Box.MaxX >= r->MinX &&
	    in->in_Y + in->in_Box.MinY <= r->MaxY &&
	    in->in_Y + in->in_Box.MaxY >= r->MinY;
}

/* The icons in r */
static void drawrect(struct WBWin *ww, struct Rectangle *r)
{
	struct WBIcon *in;

	FOREACH(struct WBIcon *, in, &ww->ww_Icons)
		if (overlaps(in, r))
			drawicon(ww, in);
}

void redraw(struct WBWin *ww)
{
	struct Window *w = ww->ww_Win;
	struct WBIcon *in;

	SetAPen(w->RPort, 0);
	RectFill(w->RPort, 0, 0, w->GZZWidth - 1, w->GZZHeight - 1);
	FOREACH(struct WBIcon *, in, &ww->ww_Icons)
		drawicon(ww, in);
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
		if (overlaps(in, &r))
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
		if (overlaps(in, r))
			selecticon(ww, in, TRUE);
}

/* The selected icons' boxes, dx, dy away, complemented on the screen */
void outline(struct WBWin *ww, WORD dx, WORD dy)
{
	struct RastPort *rp = &scr->RastPort;
	struct Window *w = ww->ww_Win;
	struct WBIcon *in;
	WORD x0, y0, x1, y1;

	SetDrMd(rp, COMPLEMENT);
	FOREACH(struct WBIcon *, in, &ww->ww_Icons) {
		if (!in->in_Selected)
			continue;
		x0 = w->LeftEdge + w->BorderLeft + in->in_X + in->in_Box.MinX + dx;
		y0 = w->TopEdge + w->BorderTop + in->in_Y + in->in_Box.MinY + dy;
		x1 = x0 + in->in_Box.MaxX - in->in_Box.MinX;
		y1 = y0 + in->in_Box.MaxY - in->in_Box.MinY;
		Move(rp, x0, y0);
		Draw(rp, x1, y0);
		Draw(rp, x1, y1);
		Draw(rp, x0, y1);
		Draw(rp, x0, y0 + 1);
	}
	SetDrMd(rp, JAM1);
}

/* Move the selected icons by dx, dy: erase each and draw what it covered */
void moveicons(struct WBWin *ww, WORD dx, WORD dy)
{
	struct RastPort *rp = ww->ww_Win->RPort;
	struct WBIcon *in;
	struct Rectangle r;

	FOREACH(struct WBIcon *, in, &ww->ww_Icons) {
		if (!in->in_Selected)
			continue;
		r.MinX = in->in_X + in->in_Box.MinX;
		r.MinY = in->in_Y + in->in_Box.MinY;
		r.MaxX = in->in_X + in->in_Box.MaxX;
		r.MaxY = in->in_Y + in->in_Box.MaxY;
		in->in_X += dx;
		in->in_Y += dy;
		SetAPen(rp, 0);
		RectFill(rp, r.MinX < 0 ? 0 : r.MinX, r.MinY < 0 ? 0 : r.MinY,
		    r.MaxX, r.MaxY);
		drawrect(ww, &r);
	}
	FOREACH(struct WBIcon *, in, &ww->ww_Icons)
		if (in->in_Selected)
			drawicon(ww, in);
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
