/* SPDX-License-Identifier: MIT */
/*
 * gadtools.library's varargs calls, through the caller's GadToolsBase.
 * The tags follow tag1 on the stack as a TagItem array, so the caller
 * passes every tag and value as 32 bits (a ULONG, pointer or long
 * constant).
 */
#include <proto/gadtools.h>

extern struct Library *GadToolsBase;

struct Gadget *CreateGadget(ULONG kind, struct Gadget *previous,
    struct NewGadget *ng, Tag tag1, ...)
{
	return CreateGadgetA(kind, previous, ng, (struct TagItem *)&tag1);
}

void GT_SetGadgetAttrs(struct Gadget *gad, struct Window *win,
    struct Requester *req, Tag tag1, ...)
{
	GT_SetGadgetAttrsA(gad, win, req, (struct TagItem *)&tag1);
}

LONG GT_GetGadgetAttrs(struct Gadget *gad, struct Window *win,
    struct Requester *req, Tag tag1, ...)
{
	return GT_GetGadgetAttrsA(gad, win, req, (struct TagItem *)&tag1);
}

struct Menu *CreateMenus(struct NewMenu *newmenu, Tag tag1, ...)
{
	return CreateMenusA(newmenu, (struct TagItem *)&tag1);
}

BOOL LayoutMenus(struct Menu *menu, APTR vi, Tag tag1, ...)
{
	return LayoutMenusA(menu, vi, (struct TagItem *)&tag1);
}

BOOL LayoutMenuItems(struct MenuItem *menuitem, APTR vi, Tag tag1, ...)
{
	return LayoutMenuItemsA(menuitem, vi, (struct TagItem *)&tag1);
}

APTR GetVisualInfo(struct Screen *screen, Tag tag1, ...)
{
	return GetVisualInfoA(screen, (struct TagItem *)&tag1);
}

void DrawBevelBox(struct RastPort *rport, WORD left, WORD top, WORD width,
    WORD height, Tag tag1, ...)
{
	DrawBevelBoxA(rport, left, top, width, height, (struct TagItem *)&tag1);
}
