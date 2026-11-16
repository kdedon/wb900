/*
    Copyright (C) 1995-2003, The AROS Development Team. All rights reserved.
*/

#include <exec/memory.h>
#include <dos/dos.h>
#include <intuition/intuition.h>
#include <intuition/imageclass.h>
#include <intuition/gadgetclass.h>
#include <libraries/gadtools.h>
#include <graphics/gfx.h>
#include <utility/hooks.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/gadtools.h>
#include <proto/utility.h>

#include "global.h"
#include "req.h"

/****************************************************************************************/


#define BORDER_SPACING_X 4
#define BORDER_SPACING_Y 4

#define GAD_SPACING_X 8
#define GAD_SPACING_Y 4

#define GAD_EXTRA_WIDTH 16
#define GAD_EXTRA_HEIGHT 6

enum {GAD_FIND_TEXT = 1,
      GAD_FIND_OK,
      GAD_FIND_CANCEL};

enum {GAD_GOTO_STRING = 1,
      GAD_GOTO_OK,
      GAD_GOTO_CANCEL};

/****************************************************************************************/


static struct RastPort  temprp;
static struct Window    *gotowin, *findwin;
static struct Gadget    *gotogad, *findgad, *gotogadlist, *findgadlist;
static struct NewGadget ng;
static WORD             fontwidth, fontheight;

static char             searchtext[256];

/****************************************************************************************/

/*
 * A requester window: a string or integer gadget of the given kind above
 * OK and Cancel, whose IDs are 1, 2 and 3.
 */
static struct Window *MakeReq(CONST_STRPTR title, ULONG kind, struct TagItem *kindtags,
                              WORD minwidth, struct Gadget **glist, struct Gadget **strgad)
{
    struct Window *w = NULL;
    struct Gadget *gad;
    struct TagItem t[16];
    WORD winwidth, winheight, gadwidth, gadheight;
    WORD strwidth, wd;

    fontwidth = dri->dri_Font->tf_XSize;
    fontheight = dri->dri_Font->tf_YSize;

    InitRastPort(&temprp);
    SetFont(&temprp, dri->dri_Font);

    ng.ng_TextAttr = NULL;
    ng.ng_UserData = NULL;
    ng.ng_VisualInfo = vi;
    ng.ng_GadgetText = NULL;

    gad = CreateContext(glist);

    gadheight = fontheight + GAD_EXTRA_HEIGHT;

    winheight = scr->WBorTop + fontheight + 1 +
                scr->WBorBottom +
                BORDER_SPACING_Y * 2 +
                gadheight * 2 +
                GAD_SPACING_Y;

    gadwidth = TextLength(&temprp, MSG(MSG_OK), strlen(MSG(MSG_OK)));
    wd = TextLength(&temprp, MSG(MSG_CANCEL), strlen(MSG(MSG_CANCEL)));
    if (wd > gadwidth) gadwidth = wd;

    gadwidth += GAD_EXTRA_WIDTH;

    strwidth = gadwidth * 2 + GAD_SPACING_X;

    if (strwidth < minwidth) strwidth = minwidth;

    winwidth = scr->WBorLeft +
               scr->WBorRight +
               BORDER_SPACING_X * 2 +
               strwidth;

    ng.ng_LeftEdge = scr->WBorLeft + BORDER_SPACING_X;
    ng.ng_TopEdge = scr->WBorTop + fontheight + 1 + BORDER_SPACING_Y;
    ng.ng_Width = strwidth;
    ng.ng_Height = gadheight;
    ng.ng_GadgetID = 1;
    ng.ng_Flags = PLACETEXT_IN;

    *strgad = gad = CreateGadgetA(kind, gad, &ng, kindtags);

    ng.ng_TopEdge += gadheight + GAD_SPACING_Y;
    ng.ng_Width = gadwidth;
    ng.ng_GadgetText = MSG(MSG_OK);
    ng.ng_GadgetID = 2;

    gad = CreateGadgetA(BUTTON_KIND, gad, &ng, NULL);

    ng.ng_LeftEdge = winwidth - scr->WBorRight - BORDER_SPACING_X - gadwidth;
    ng.ng_GadgetText = MSG(MSG_CANCEL);
    ng.ng_GadgetID = 3;

    gad = CreateGadgetA(BUTTON_KIND, gad, &ng, NULL);

    if (gad)
    {
        t[0].ti_Tag = WA_CustomScreen;  t[0].ti_Data = (IPTR)scr;
        t[1].ti_Tag = WA_Left;          t[1].ti_Data = scr->MouseX - (winwidth / 2);
        t[2].ti_Tag = WA_Top;           t[2].ti_Data = scr->MouseY - (winheight / 2);
        t[3].ti_Tag = WA_Width;         t[3].ti_Data = winwidth;
        t[4].ti_Tag = WA_Height;        t[4].ti_Data = winheight;
        t[5].ti_Tag = WA_AutoAdjust;    t[5].ti_Data = TRUE;
        t[6].ti_Tag = WA_Title;         t[6].ti_Data = (IPTR)title;
        t[7].ti_Tag = WA_CloseGadget;   t[7].ti_Data = TRUE;
        t[8].ti_Tag = WA_DepthGadget;   t[8].ti_Data = TRUE;
        t[9].ti_Tag = WA_DragBar;       t[9].ti_Data = TRUE;
        t[10].ti_Tag = WA_Activate;     t[10].ti_Data = TRUE;
        t[11].ti_Tag = WA_SimpleRefresh; t[11].ti_Data = TRUE;
        t[12].ti_Tag = WA_IDCMP;        t[12].ti_Data = IDCMP_CLOSEWINDOW |
                                                        IDCMP_REFRESHWINDOW |
                                                        IDCMP_VANILLAKEY |
                                                        BUTTONIDCMP |
                                                        STRINGIDCMP;
        t[13].ti_Tag = WA_Gadgets;      t[13].ti_Data = (IPTR)*glist;
        t[14].ti_Tag = TAG_DONE;

        w = OpenWindowTagList(NULL, t);
    }

    if (!w)
    {
        FreeGadgets(*glist);
        *glist = NULL;
    } else {
        GT_RefreshWindow(w, NULL);
        ActivateGadget(*strgad, w, NULL);
    }

    return w;
}

/****************************************************************************************/

static void KillReq(struct Window **w, struct Gadget **glist, ULONG *mask)
{
    if (*w)
    {
        CloseWindow(*w);*w = NULL;*mask = 0;
        ActivateWindow(win);
    }

    if (*glist)
    {
        FreeGadgets(*glist);*glist = NULL;
    }
}

/****************************************************************************************/


void CleanupRequesters(void)
{
    if (gotowin) Kill_Goto_Requester();
    if (findwin) Kill_Find_Requester();
}

/****************************************************************************************/


void Make_Goto_Requester(void)
{
    struct TagItem t[3];

    if (gotowin) return;

    t[0].ti_Tag = GTIN_MaxChars;        t[0].ti_Data = 8;
    t[1].ti_Tag = STRINGA_Justification; t[1].ti_Data = GACT_STRINGCENTER;
    t[2].ti_Tag = TAG_DONE;

    gotowin = MakeReq(MSG(MSG_JUMP_TITLE), INTEGER_KIND, t, 0, &gotogadlist, &gotogad);
    if (gotowin)
        gotomask = 1L << gotowin->UserPort->mp_SigBit;
}

/****************************************************************************************/

BOOL Handle_Goto_Requester(LONG *line)
{
    struct IntuiMessage *msg;
    IPTR l;
    BOOL killreq = FALSE, rc = FALSE;

    while ((msg = GT_GetIMsg(gotowin->UserPort)))
    {
        switch (msg->Class)
        {
            case IDCMP_REFRESHWINDOW:
                GT_BeginRefresh(gotowin);
                GT_EndRefresh(gotowin, TRUE);
                break;

            case IDCMP_CLOSEWINDOW:
                killreq = TRUE;
                break;

            case IDCMP_GADGETUP:
                switch (((struct Gadget *)msg->IAddress)->GadgetID)
                {
                    case GAD_GOTO_CANCEL:
                        killreq = TRUE;
                        break;

                    case GAD_GOTO_STRING:
                    case GAD_GOTO_OK:
                        GT_GetGadgetAttrs(gotogad, gotowin, NULL, GTIN_Number, &l, TAG_DONE);
                        rc = TRUE;
                        break;

                } /* switch (((struct Gadget *)msg->IAddress)->GadgetID) */
                break;

            case IDCMP_VANILLAKEY:
                switch(ToUpper(msg->Code))
                {
                    case 27:
                        killreq = TRUE;
                        break;

                    case 9:
                    case 'G':
                        ActivateGadget(gotogad, gotowin, NULL);
                        break;

                } /* switch(msg->Code) */
                break;

        } /* switch (msg->Class) */
        GT_ReplyIMsg(msg);

    }  /* while ((msg = GT_GetIMsg(gotowin))) */

    if (killreq) Kill_Goto_Requester();

    if (rc) *line = l;

    return rc;
}

/****************************************************************************************/

void Kill_Goto_Requester(void)
{
    KillReq(&gotowin, &gotogadlist, &gotomask);
}

/****************************************************************************************/

void Make_Find_Requester(void)
{
    struct TagItem t[3];

    if (findwin) return;

    t[0].ti_Tag = GTST_MaxChars;        t[0].ti_Data = sizeof(searchtext) - 1;
    t[1].ti_Tag = GTST_String;          t[1].ti_Data = (IPTR)searchtext;
    t[2].ti_Tag = TAG_DONE;

    findwin = MakeReq(MSG(MSG_FIND_TITLE), STRING_KIND, t, 250, &findgadlist, &findgad);
    if (findwin)
        findmask = 1L << findwin->UserPort->mp_SigBit;
}

/****************************************************************************************/

WORD Handle_Find_Requester(char **text)
{
    struct IntuiMessage *msg;
    char *sp;
    BOOL killreq = FALSE;
    WORD rc = 0;

    while ((msg = GT_GetIMsg(findwin->UserPort)))
    {
        switch (msg->Class)
        {
            case IDCMP_REFRESHWINDOW:
                GT_BeginRefresh(findwin);
                GT_EndRefresh(findwin, TRUE);
                break;

            case IDCMP_CLOSEWINDOW:
                killreq = TRUE;
                break;

            case IDCMP_GADGETUP:
                switch (((struct Gadget *)msg->IAddress)->GadgetID)
                {
                    case GAD_FIND_CANCEL:
                        killreq = TRUE;
                        break;

                    case GAD_FIND_TEXT:
                    case GAD_FIND_OK:
                        GT_GetGadgetAttrs(findgad, findwin, NULL, GTST_String, &sp, TAG_DONE);
                        strncpy(searchtext, sp, sizeof(searchtext) - 1);

                        rc = SEARCH_NEW;
                        break;

                } /* switch (((struct Gadget *)msg->IAddress)->GadgetID) */
                break;

            case IDCMP_VANILLAKEY:
                switch(ToUpper(msg->Code))
                {
                    case 27:
                        killreq = TRUE;
                        break;

                    case 9:
                    case 'S':
                    case 'F':
                        ActivateGadget(findgad, findwin, NULL);
                        break;

                    case 13:
                    case 'N':
                        rc = SEARCH_NEXT;
                        break;

                    case 'P':
                        rc = SEARCH_PREV;
                        break;

                } /* switch(msg->Code) */
                break;

        } /* switch (msg->Class) */
        GT_ReplyIMsg(msg);

    } /* while ((msg = GT_GetIMsg(findwin))) */

    if (killreq) Kill_Find_Requester();

    if (rc) *text = searchtext;

    return rc;
}

/****************************************************************************************/

void Kill_Find_Requester(void)
{
    KillReq(&findwin, &findgadlist, &findmask);
}

/****************************************************************************************/
