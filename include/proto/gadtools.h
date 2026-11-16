/*
 * gadtools.library's calls, as AROS's <proto/gadtools.h> gives them,
 * through the caller's GadToolsBase, and amiga.lib's varargs forms.
 */
#ifndef PROTO_GADTOOLS_H
#define PROTO_GADTOOLS_H

#include <aros/libcall.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <utility/tagitem.h>
#include <libraries/gadtools.h>
#include <lvo/gadtools_lib.h>

/* Each tag and value is 32 bits: cast an int to ULONG */
struct Gadget *CreateGadget(ULONG kind, struct Gadget *previous,
    struct NewGadget *ng, Tag tag1, ...);
void GT_SetGadgetAttrs(struct Gadget *gad, struct Window *win,
    struct Requester *req, Tag tag1, ...);
LONG GT_GetGadgetAttrs(struct Gadget *gad, struct Window *win,
    struct Requester *req, Tag tag1, ...);
struct Menu *CreateMenus(struct NewMenu *newmenu, Tag tag1, ...);
BOOL LayoutMenus(struct Menu *menu, APTR vi, Tag tag1, ...);
BOOL LayoutMenuItems(struct MenuItem *menuitem, APTR vi, Tag tag1, ...);
APTR GetVisualInfo(struct Screen *screen, Tag tag1, ...);
void DrawBevelBox(struct RastPort *rport, WORD left, WORD top, WORD width,
    WORD height, Tag tag1, ...);

#endif
