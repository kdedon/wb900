/* SPDX-License-Identifier: MIT */
/* Workbench: the shared declarations of its files */
#ifndef WB_WORKBENCH_H
#define WB_WORKBENCH_H

#include <exec/exec.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <graphics/regions.h>
#include <graphics/layers.h>
#include <utility/utility.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <libraries/gadtools.h>
#include <workbench/workbench.h>
#include <workbench/icon.h>
#include <workbench/handler.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/layers.h>
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <proto/icon.h>
#include <proto/workbench.h>
#include <proto/utility.h>

#define PATHLEN		256
#define NAMELEN		32

/* An icon in a window */
struct WBIcon {
	struct MinNode	 in_Node;
	struct DiskObject *in_Obj;
	WORD		 in_X, in_Y;	/* frame's top left, inner coordinates */
	struct Rectangle in_Box;	/* GetIconRectangleA()'s, from in_X, in_Y */
	BOOL		 in_Selected;
	UBYTE		 in_Mark;	/* appicons()'s */
	struct DiskObject *in_App;	/* an AppIcon's, of which in_Obj is a copy */
	char		 in_Name[NAMELEN];	/* file, or volume with ':' */
};

#define HSCROLL		0
#define VSCROLL		1

/*
 * A drawer window, or the backdrop with the disks (ww_Path NULL).  Icon
 * positions are in the drawer's area, of which the window shows the part
 * at ww_OffX, ww_OffY; ww_Virt bounds the icons and that part.
 */
struct WBWin {
	struct MinNode	 ww_Node;
	struct Window	*ww_Win;
	STRPTR		 ww_Path;
	struct MinList	 ww_Icons;
	BOOL		 ww_ShowAll;
	WORD		 ww_OffX, ww_OffY;
	struct Rectangle ww_Virt;
	struct Gadget	 ww_Scroll[2];	/* HSCROLL, VSCROLL; none on the backdrop */
	struct PropInfo	 ww_Prop[2];
	struct Image	 ww_Knob[2];
	char		 ww_Title[NAMELEN];
};

#define FOREACH(t, n, l) \
	for ((n) = (t)(l)->mlh_Head; (n)->in_Node.mln_Succ; \
	    (n) = (t)(n)->in_Node.mln_Succ)
#define FOREACHWIN(n) \
	for ((n) = (struct WBWin *)wins.mlh_Head; (n)->ww_Node.mln_Succ; \
	    (n) = (struct WBWin *)(n)->ww_Node.mln_Succ)

extern struct DosLibrary *DOSBase;
extern struct GfxBase *GfxBase;
extern struct IntuitionBase *IntuitionBase;
extern struct Library *LayersBase, *UtilityBase, *GadToolsBase, *IconBase,
    *WorkbenchBase;
extern struct Screen *scr;
extern struct MinList wins;
extern struct WBWin *root;
extern BOOL wbbackdrop;

/* drawer.c */
struct WBWin *openwin(STRPTR path, struct DiskObject *dobj);
void closewin(struct WBWin *ww);
struct WBWin *findwin(struct Window *w);
struct WBWin *findpath(STRPTR path);
void scan(struct WBWin *ww);
void freeicons(struct WBWin *ww);
void cleanup(struct WBWin *ww);
void drawicon(struct WBWin *ww, struct WBIcon *in);
void redraw(struct WBWin *ww);
void refresh(struct WBWin *ww);
struct WBIcon *hit(struct WBWin *ww, WORD x, WORD y);
BOOL fullpath(struct WBWin *ww, struct WBIcon *in, STRPTR buf);
void selecticon(struct WBWin *ww, struct WBIcon *in, BOOL sel);
void deselectall(struct WBWin *keep);
void lasso(struct WBWin *ww, struct Rectangle *r);
void moveicons(struct WBWin *ww, WORD dx, WORD dy);
void outline(struct WBWin *ww, WORD dx, WORD dy);
void update(STRPTR name);
void appicons(void);
void removeicon(struct WBWin *ww, struct WBIcon *in);
void addicon(struct WBWin *ww, STRPTR name, LONG x, LONG y);
void setscroll(struct WBWin *ww);
void scroll(struct WBWin *ww);

/* workbench.c */
extern struct MsgPort *idport;
ULONG strlen(CONST_STRPTR s);
char *strcpy(char *d, CONST_STRPTR s);
char *strcat(char *d, CONST_STRPTR s);
char *strncpy(char *d, CONST_STRPTR s, ULONG n);
APTR memset(APTR d, int c, ULONG n);
void windowmenus(struct Window *w);
void openicon(struct WBWin *ww, struct WBIcon *in);

#endif
