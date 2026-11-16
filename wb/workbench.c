/* SPDX-License-Identifier: MIT */
/*
 * Workbench: the Workbench screen with the disks on its backdrop and a
 * window for each open drawer.  Icons are selected with a click (shift
 * adds), a lasso, or dragged as outlines within or between windows; a
 * double click opens a drawer or launches a program through
 * workbench.library.  Icons dropped on another volume are copied.
 * AppIcons and the Tools menu's AppMenuItems send their applications
 * AppMessages.  Leave Out puts icons on the backdrop and lists them in
 * their volume's .backdrop, Put Away takes them back.  Without a display
 * it drives itself (selftest()).
 */
#include "workbench.h"
#include <devices/inputevent.h>

struct DosLibrary *DOSBase;
struct GfxBase *GfxBase;
struct IntuitionBase *IntuitionBase;
struct Library *LayersBase, *UtilityBase, *GadToolsBase, *IconBase,
    *WorkbenchBase;

struct Screen *scr;
struct MinList wins;
struct WBWin *root;
struct MsgPort *idport;

static struct MsgPort *wbport;
static APTR vi;
struct Menu *wbmenus;
BOOL wbbackdrop = TRUE;
static BOOL running = TRUE;

ULONG strlen(CONST_STRPTR s)
{
	CONST_STRPTR p = s;

	while (*p)
		p++;
	return (ULONG)(p - s);
}

char *strcpy(char *d, CONST_STRPTR s)
{
	char *p = d;

	while ((*p++ = *s++) != 0)
		;
	return d;
}

char *strcat(char *d, CONST_STRPTR s)
{
	strcpy(d + strlen(d), s);
	return d;
}

APTR memset(APTR d, int c, ULONG n)
{
	UBYTE *p = d;

	while (n--)
		*p++ = c;
	return d;
}

/* At most n - 1 characters, always terminated */
char *strncpy(char *d, CONST_STRPTR s, ULONG n)
{
	char *p = d;

	while (n > 1 && (*p = *s++) != 0) {
		p++;
		n--;
	}
	*p = 0;
	return d;
}

/*** Menus ***/

enum { M_BACKDROP = 1, M_EXECUTE, M_REDRAW, M_ABOUT, M_QUIT, M_PARENT,
	M_CLOSE, M_CLEANUP, M_ICONS, M_ALL, M_SNAPWIN, M_OPEN, M_INFO,
	M_SNAPSHOT, M_UNSNAPSHOT, M_LEAVEOUT, M_PUTAWAY, M_RENAME, M_DELETE,
	M_TOOL };

static struct NewMenu newmenus[] = {
	{ NM_TITLE, "Workbench", NULL, 0, 0, NULL },
	{ NM_ITEM, "Backdrop", "B", CHECKIT | MENUTOGGLE | CHECKED, 0,
	    (APTR)M_BACKDROP },
	{ NM_ITEM, "Execute Command...", "E", 0, 0, (APTR)M_EXECUTE },
	{ NM_ITEM, "Redraw All", NULL, 0, 0, (APTR)M_REDRAW },
	{ NM_ITEM, "About...", NULL, 0, 0, (APTR)M_ABOUT },
	{ NM_ITEM, "Quit", "Q", 0, 0, (APTR)M_QUIT },
	{ NM_TITLE, "Window", NULL, 0, 0, NULL },
	{ NM_ITEM, "Open Parent", NULL, 0, 0, (APTR)M_PARENT },
	{ NM_ITEM, "Close", "K", 0, 0, (APTR)M_CLOSE },
	{ NM_ITEM, "Clean Up", ".", 0, 0, (APTR)M_CLEANUP },
	{ NM_ITEM, "Show Only Icons", NULL, CHECKIT | CHECKED, 0x10,
	    (APTR)M_ICONS },
	{ NM_ITEM, "Show All Files", NULL, CHECKIT, 0x08, (APTR)M_ALL },
	{ NM_ITEM, "Snapshot", NULL, 0, 0, (APTR)M_SNAPWIN },
	{ NM_TITLE, "Icons", NULL, 0, 0, NULL },
	{ NM_ITEM, "Open", "O", 0, 0, (APTR)M_OPEN },
	{ NM_ITEM, "Information...", "I", 0, 0, (APTR)M_INFO },
	{ NM_ITEM, "Snapshot", "S", 0, 0, (APTR)M_SNAPSHOT },
	{ NM_ITEM, "UnSnapshot", "U", 0, 0, (APTR)M_UNSNAPSHOT },
	{ NM_ITEM, "Leave Out", "L", 0, 0, (APTR)M_LEAVEOUT },
	{ NM_ITEM, "Put Away", "P", 0, 0, (APTR)M_PUTAWAY },
	{ NM_ITEM, "Rename...", "R", 0, 0, (APTR)M_RENAME },
	{ NM_ITEM, "Delete...", NULL, 0, 0, (APTR)M_DELETE },
	{ NM_END, NULL, NULL, 0, 0, NULL }
};

void windowmenus(struct Window *w)
{
	if (wbmenus != NULL)
		SetMenuStrip(w, wbmenus);
}

static void showchecks(struct WBWin *ww);

/* The menus, with a Tools menu of the AppMenuItems after newmenus' */
#define NBASE	(sizeof(newmenus) / sizeof(newmenus[0]) - 1)
#define MAXTOOLS 16

static struct NewMenu toolmenus[NBASE + MAXTOOLS + 2];
static struct AppMenuItem *tools[MAXTOOLS];
static char toolname[MAXTOOLS][APPMENUTEXT], toolkey[MAXTOOLS][2];

/* An AppMenuItem of only '-', '_' and '~' is a bar */
static BOOL isbar(char *s)
{
	for (; *s; s++)
		if (*s != '-' && *s != '_' && *s != '~')
			return FALSE;
	return TRUE;
}

static void makemenus(void)
{
	struct NewMenu *nm = toolmenus;
	struct AppMenuItem *ami = NULL;
	struct TagItem t[2];
	struct Menu *m;
	struct WBWin *w;
	WORD n = NBASE, i;

	CopyMem(newmenus, nm, sizeof(newmenus));
	if (!wbbackdrop)
		nm[1].nm_Flags &= ~CHECKED;
	for (i = 0; i < MAXTOOLS &&
	    (ami = GetNextAppMenuItem(ami, toolname[i], toolkey[i])) != NULL;
	    i++) {
		if (i == 0) {
			memset(&nm[n], 0, sizeof(nm[n]));
			nm[n].nm_Type = NM_TITLE;
			nm[n++].nm_Label = "Tools";
		}
		tools[i] = ami;
		memset(&nm[n], 0, sizeof(nm[n]));
		nm[n].nm_Type = NM_ITEM;
		nm[n].nm_Label = isbar(toolname[i]) ? NM_BARLABEL : toolname[i];
		nm[n].nm_CommKey = toolkey[i][0] ? toolkey[i] : NULL;
		nm[n++].nm_UserData = (APTR)(IPTR)(M_TOOL + i);
	}
	memset(&nm[n], 0, sizeof(nm[n]));
	t[0].ti_Tag = GTMN_NewLookMenus;	t[0].ti_Data = TRUE;
	t[1].ti_Tag = TAG_DONE;
	if ((m = CreateMenusA(nm, NULL)) == NULL)
		return;
	if (!LayoutMenusA(m, vi, t)) {
		FreeMenus(m);
		return;
	}
	FOREACHWIN(w)
		ClearMenuStrip(w->ww_Win);
	FreeMenus(wbmenus);
	wbmenus = m;
	FOREACHWIN(w)
		SetMenuStrip(w->ww_Win, wbmenus);
	if ((w = findwin(IntuitionBase->ActiveWindow)) != NULL)
		showchecks(w);
}

/* The Show items' checkmarks for the active window */
static void showchecks(struct WBWin *ww)
{
	struct MenuItem *icons, *all;
	struct WBWin *w;

	if (wbmenus == NULL)
		return;
	icons = ItemAddress(wbmenus, FULLMENUNUM(1, 3, NOSUB));
	all = ItemAddress(wbmenus, FULLMENUNUM(1, 4, NOSUB));
	/* ClearMenuStrip() waits while a menu is up */
	if (!(all->Flags & CHECKED) == !ww->ww_ShowAll &&
	    !(icons->Flags & CHECKED) == !!ww->ww_ShowAll)
		return;
	FOREACHWIN(w)
		ClearMenuStrip(w->ww_Win);
	icons->Flags &= ~CHECKED;
	all->Flags &= ~CHECKED;
	if (ww->ww_ShowAll)
		all->Flags |= CHECKED;
	else
		icons->Flags |= CHECKED;
	FOREACHWIN(w)
		ResetMenuStrip(w->ww_Win, wbmenus);
}

/*** Requesters ***/

static LONG request(struct WBWin *ww, CONST_STRPTR text, CONST_STRPTR gads,
    ULONG *args)
{
	struct EasyStruct es;

	es.es_StructSize = sizeof(es);
	es.es_Flags = 0;
	es.es_Title = "Workbench";
	es.es_TextFormat = text;
	es.es_GadgetFormat = gads;
	return EasyRequestArgs(ww ? ww->ww_Win : NULL, &es, NULL, args);
}

/* A line of text from the user into buf, of len bytes */
static BOOL askstring(CONST_STRPTR title, STRPTR buf, WORD len)
{
	static UBYTE undo[PATHLEN];
	static WORD xy[] = { -2, -2, 301, -2, 301, 9, -2, 9, -2, -2 };
	static struct Border border = { 0, 0, 1, 0, JAM1, 5, xy, NULL };
	struct StringInfo si;
	struct Gadget g;
	struct Window *w;
	struct IntuiMessage *im;
	struct TagItem t[10];
	ULONG class = 0;

	memset(&si, 0, sizeof(si));
	memset(&g, 0, sizeof(g));
	si.Buffer = buf;
	si.UndoBuffer = undo;
	si.MaxChars = len;
	g.LeftEdge = 12;
	g.TopEdge = scr->BarHeight + 10;
	g.Width = 300;
	g.Height = 8;
	g.Flags = GFLG_GADGHCOMP;
	g.Activation = GACT_RELVERIFY;
	g.GadgetType = GTYP_STRGADGET;
	g.GadgetRender = (APTR)&border;
	g.SpecialInfo = (APTR)&si;

	t[0].ti_Tag = WA_Left;		t[0].ti_Data = (scr->Width - 324) / 2;
	t[1].ti_Tag = WA_Top;		t[1].ti_Data = scr->Height / 3;
	t[2].ti_Tag = WA_Width;		t[2].ti_Data = 324;
	t[3].ti_Tag = WA_Height;	t[3].ti_Data = scr->BarHeight + 26;
	t[4].ti_Tag = WA_Title;		t[4].ti_Data = (IPTR)title;
	t[5].ti_Tag = WA_Gadgets;	t[5].ti_Data = (IPTR)&g;
	t[6].ti_Tag = WA_IDCMP;
	t[6].ti_Data = IDCMP_GADGETUP | IDCMP_CLOSEWINDOW;
	t[7].ti_Tag = WA_CloseGadget;	t[7].ti_Data = TRUE;
	t[8].ti_Tag = WA_Activate;	t[8].ti_Data = TRUE;
	t[9].ti_Tag = WA_CustomScreen;	t[9].ti_Data = (IPTR)scr;
	if ((w = OpenWindowTagList(NULL, t)) == NULL)
		return FALSE;
	ActivateGadget(&g, w, NULL);
	while (class == 0) {
		WaitPort(w->UserPort);
		while ((im = (struct IntuiMessage *)GetMsg(w->UserPort)) != NULL) {
			class = im->Class;
			ReplyMsg((struct Message *)im);
		}
	}
	CloseWindow(w);
	return class == IDCMP_GADGETUP && buf[0] != 0;
}

/*** Commands ***/

/* The commands keep their paths static, off the stack their requesters use */

static void execute(void)
{
	static char cmd[PATHLEN];
	struct TagItem t[3];
	BPTR out;

	cmd[0] = 0;
	if (!askstring("Execute a file", cmd, PATHLEN) ||
	    (out = Open("CON:", MODE_NEWFILE)) == NULL)
		return;
	t[0].ti_Tag = SYS_Asynch;	t[0].ti_Data = TRUE;
	t[1].ti_Tag = SYS_Output;	t[1].ti_Data = (IPTR)out;
	t[2].ti_Tag = TAG_DONE;
	if (SystemTagList(cmd, t) == -1) {
		Close(out);
		DisplayBeep(scr);
	}
}

/* Open a drawer, or bring its window to the front */
static void opendrawer(STRPTR path, struct DiskObject *dobj)
{
	static char name[PATHLEN];
	struct WBWin *ww;
	BPTR lock;

	/* by its volume's name, as the disks and left-out icons have it */
	if ((lock = Lock(path, ACCESS_READ)) == NULL) {
		DisplayBeep(scr);
		return;
	}
	if (NameFromLock(lock, name, PATHLEN))
		path = name;
	UnLock(lock);
	if ((ww = findpath(path)) != NULL) {
		WindowToFront(ww->ww_Win);
		ActivateWindow(ww->ww_Win);
	} else if (openwin(path, dobj) == NULL) {
		DisplayBeep(scr);
	}
}

void openicon(struct WBWin *ww, struct WBIcon *in)
{
	static char path[PATHLEN];
	struct DiskObject *dobj;

	if (in->in_App != NULL) {
		if (!SendAppIconMessage(in->in_App, 0, NULL, AMCLASSICON_Open,
		    0, 0, 0, 0))
			DisplayBeep(scr);
		return;
	}
	if (!fullpath(ww, in, path))
		return;
	switch (in->in_Obj->do_Type) {
	case WBDISK:
	case WBDRAWER:
	case WBGARBAGE:
		/* the window as last snapshot */
		dobj = GetDiskObjectNew(path);
		opendrawer(path, dobj);
		FreeDiskObject(dobj);
		break;
	default:
		if (!OpenWorkbenchObjectA(path, NULL))
			DisplayBeep(scr);
		break;
	}
}

static void parent(struct WBWin *ww)
{
	char dir[PATHLEN];
	struct DiskObject *dobj;
	WORD n;

	if (ww == root || ww->ww_Path[strlen(ww->ww_Path) - 1] == ':') {
		DisplayBeep(scr);
		return;
	}
	n = PathPart(ww->ww_Path) - ww->ww_Path;
	CopyMem(ww->ww_Path, dir, n);
	dir[n] = 0;
	dobj = GetDiskObjectNew(dir);
	opendrawer(dir, dobj);
	FreeDiskObject(dobj);
}

static char *typename[] = { "?", "disk", "drawer", "tool", "project",
	"trashcan", "device", "kick", "AppIcon" };

static void info(struct WBWin *ww, struct WBIcon *in)
{
	struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
	struct DiskObject *d = in->in_Obj;
	static char path[PATHLEN];
	ULONG args[5];
	BPTR lock;

	args[0] = (IPTR)in->in_Name;
	args[1] = (IPTR)typename[d->do_Type <= WBAPPICON ? d->do_Type : 0];
	args[2] = 0;
	args[3] = (IPTR)(d->do_DefaultTool ? d->do_DefaultTool : "");
	args[4] = d->do_StackSize;
	if (fib != NULL && fullpath(ww, in, path) &&
	    (lock = Lock(path, ACCESS_READ)) != NULL) {
		if (Examine(lock, fib))
			args[2] = fib->fib_Size;
		UnLock(lock);
	}
	FreeDosObject(DOS_FIB, fib);
	request(ww, "%s\nType: %s\nSize: %ld bytes\nDefault tool: %s\n"
	    "Stack: %ld bytes", "OK", args);
}

/* The object path and its icon renamed to name, in the same drawer */
static BOOL renameobj(STRPTR path, STRPTR name)
{
	char to[PATHLEN], from[PATHLEN];
	WORD n = PathPart(path) - path;
	BOOL ok;

	CopyMem(path, to, n);
	to[n] = 0;
	if (!AddPart(to, name, PATHLEN - 5))
		return FALSE;
	ok = Rename(path, to);
	strcpy(from, path);
	strcat(from, ".info");
	strcat(to, ".info");
	Rename(from, to);
	return ok;
}

static void wbrename(struct WBWin *ww, struct WBIcon *in)
{
	static char path[PATHLEN], name[NAMELEN];

	if (ww == root) {
		DisplayBeep(scr);
		return;
	}
	strcpy(name, in->in_Name);
	if (!askstring("Rename", name, NAMELEN) || !fullpath(ww, in, path))
		return;
	if (!renameobj(path, name))
		DisplayBeep(scr);
	update(path);
}

static void wbdelete(struct WBWin *ww, struct WBIcon *in)
{
	static char path[PATHLEN];
	ULONG args[1];

	args[0] = (IPTR)in->in_Name;
	if (ww == root || !fullpath(ww, in, path) ||
	    !request(ww, "Delete %s?", "Delete|Cancel", args))
		return;
	if (!DeleteFile(path) && IoErr() != ERROR_OBJECT_NOT_FOUND)
		DisplayBeep(scr);
	else
		DeleteDiskObject(path);
	update(path);
}

/* fn on each selected icon, or with first only on the first */
static void selected(void (*fn)(struct WBWin *, struct WBIcon *), BOOL first)
{
	struct WBWin *ww;
	struct WBIcon *in;

	FOREACHWIN(ww)
		FOREACH(struct WBIcon *, in, &ww->ww_Icons)
			if (in->in_Selected) {
				(*fn)(ww, in);
				if (first)
					return;
			}
}

/* The icon's position into its .info: where it is, or none */
static void snapicon(struct WBWin *ww, struct WBIcon *in, BOOL keep)
{
	char path[PATHLEN];

	if (in->in_App != NULL || !fullpath(ww, in, path))
		return;
	in->in_Obj->do_CurrentX = keep ? in->in_X : NO_ICON_POSITION;
	in->in_Obj->do_CurrentY = keep ? in->in_Y : NO_ICON_POSITION;
	if (!PutIconTagList(path, in->in_Obj, NULL))
		DisplayBeep(scr);
}

static void snapshot(struct WBWin *ww, struct WBIcon *in)
{
	snapicon(ww, in, TRUE);
}

static void unsnapshot(struct WBWin *ww, struct WBIcon *in)
{
	snapicon(ww, in, FALSE);
}

/* The icons' positions, and the window's in its drawer's icon */
static void snapwin(struct WBWin *ww)
{
	struct Window *w = ww->ww_Win;
	struct DiskObject *d;
	struct DrawerData *dd;
	struct WBIcon *in;

	FOREACH(struct WBIcon *, in, &ww->ww_Icons)
		snapicon(ww, in, TRUE);
	if (ww == root)
		return;
	d = GetDiskObjectNew(ww->ww_Path);
	if (d == NULL || (dd = d->do_DrawerData) == NULL) {
		DisplayBeep(scr);
		FreeDiskObject(d);
		return;
	}
	dd->dd_NewWindow.LeftEdge = w->LeftEdge;
	dd->dd_NewWindow.TopEdge = w->TopEdge;
	dd->dd_NewWindow.Width = w->Width;
	dd->dd_NewWindow.Height = w->Height;
	dd->dd_CurrentX = ww->ww_OffX;
	dd->dd_CurrentY = ww->ww_OffY;
	dd->dd_Flags = ww->ww_ShowAll ? DDFLAGS_SHOWALL : DDFLAGS_SHOWICONS;
	if (!PutIconTagList(ww->ww_Path, d, NULL))
		DisplayBeep(scr);
	FreeDiskObject(d);
}

#define MAXARGS	8

/* The paths of the selected icons, of from or all windows, in buf */
static WORD selfiles(struct WBWin *from, char **files, char *buf)
{
	struct WBWin *ww;
	struct WBIcon *in;
	WORD n = 0;

	FOREACHWIN(ww)
		if (from == NULL || ww == from)
			FOREACH(struct WBIcon *, in, &ww->ww_Icons)
				if (in->in_Selected && in->in_App == NULL &&
				    n < MAXARGS && fullpath(ww, in, buf)) {
					files[n++] = buf;
					buf += PATHLEN;
				}
	return n;
}

/* An AppMessage of the selected icons for a pick of the Tools menu */
static void picktool(WORD i)
{
	char *files[MAXARGS], *buf = AllocVec(MAXARGS * PATHLEN, MEMF_ANY);
	ULONG secs, micros;

	CurrentTime(&secs, &micros);
	if (buf == NULL || !SendAppMenuItemMessage(tools[i],
	    selfiles(NULL, files, buf), files, secs, micros))
		DisplayBeep(scr);
	FreeVec(buf);
}

static void setbackdrop(BOOL on)
{
	if (on == wbbackdrop)
		return;
	wbbackdrop = on;
	if (root != NULL)
		closewin(root);
	openwin(NULL, NULL);
}

static void showmode(struct WBWin *ww, BOOL all)
{
	if (ww == root || ww->ww_ShowAll == all)
		return;
	ww->ww_ShowAll = all;
	freeicons(ww);
	scan(ww);
	redraw(ww);
}

static void menu(struct WBWin *ww, UWORD code)
{
	struct MenuItem *item;
	struct WBWin *w;
	ULONG about[2];

	while (code != MENUNULL && running &&
	    (item = ItemAddress(wbmenus, code)) != NULL) {
		switch ((IPTR)GTMENUITEM_USERDATA(item)) {
		case M_BACKDROP:
			setbackdrop((item->Flags & CHECKED) != 0);
			return;
		case M_EXECUTE:
			execute();
			break;
		case M_REDRAW:
			FOREACHWIN(w) {
				if (w != root || !wbbackdrop)
					RefreshWindowFrame(w->ww_Win);
				redraw(w);
			}
			break;
		case M_ABOUT:
			about[0] = IconBase->lib_Version;
			about[1] = WorkbenchBase->lib_Version;
			request(ww, "wb900 Workbench\nicon.library %ld, "
			    "workbench.library %ld", "OK", about);
			break;
		case M_QUIT:
			running = FALSE;
			break;
		case M_PARENT:
			if (ww != NULL)
				parent(ww);
			break;
		case M_CLOSE:
			if (ww != NULL && ww != root)
				closewin(ww);
			return;
		case M_CLEANUP:
			if (ww != NULL)
				cleanup(ww);
			break;
		case M_ICONS:
		case M_ALL:
			if (ww != NULL)
				showmode(ww, (IPTR)GTMENUITEM_USERDATA(item) == M_ALL);
			break;
		case M_SNAPWIN:
			if (ww != NULL)
				snapwin(ww);
			break;
		case M_OPEN:
			selected(openicon, FALSE);
			break;
		case M_INFO:
			selected(info, FALSE);
			break;
		case M_SNAPSHOT:
			selected(snapshot, FALSE);
			break;
		case M_UNSNAPSHOT:
			selected(unsnapshot, FALSE);
			break;
		case M_LEAVEOUT:
			leaveout();
			break;
		case M_PUTAWAY:
			putaway();
			break;
		case M_RENAME:
			selected(wbrename, TRUE);
			return;
		case M_DELETE:
			selected(wbdelete, TRUE);
			return;
		default:
			picktool((IPTR)GTMENUITEM_USERDATA(item) - M_TOOL);
			break;
		}
		code = item->NextSelect;
	}
}

/*** Mouse ***/

enum { IDLE, PRESS, DRAG, LASSO, SCROLL };

static WORD state = IDLE;
static struct WBWin *mww;		/* the window of the press */
static WORD sx, sy, dx, dy;		/* press on the screen; drag offset */
static struct Rectangle band;		/* the lasso, inner coordinates */
static struct WBIcon *lastin;		/* the last icon clicked, and when */
static ULONG lastsec, lastmic;

static void bandbox(void)
{
	struct RastPort *rp = mww->ww_Win->RPort;

	SetDrMd(rp, COMPLEMENT);
	Move(rp, band.MinX, band.MinY);
	Draw(rp, band.MaxX, band.MinY);
	Draw(rp, band.MaxX, band.MaxY);
	Draw(rp, band.MinX, band.MaxY);
	Draw(rp, band.MinX, band.MinY + 1);
	SetDrMd(rp, JAM1);
}

#define COPYBUF	4096

static BOOL copyfile(STRPTR src, STRPTR dst, UBYTE *buf)
{
	BPTR in, out;
	LONG n;

	if ((in = Open(src, MODE_OLDFILE)) == NULL)
		return FALSE;
	if ((out = Open(dst, MODE_NEWFILE)) == NULL) {
		Close(in);
		return FALSE;
	}
	while ((n = Read(in, buf, COPYBUF)) > 0)
		if (Write(out, buf, n) != n) {
			n = -1;
			break;
		}
	Close(out);
	Close(in);
	if (n != 0)
		DeleteFile(dst);
	return n == 0;
}

/* A drawer level's names, on the heap to spare the stack */
struct level {
	struct FileInfoBlock l_Fib;
	char	 l_From[PATHLEN], l_To[PATHLEN];
};

/* The file or drawer src, and what is in it, to the new name dst */
static BOOL copytree(STRPTR src, STRPTR dst, UBYTE *buf)
{
	struct level *l = AllocVec(sizeof(*l), MEMF_CLEAR);
	BPTR lock, dir;
	BOOL ok = FALSE;

	if (l == NULL)
		return FALSE;
	if ((lock = Lock(src, ACCESS_READ)) != NULL && Examine(lock, &l->l_Fib)) {
		if (l->l_Fib.fib_DirEntryType < 0) {
			ok = copyfile(src, dst, buf);
		} else if ((dir = CreateDir(dst)) != NULL) {
			UnLock(dir);
			ok = TRUE;
			while (ok && ExNext(lock, &l->l_Fib)) {
				strcpy(l->l_From, src);
				strcpy(l->l_To, dst);
				ok = AddPart(l->l_From, l->l_Fib.fib_FileName,
				    PATHLEN) && AddPart(l->l_To,
				    l->l_Fib.fib_FileName, PATHLEN) &&
				    copytree(l->l_From, l->l_To, buf);
			}
		}
	}
	UnLock(lock);
	FreeVec(l);
	return ok;
}

/*
 * Move the selected icons of from into the drawer dir, or copy them
 * with their icons when dir is on another volume.  In dir's window they
 * go dx, dy from where they were, or to free places for NO_ICON_POSITION.
 */
static void dropinto(struct WBWin *from, STRPTR dir, LONG dx, LONG dy)
{
	static struct TagItem busytags[] = {
		{ WA_BusyPointer, TRUE }, { TAG_DONE, 0 }
	};
	struct level *l;
	STRPTR src, to;
	struct WBIcon *in, *next;
	struct WBWin *ww = findpath(dir);
	UBYTE *buf = NULL;
	BOOL moved;

	if (from == root || ww == from ||
	    (l = AllocVec(sizeof(*l), MEMF_ANY)) == NULL)
		return;
	src = l->l_From;
	to = l->l_To;
	for (in = (struct WBIcon *)from->ww_Icons.mlh_Head;
	    (next = (struct WBIcon *)in->in_Node.mln_Succ) != NULL; in = next) {
		if (!in->in_Selected || !fullpath(from, in, src))
			continue;
		strcpy(to, dir);
		if (!AddPart(to, in->in_Name, PATHLEN - 5)) {
			DisplayBeep(scr);
			continue;
		}
		if ((moved = Rename(src, to)) != FALSE) {
			strcat(src, ".info");
			strcat(to, ".info");
			Rename(src, to);
		} else if (IoErr() != ERROR_RENAME_ACROSS_DEVICES ||
		    (buf == NULL && (buf = AllocVec(COPYBUF, MEMF_ANY)) == NULL)) {
			DisplayBeep(scr);
			continue;
		} else {
			SetWindowPointerA(from->ww_Win, busytags);
			if (!copytree(src, to, buf)) {
				DisplayBeep(scr);
				continue;
			}
			strcat(src, ".info");
			strcat(to, ".info");
			copyfile(src, to, buf);
		}
		if (ww != NULL)
			addicon(ww, in->in_Name, dx == NO_ICON_POSITION ? dx :
			    in->in_X + dx, in->in_Y + dy);
		if (moved)
			removeicon(from, in);
	}
	if (buf != NULL) {
		FreeVec(buf);
		SetWindowPointerA(from->ww_Win, NULL);
	}
	FreeVec(l);
}

/* Launch the tool at with the selected icons of from as its arguments */
static void droptool(struct WBWin *from, STRPTR tool)
{
	char path[PATHLEN];
	struct TagItem t[2 * 8 + 1];
	BPTR locks[8];
	struct WBIcon *in;
	WORD n = 0, i;

	FOREACH(struct WBIcon *, in, &from->ww_Icons) {
		if (!in->in_Selected || n == 8 || !fullpath(from, in, path))
			continue;
		*PathPart(path) = 0;
		if ((locks[n] = Lock(path, ACCESS_READ)) == NULL)
			continue;
		t[2 * n].ti_Tag = WBOPENA_ArgLock;
		t[2 * n].ti_Data = (IPTR)locks[n];
		t[2 * n + 1].ti_Tag = WBOPENA_ArgName;
		t[2 * n + 1].ti_Data = (IPTR)in->in_Name;
		n++;
	}
	t[2 * n].ti_Tag = TAG_DONE;
	if (!OpenWorkbenchObjectA(tool, t))
		DisplayBeep(scr);
	for (i = 0; i < n; i++)
		UnLock(locks[i]);
}

/* The selected icons of mww dropped on the AppIcon in, or the AppWindow w */
static void dropapp(struct IntuiMessage *im, struct Window *w,
    struct WBIcon *in)
{
	char *files[MAXARGS], *buf = AllocVec(MAXARGS * PATHLEN, MEMF_ANY);
	WORD n;
	BOOL ok = FALSE;

	if (buf != NULL) {
		n = selfiles(mww, files, buf);
		if (in != NULL)
			ok = SendAppIconMessage(in->in_App, n, files, 0,
			    scr->MouseX, scr->MouseY, im->Seconds, im->Micros);
		else if (w != NULL)
			ok = SendAppWindowMessage(w, n, files, 0,
			    scr->MouseX - w->LeftEdge, scr->MouseY - w->TopEdge,
			    im->Seconds, im->Micros);
	}
	if (!ok)
		DisplayBeep(scr);
	FreeVec(buf);
}

/* The selected icons of mww let go at the screen's mouse position */
static void drop(struct IntuiMessage *im)
{
	char path[PATHLEN];
	struct Layer *l = WhichLayer(&scr->LayerInfo, scr->MouseX, scr->MouseY);
	struct Window *w = l ? l->Window : NULL;
	struct WBWin *ww = findwin(w);
	struct WBIcon *in;

	if (ww == mww) {
		in = hit(ww, scr->MouseX - w->LeftEdge - w->BorderLeft,
		    scr->MouseY - w->TopEdge - w->BorderTop);
		if (in == NULL || in->in_Selected) {
			moveicons(mww, dx, dy);
			return;
		}
	} else if (ww != NULL) {
		in = hit(ww, scr->MouseX - w->LeftEdge - w->BorderLeft,
		    scr->MouseY - w->TopEdge - w->BorderTop);
	} else {
		dropapp(im, w, NULL);
		return;
	}
	if (in == NULL) {
		/* where the outlines were let go */
		if (ww != root)
			dropinto(mww, ww->ww_Path, dx + mww->ww_Win->LeftEdge +
			    mww->ww_Win->BorderLeft - mww->ww_OffX -
			    w->LeftEdge - w->BorderLeft + ww->ww_OffX,
			    dy + mww->ww_Win->TopEdge + mww->ww_Win->BorderTop -
			    mww->ww_OffY - w->TopEdge - w->BorderTop + ww->ww_OffY);
		return;
	}
	if (in->in_App != NULL) {
		dropapp(im, w, in);
		return;
	}
	if (!fullpath(ww, in, path))
		return;
	switch (in->in_Obj->do_Type) {
	case WBDISK:
	case WBDRAWER:
	case WBGARBAGE:
		dropinto(mww, path, NO_ICON_POSITION, 0);
		break;
	case WBTOOL:
		droptool(mww, path);
		break;
	default:
		DisplayBeep(scr);
		break;
	}
}

static void buttons(struct WBWin *ww, struct IntuiMessage *im)
{
	struct Window *w = ww->ww_Win;
	WORD x = im->MouseX - w->BorderLeft, y = im->MouseY - w->BorderTop;
	BOOL shift = (im->Qualifier &
	    (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) != 0;
	struct WBIcon *in;

	if (im->Code == SELECTDOWN) {
		mww = ww;
		sx = scr->MouseX;
		sy = scr->MouseY;
		dx = dy = 0;
		if ((in = hit(ww, x, y)) == NULL) {
			if (!shift)
				deselectall(NULL);
			band.MinX = band.MaxX = x;
			band.MinY = band.MaxY = y;
			bandbox();
			state = LASSO;
			lastin = NULL;
			return;
		}
		if (in == lastin && DoubleClick(lastsec, lastmic, im->Seconds,
		    im->Micros)) {
			lastin = NULL;
			state = IDLE;
			openicon(ww, in);
			return;
		}
		lastin = in;
		lastsec = im->Seconds;
		lastmic = im->Micros;
		if (shift) {
			selecticon(ww, in, !in->in_Selected);
		} else if (!in->in_Selected) {
			deselectall(NULL);
			selecticon(ww, in, TRUE);
		}
		state = in->in_Selected ? PRESS : IDLE;
		return;
	}
	if (im->Code != SELECTUP)
		return;
	if (state == DRAG) {
		outline(mww, dx, dy);
		drop(im);
	} else if (state == LASSO) {
		struct Rectangle r;

		bandbox();
		r.MinX = band.MinX < band.MaxX ? band.MinX : band.MaxX;
		r.MaxX = band.MinX < band.MaxX ? band.MaxX : band.MinX;
		r.MinY = band.MinY < band.MaxY ? band.MinY : band.MaxY;
		r.MaxY = band.MinY < band.MaxY ? band.MaxY : band.MinY;
		lasso(mww, &r);
	}
	state = IDLE;
}

static void moved(void)
{
	struct Window *w;

	switch (state) {
	case PRESS:
		if (scr->MouseX - sx < 4 && sx - scr->MouseX < 4 &&
		    scr->MouseY - sy < 4 && sy - scr->MouseY < 4)
			break;
		state = DRAG;
		lastin = NULL;
		dx = scr->MouseX - sx;
		dy = scr->MouseY - sy;
		outline(mww, dx, dy);
		break;
	case DRAG:
		outline(mww, dx, dy);
		dx = scr->MouseX - sx;
		dy = scr->MouseY - sy;
		outline(mww, dx, dy);
		break;
	case LASSO:
		w = mww->ww_Win;
		bandbox();
		band.MaxX = scr->MouseX - w->LeftEdge - w->BorderLeft;
		band.MaxY = scr->MouseY - w->TopEdge - w->BorderTop;
		bandbox();
		break;
	case SCROLL:
		scroll(mww);
		break;
	}
}

/*** Events ***/

BOOL busy;
static struct Gadget *arrow;		/* held, and ticks until it repeats */
static WORD ticks;

static void stoparrow(struct WBWin *ww)
{
	arrow = NULL;
	ModifyIDCMP(ww->ww_Win, IDCMPFLAGS);
}

static void idcmp(void)
{
	struct IntuiMessage *im;
	struct WBWin *ww;
	struct Gadget *g;
	BOOL move = FALSE;

	busy = TRUE;
	while ((im = (struct IntuiMessage *)GetMsg(idport)) != NULL) {
		ww = findwin(im->IDCMPWindow);
		if (ww == NULL) {
			ReplyMsg((struct Message *)im);
			continue;
		}
		switch (im->Class) {
		case IDCMP_MOUSEMOVE:
			move = TRUE;
			break;
		case IDCMP_MOUSEBUTTONS:
			if (move)
				moved();
			move = FALSE;
			buttons(ww, im);
			break;
		case IDCMP_REFRESHWINDOW:
			refresh(ww);
			break;
		case IDCMP_NEWSIZE:
			setscroll(ww);
			break;
		case IDCMP_GADGETDOWN:
			g = (struct Gadget *)im->IAddress;
			if (g->GadgetID == 0) {
				mww = ww;
				state = SCROLL;
				break;
			}
			/* one step, then more while it is held */
			arrow = g;
			ticks = 3;
			ModifyIDCMP(ww->ww_Win, IDCMPFLAGS | IDCMP_INTUITICKS);
			arrowstep(ww, g->GadgetID);
			break;
		case IDCMP_GADGETUP:
			if (((struct Gadget *)im->IAddress)->GadgetID != 0) {
				stoparrow(ww);
				break;
			}
			scroll(ww);
			setscroll(ww);
			state = IDLE;
			break;
		case IDCMP_INTUITICKS:
			if (arrow == NULL ||
			    !(im->Qualifier & IEQUALIFIER_LEFTBUTTON))
				stoparrow(ww);
			else if (ticks > 0)
				ticks--;
			else if (arrow->Flags & GFLG_SELECTED)
				arrowstep(ww, arrow->GadgetID);
			break;
		case IDCMP_ACTIVEWINDOW:
			showchecks(ww);
			break;
		case IDCMP_MENUPICK:
			ReplyMsg((struct Message *)im);
			menu(ww, im->Code);
			continue;
		case IDCMP_CLOSEWINDOW:
			ReplyMsg((struct Message *)im);
			if (ww != root)
				closewin(ww);
			continue;
		}
		ReplyMsg((struct Message *)im);
	}
	if (move)
		moved();
	busy = FALSE;
}

/* workbench.library's messages: drawers to open, objects that changed */
static void handler(void)
{
	struct WBHandlerMessage *m;

	while ((m = (struct WBHandlerMessage *)GetMsg(wbport)) != NULL) {
		switch (m->wbhm_Type) {
		case WBHM_TYPE_OPEN:
			opendrawer((STRPTR)m->wbhm_Data.Open.Name, NULL);
			break;
		case WBHM_TYPE_UPDATE:
			if (m->wbhm_Data.Update.Name != NULL)
				update((STRPTR)m->wbhm_Data.Update.Name);
			else if (m->wbhm_Data.Update.Type == WBAPPICON)
				appicons();
			else if (m->wbhm_Data.Update.Type == AMTYPE_APPMENUITEM)
				makemenus();
			break;
		}
		ReplyMsg((struct Message *)m);
	}
}

static BOOL openlibs(void)
{
	return (DOSBase = (struct DosLibrary *)OpenLibrary(DOSNAME, 36L)) &&
	    (GfxBase = (struct GfxBase *)OpenLibrary(GRAPHICSNAME, 39L)) &&
	    (IntuitionBase = (struct IntuitionBase *)
	    OpenLibrary(INTUITIONNAME, 39L)) &&
	    (LayersBase = OpenLibrary("layers.library", 39L)) &&
	    (UtilityBase = OpenLibrary(UTILITYNAME, 39L)) &&
	    (GadToolsBase = OpenLibrary("gadtools.library", 39L)) &&
	    (IconBase = OpenLibrary(ICONNAME, 44L)) &&
	    (WorkbenchBase = OpenLibrary(WORKBENCH_NAME, 45L));
}

static void closelibs(void)
{
	CloseLibrary(WorkbenchBase);
	CloseLibrary(IconBase);
	CloseLibrary(GadToolsBase);
	CloseLibrary(UtilityBase);
	CloseLibrary(LayersBase);
	CloseLibrary((struct Library *)IntuitionBase);
	CloseLibrary((struct Library *)GfxBase);
	CloseLibrary((struct Library *)DOSBase);
}

/*
 * Without ENV:, the assigns a Startup-Sequence makes: ENVARC: for
 * SYS:Prefs/Env-Archive and ENV: for a copy of it in RAM:, or for the
 * archive itself without a RAM disk.
 */
static void envinit(void)
{
	BPTR lock;
	UBYTE *buf;

	if ((lock = Lock("ENV:", ACCESS_READ)) != NULL) {
		UnLock(lock);
		return;
	}
	if ((lock = Lock("SYS:Prefs/Env-Archive", ACCESS_READ)) == NULL)
		return;
	if (!AssignLock("ENVARC", lock))
		UnLock(lock);
	if ((buf = AllocVec(COPYBUF, MEMF_ANY)) != NULL) {
		copytree("ENVARC:", "RAM:ENV", buf);
		FreeVec(buf);
	}
	if ((lock = Lock("RAM:ENV", ACCESS_READ)) == NULL)
		lock = Lock("ENVARC:", ACCESS_READ);
	if (lock != NULL && !AssignLock("ENV", lock))
		UnLock(lock);
}

BOOL selftest(void);
LONG selftestdone(void);

/* Workbench's process: 0 when it quits, or the self test's result */
LONG Workbench(char *args, LONG len)
{
	struct TagItem t[2];
	struct WBWin *ww;
	ULONG sigs;
	LONG rc = RETURN_OK;
	BOOL test;

	if (!openlibs()) {
		closelibs();
		return RETURN_FAIL;
	}
	envinit();
	NewList((struct List *)&wins);
	idport = CreateMsgPort();
	wbport = CreateMsgPort();
	if (idport == NULL || wbport == NULL || OpenWorkBench() == 0 ||
	    (scr = LockPubScreen("Workbench")) == NULL) {
		rc = RETURN_FAIL;
		goto done;
	}
	t[0].ti_Tag = ICONCTRLA_SetGlobalScreen;	t[0].ti_Data = (IPTR)scr;
	t[1].ti_Tag = TAG_DONE;
	IconControlA(NULL, t);
	RegisterWorkbench(wbport);
	if ((vi = GetVisualInfoA(scr, NULL)) != NULL)
		makemenus();
	arrowimages();
	openwin(NULL, NULL);
	test = NextDisplayInfo(INVALID_ID) == INVALID_ID;
	if (test && !selftest())
		running = test = FALSE;
	while (running) {
		sigs = Wait((1L << idport->mp_SigBit) |
		    (1L << wbport->mp_SigBit) | SIGBREAKF_CTRL_C);
		if (sigs & (1L << idport->mp_SigBit))
			idcmp();
		if (sigs & (1L << wbport->mp_SigBit))
			handler();
		if (sigs & SIGBREAKF_CTRL_C)
			running = FALSE;
	}

	if (test)
		rc = selftestdone();
	while ((ww = (struct WBWin *)wins.mlh_Head)->ww_Node.mln_Succ != NULL)
		closewin(ww);
	UnregisterWorkbench(wbport);
	handler();
	FreeMenus(wbmenus);
	FreeVisualInfo(vi);
	freearrowimages();
	t[0].ti_Tag = ICONCTRLA_SetGlobalScreen;	t[0].ti_Data = (IPTR)NULL;
	t[1].ti_Tag = TAG_DONE;
	IconControlA(NULL, t);
	UnlockPubScreen(NULL, scr);
	/* the display goes back when the last screen closes */
	CloseWorkBench();
done:
	DeleteMsgPort(wbport);
	DeleteMsgPort(idport);
	closelibs();
	return rc;
}
