/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.
    Copyright (C) 2001-2003, The MorphOS Development Team. All Rights Reserved.
*/

#include <exec/memory.h>
#include <graphics/layers.h>
#include <graphics/gfx.h>
#include <intuition/intuition.h>
#include <intuition/imageclass.h>
#include <intuition/windecorclass.h>
#include <intuition/gadgetclass.h>
#include <intuition/extensions.h>
#include <utility/tagitem.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/utility.h>
#include <proto/intuition.h>
#include <proto/layers.h>
#include <exec/ports.h>
#include "intuition_intern.h"

#include "intuition_customize.h"
#include "inputhandler.h"
#include "inputhandler_actions.h"
#include "boopsigadgets.h"

#define DEBUG DEBUG_OpenWindow

struct OpenWindowActionMsg
{
    struct IntuiActionMsg    msg;
    struct Window               *window;
    struct BitMap               *bitmap;
    struct Hook                 *backfillhook;
    struct Region               *shape;
    struct Hook                 *shapehook;
    struct Layer                *parentlayer;
    BOOL                             invisible;
    BOOL                             success;
    /* composited window (WA_Opaqueness / WA_AlphaSurface): LA_Composited layer */
    BOOL                             composited;
    BOOL                             alphasurface;
    UBYTE                            opaqueness;
    struct Region                   *inputregion;   /* WA_InputRegion, copied by layers.library */
};

static VOID int_openwindow(struct OpenWindowActionMsg *msg,
                           struct IntuitionBase *IntuitionBase);

/*
 * The window tags that switch one window flag on or off, in the form
 * utility.library/PackBoolTags() takes. WA_Zoom is in here because the
 * presence of a zoom box also sets a flag.
 */
static const struct TagItem windowflagtags[] =
{
    { WA_SizeGadget     , WFLG_SIZEGADGET       },
    { WA_DragBar        , WFLG_DRAGBAR          },
    { WA_DepthGadget    , WFLG_DEPTHGADGET      },
    { WA_CloseGadget    , WFLG_CLOSEGADGET      },
    { WA_Backdrop       , WFLG_BACKDROP         },
    { WA_ReportMouse    , WFLG_REPORTMOUSE      },
    { WA_NoCareRefresh  , WFLG_NOCAREREFRESH    },
    { WA_Borderless     , WFLG_BORDERLESS       },
    { WA_Activate       , WFLG_ACTIVATE         },
    { WA_RMBTrap        , WFLG_RMBTRAP          },
    { WA_WBenchWindow   , WFLG_WBENCHWINDOW     },
    { WA_SizeBRight     , WFLG_SIZEBRIGHT       },
    { WA_SizeBBottom    , WFLG_SIZEBBOTTOM      },
    { WA_GimmeZeroZero  , WFLG_GIMMEZEROZERO    },
    { WA_NewLookMenus   , WFLG_NEWLOOKMENUS     },
    { WA_ToolBox        , WFLG_TOOLBOX          },
    { WA_Zoom           , WFLG_HASZOOM          },
    { TAG_DONE          , 0                     }
};

/* The same for the flags kept in IntWindow.moreflags */
static const struct TagItem windowmoreflagtags[] =
{
    { WA_NotifyDepth    , WMFLG_NOTIFYDEPTH     },
    { WA_MenuHelp       , WMFLG_MENUHELP        },
    { WA_PointerDelay   , WMFLG_POINTERDELAY    },
    { WA_TabletMessages , WMFLG_TABLETMESSAGES  },
    { TAG_DONE          , 0                     }
};

/*****************************************************************************

    NAME */
        AROS_LH1(struct Window *, OpenWindow,

/*  SYNOPSIS */
        AROS_LHA(struct NewWindow *, newWindow, A0),

/*  LOCATION */
        struct IntuitionBase *, IntuitionBase, 34, Intuition)

/*  FUNCTION
        Opens a new window with the characteristics specified in
        newWindow.

    INPUTS
        newWindow - How you would like your new window.

    RESULT
        A pointer to the new window or NULL if it couldn't be opened. Reasons
        for this might be lack of memory or illegal attributes.

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO
        CloseWindow(), ModifyIDCMP()

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct GfxBase              *GfxBase = GetPrivIBase(IntuitionBase)->GfxBase;
    struct Library              *UtilityBase = GetPrivIBase(IntuitionBase)->UtilityBase;
    struct OpenWindowActionMsg   msg;
    struct NewWindow                 nw;
    struct Window                       *w = NULL, *helpgroupwindow = NULL, *parentwin = NULL;
    struct TagItem                      *tag, *shapeti = NULL, *shapehookti = NULL;
    struct TagItem                    *tagList;
    struct RastPort                     *rp;
    struct Hook                         *backfillhook = LAYERS_BACKFILL, *shapehook = NULL;
    struct Region                       *shape = NULL;
    struct IBox                         *zoombox = NULL;
    struct Image                        *AmigaKey = NULL;
    struct Image                        *Checkmark = NULL;
    struct Layer                        *parentl = NULL;
    STRPTR                               pubScreenName = NULL;
    BOOL                                 pubScreenNameSet = FALSE;
    UBYTE                               *screenTitle = NULL;
    BOOL                                 autoAdjust = FALSE, pubScreenFallBack = FALSE;
    ULONG                                innerWidth = ~0;
    ULONG                                innerHeight = ~0;
    WORD                                 mousequeue = DEFAULTMOUSEQUEUE;
    WORD                                 repeatqueue = 3; /* stegerg: test on my Amiga suggests this */
    ULONG                                moreFlags = 0;
    ULONG                                helpgroup = 0;
    ULONG                                extrabuttons = 0, extrabuttonsid = ETI_Dummy;
  //ULONG                                      lock;
    ULONG                            windowinvisible = FALSE;
    BOOL                                 composited = FALSE, alphasurface = FALSE;
    struct Region                       *inputregion = NULL;
    ULONG                                opaqueness = 255;
    BOOL                                 driver_init_done = FALSE, have_helpgroup = FALSE;
    BOOL                             do_setwindowpointer = FALSE;


    nw = *newWindow;

#define WFLG_PRIVATEFLAGS (WFLG_WINDOWREFRESH |\
WFLG_WINDOWTICKED  | WFLG_VISITOR       | \
WFLG_ZOOMED        |\
WFLG_WINDOWACTIVE )
    /* jDc*/
    /* WFLG_WBENCHWINDOW  | \*/
    /* this is used by WORKBENCH! */
    /* WFLG_HASZOOM       | \*/
    /* do NOT filter this! how do you think apps could manage to get a zoom image with struct NewWindow? */

    nw.Flags &= ~WFLG_PRIVATEFLAGS;

    if (newWindow->Flags & WFLG_NW_EXTENDED)
    {
        tagList = ((struct ExtNewWindow *)newWindow)->Extension;
    }
    else
    {
        tagList = NULL;
    }

    if (tagList)
    {

    }

    /*
     * Evaluate the tags. The tag functions accept a NULL list: GetTagData()
     * returns the default and NextTagItem() returns NULL at once, so without
     * tags everything keeps the value taken from the NewWindow structure.
     */

    /* Look at WA_Flags first, since boolean tags override part of it
     * even if they appear before it.
     */
    nw.Flags |= (GetTagData(WA_Flags, nw.Flags, tagList) & ~WFLG_PRIVATEFLAGS);

    /*
     * The boolean tags, each setting or clearing its flag in the order they
     * appear in the list. None of the tags handled in the loop below changes
     * one of these flags, so it does not matter that this is done first.
     */
    nw.Flags  = PackBoolTags(nw.Flags, tagList, (struct TagItem *)windowflagtags);
    moreFlags = PackBoolTags(moreFlags, tagList, (struct TagItem *)windowmoreflagtags);

    while ((tag = NextTagItem (&tagList)))
    {
        /* ASSERT_VALID_PTR_ROMOK(tag); */

        switch ((ULONG)tag->ti_Tag)
        {
        case WA_Left:
            nw.LeftEdge     = tag->ti_Data;
            break;

        case WA_Top:
            nw.TopEdge          = tag->ti_Data;
            break;

        case WA_Width:
            nw.Width            = tag->ti_Data;
            break;

        case WA_Height:
            nw.Height           = tag->ti_Data;
            break;

        case WA_IDCMP:
            nw.IDCMPFlags   = tag->ti_Data;
            break;

        case WA_MinWidth:
            nw.MinWidth     = tag->ti_Data;
            break;

        case WA_MinHeight:
            nw.MinHeight    = tag->ti_Data;
            break;

        case WA_MaxWidth:
            nw.MaxWidth     = tag->ti_Data;
            break;

        case WA_MaxHeight:
            nw.MaxHeight    = tag->ti_Data;
            break;

        case WA_Gadgets:
            nw.FirstGadget  = (struct Gadget *)(tag->ti_Data);
            break;

        case WA_Title:
            nw.Title    = (UBYTE *)(tag->ti_Data);
            break;

        case WA_ScreenTitle:
            screenTitle = (UBYTE *)tag->ti_Data;
            break;

        case WA_AutoAdjust:
            autoAdjust  = (tag->ti_Data != 0);
            break;

        case WA_InnerWidth:
            innerWidth  = tag->ti_Data;
            break;

        case WA_InnerHeight:
            innerHeight = tag->ti_Data;
            break;

        case WA_Zoom:
            /* WFLG_HASZOOM is set through windowflagtags */
            zoombox = (struct IBox *)tag->ti_Data;

            break;

        case WA_DetailPen:
            if (nw.DetailPen == 0xFF)
                nw.DetailPen = tag->ti_Data;
            break;

        case WA_BlockPen:
            if (nw.BlockPen == 0xFF)
                nw.BlockPen = tag->ti_Data;
            break;

        case WA_CustomScreen:
            nw.Screen = (struct Screen *)(tag->ti_Data);
            nw.Type = CUSTOMSCREEN;
            break;

        case WA_SuperBitMap:
            nw.Flags |= WFLG_SUPER_BITMAP;
            nw.BitMap = (struct BitMap *)(tag->ti_Data);
            break;

        case WA_SimpleRefresh:
            if (tag->ti_Data)
            {
                nw.Flags |= WFLG_SIMPLE_REFRESH;
                nw.Flags &= ~WFLG_SUPER_BITMAP;
            }
            break;

        case WA_SmartRefresh:
            if (tag->ti_Data)
                nw.Flags &= ~(WFLG_SIMPLE_REFRESH | WFLG_SUPER_BITMAP);
            break;

        case WA_PubScreenFallBack:
            pubScreenFallBack = (tag->ti_Data ? TRUE : FALSE);
            break;

        case WA_PubScreenName:
            pubScreenNameSet = TRUE;
            pubScreenName = (STRPTR)tag->ti_Data;
            break;

        case WA_PubScreen:
            nw.Type = PUBLICSCREEN;
            nw.Screen = (struct Screen *)tag->ti_Data;
            break;

        case WA_BackFill:
            backfillhook = (struct Hook *)tag->ti_Data;
            break;

        case WA_MouseQueue:
            mousequeue = tag->ti_Data;
            break;

            /* These two are not implemented in AmigaOS */
        case WA_WindowName:
        case WA_Colors:
            break;

        case WA_RptQueue:
            repeatqueue = tag->ti_Data;
            break;

        case WA_Checkmark:
            Checkmark = (struct Image *)tag->ti_Data;
            break;

        case WA_AmigaKey:
            AmigaKey = (struct Image *)tag->ti_Data;
            break;

        case WA_HelpGroup:
            helpgroup = (ULONG)tag->ti_Data;
            have_helpgroup = TRUE;
            break;

        case WA_HelpGroupWindow:
            helpgroupwindow = (struct Window *)tag->ti_Data;
            break;

        case WA_ShapeRegion:
                shapeti = tag;
            shape = (struct Region *)tag->ti_Data;
            break;

            case WA_ShapeHook:
                shapehookti = tag;
                shapehook = (struct Hook *)tag->ti_Data;
            break;

        case WA_Hidden:
            windowinvisible = tag->ti_Data;
            break;

        case WA_Pointer:
        case WA_BusyPointer:
            do_setwindowpointer = TRUE;
            break;

        default:
            /*
             * The tags numbered from WA_Dummy + 100 up lie apart from the
             * ones above. They have a switch of their own, so that each of
             * the two covers a dense range of numbers.
             */
            switch ((ULONG)tag->ti_Tag)
            {
            case WA_ExtraTitlebarGadgets:
                extrabuttons = (ULONG)tag->ti_Data;
                break;

            case WA_ExtraGadgetsStartID:
                extrabuttonsid = (ULONG)tag->ti_Data;
                break;

            case WA_ExtraGadget_Iconify:
            case WA_ExtraGadget_Lock:
            case WA_ExtraGadget_MUI:
            case WA_ExtraGadget_PopUp:
            case WA_ExtraGadget_Snapshot:
            case WA_ExtraGadget_Jump:
                {
                    /* Each tag's ETG_ flag is the bit of its distance from the first tag */
                    ULONG etg = 1UL << (tag->ti_Tag - WA_ExtraGadget_Iconify);


                    if (tag->ti_Data)
                    {
                        extrabuttons |= etg;
                    }
                    else
                    {
                        extrabuttons &= ~etg;
                    }
                }
                break;

            case WA_Parent:
                parentwin = ((struct Window *)tag->ti_Data);
                parentl   = parentwin->WLayer;
                break;

            /*
             * Composited windows: the window's layer renders into a surface
             * of its own that the display composes (layers.library
             * LA_Composited). Opt-in only; without these tags nothing changes.
             */
            case WA_Opaqueness:
                opaqueness = (tag->ti_Data > 255) ? 255 : tag->ti_Data;
                composited = TRUE;
                break;

            case WA_AlphaSurface:
                alphasurface = tag->ti_Data ? TRUE : FALSE;
                if (alphasurface)
                    composited = TRUE;
                break;

            /* Where the window takes input, see LA_InputShape */
            case WA_InputRegion:
                inputregion = (struct Region *)tag->ti_Data;
                break;

            } /* switch Tag, WA_Dummy + 100 up */
            break;

        } /* switch Tag */

    } /* while ((tag = NextTagItem (&tagList))) */

    /*
     * A composited window keeps all its pixels in its surface, so it is
     * a smart refresh window: it never gets refresh events from exposure.
     */
    if (composited)
        nw.Flags &= ~(WFLG_SIMPLE_REFRESH | WFLG_SUPER_BITMAP);

    if (nw.Flags & WFLG_SIZEGADGET)
    {
        if (!(nw.Flags & (WFLG_SIZEBRIGHT | WFLG_SIZEBBOTTOM)))
        {
            nw.Flags |= WFLG_SIZEBRIGHT;
        }

        //jDc: tested behavior of intuition68k
        nw.Flags |= WFLG_HASZOOM;
    }
    else
    {
        nw.Flags &= ~(WFLG_SIZEBRIGHT|WFLG_SIZEBBOTTOM);
    }

    if (nw.Flags & WFLG_BORDERLESS)
    {
        nw.Flags &= ~(WFLG_SIZEBRIGHT|WFLG_SIZEBBOTTOM|WFLG_SIZEGADGET);
    }

    /* Find out on which Screen the window must open */

        /* (cyfm 03/03/03 check for nw.Type == PUBLICSCREEN as well, some programs
         *  like TurboPrint GraphicPublisher specify {WA_PubScreen, NULL} and want
         *  to open on the default public screen that way
         */
    if (pubScreenNameSet || (nw.Type == PUBLICSCREEN && nw.Screen == NULL))
    {
        struct Screen *pubs = 0;

        moreFlags |= WMFLG_DO_UNLOCKPUBSCREEN;
        pubs = LockPubScreen(pubScreenName);
        if (!pubs && pubScreenFallBack)
        {
            nw.Screen = LockPubScreen(NULL);
        }
        if (pubs)
        {
            nw.Screen = pubs;
        }
        nw.Type = PUBLICSCREEN;
        if (nw.Screen) nw.Flags |= WFLG_VISITOR;
    }
    else if (nw.Type == PUBLICSCREEN)
    {
        /* There is no LockPubScreen() with a "struct Screen *" argument, so we have to do
         * the necessary steps ourself
         */
        LockPubScreenList();
        if (GetPrivScreen(nw.Screen)->pubScrNode)
        {
            GetPrivScreen(nw.Screen)->pubScrNode->psn_VisitorCount++;
            moreFlags |= WMFLG_DO_UNLOCKPUBSCREEN;
            nw.Flags  |= WFLG_VISITOR;
        }
        else
        {
            /* The screen wasn't a PublicScreen */
            UnlockPubScreenList();

            goto failexit;
        }
        UnlockPubScreenList();
    }

    if (nw.Type == WBENCHSCREEN)
    {
        if (GetPrivIBase(IntuitionBase)->pubScrGlobalMode & SHANGHAI)
            nw.Screen = LockPubScreen(NULL);
        else
            nw.Screen = LockPubScreen("Workbench");
        if (nw.Screen)
        {
            moreFlags |= WMFLG_DO_UNLOCKPUBSCREEN;
            nw.Flags  |= WFLG_VISITOR;
        }
    }

    if (nw.Screen == NULL) {

        goto failexit;
    }

    w  = AllocMem (sizeof(struct IntWindow), MEMF_CLEAR);

    /* nlorentz: For now, creating a rastport becomes the responsibility of
       intui_OpenWindow(). This is because intui_OpenWindow() in
       config/hidd/intuition_driver.c must call CreateUpfrontLayer(),
       and that will create a rastport for the layer/window, and we don't
       want two rastports pr. window.
       Alternatively we may create a layers_driver.c driver for layers,
       and then call CreateUpfrontLayer() here from openwindow.
       For the Amiga window<-->X11 window stuff, the layers driver
       would just allocate a layer struct, a rastport and
       put the rasport into layer->RastPort, so we
       could get it inside this routine and put it into
       window->RPort;.

    */

    if (NULL == w) {

        goto failexit;
    }

    IPTR        userbuffersize;

    GetAttr(WDA_UserBuffer, ((struct IntScreen *)(nw.Screen))->WinDecorObj, &userbuffersize);

    if (userbuffersize)
    {
        ((struct IntWindow *)w)->DecorUserBufferSize = userbuffersize;
        ((struct IntWindow *)w)->DecorUserBuffer = (IPTR) AllocMem(userbuffersize, MEMF_ANY | MEMF_CLEAR);
        if (0 == ((struct IntWindow *)w)->DecorUserBuffer) {

                goto failexit;
        }
    }

    struct wdpInitWindow       initmsg;
    BOOL                       ok;

    initmsg.MethodID           = WDM_INITWINDOW;
    initmsg.wdp_TrueColor      = (((struct IntScreen *) nw.Screen)->DInfo.dri_Flags & DRIF_DIRECTCOLOR) ? TRUE : FALSE;
    initmsg.wdp_UserBuffer     = ((struct IntWindow *)w)->DecorUserBuffer;
    initmsg.wdp_ScreenUserBuffer   = ((struct IntScreen *) nw.Screen)->DecorUserBuffer;
    initmsg.wdp_Screen             = nw.Screen;

    ok = DoMethodA(((struct IntScreen *)(nw.Screen))->WinDecorObj, (Msg)&initmsg);

    if (!ok) {

        goto failexit;
    }

    w->WScreen = nw.Screen;

    if (!ModifyIDCMP (w, nw.IDCMPFlags)) {

        goto failexit;
    }

    ((struct IntWindow *)w)->extrabuttons = extrabuttons;
    ((struct IntWindow *)w)->extrabuttonsid = extrabuttonsid;

    /*    w->RPort     = rp; */

    //w->FirstGadget = nw.FirstGadget;

    w->DetailPen = (nw.DetailPen != 0xFF) ? nw.DetailPen : w->WScreen->DetailPen;
    w->BlockPen  = (nw.BlockPen != 0xFF)  ? nw.BlockPen  : w->WScreen->BlockPen;

    /* Copy flags */
    w->Flags = nw.Flags;
    w->MoreFlags = moreFlags;

    if (!(w->Flags & WFLG_BORDERLESS))
    {
        w->BorderLeft   = w->WScreen->WBorLeft;
        w->BorderRight  = w->WScreen->WBorRight;
        w->BorderTop    = w->WScreen->WBorTop;
        w->BorderBottom = w->WScreen->WBorBottom;
    }

    if (nw.Title || (w->Flags & (WFLG_DRAGBAR | WFLG_CLOSEGADGET | WFLG_DEPTHGADGET)))
    {
        /* this is a hack. the correct way to adjust (increase if necessary)
           the w->Border??? values would be to check all GACT_???BORDER gadgets
           (inclusive sysgadgets which are GACT_????BORDER gadgets as well) in
           nw.FirstGadget (or WA_Gadgets tag) and all sysgadgets and then
           make sure that each window border is big enough so that none of these
           gadgets extends outside the window border area */

        /* Georg Steger: ??? font ??? */
        if (w->WScreen->Font)
            w->BorderTop += ((struct IntScreen *)(w->WScreen))->DInfo.dri_Font->tf_YSize + 1;
        else
            w->BorderTop += GfxBase->DefaultFont->tf_YSize + 1;
    }

    /* look for GACT_???BORDER gadgets which increase the BorderSizes */

    if (nw.FirstGadget)
    {
        struct Gadget *gad;

        for(gad = nw.FirstGadget; gad; gad = gad->NextGadget)
        {
            WORD gadx1, gady1, gadx2, gady2;

            if (gad->Activation & GACT_LEFTBORDER)
            {
                /* may never be GFLG_RELRIGHT / GFLG_RELWIDTH */

                gadx2 = gad->LeftEdge + gad->Width - 1;
                if (gadx2 >= w->BorderLeft) w->BorderLeft = gadx2/* + 1*/;
            }

            if (gad->Activation & GACT_TOPBORDER)
            {
                /* may never be GFLG_RELBOTTOM / GFLG_RELHEIGHT */

                gady2 = gad->TopEdge + gad->Height - 1;
                if (gady2 >= w->BorderTop) w->BorderTop = gady2/* + 1*/;
            }

            if (gad->Activation & GACT_RIGHTBORDER)
            {
                /* must be GFLG_RELRIGHT but never GFLG_RELWIDTH */

                gadx1 = -gad->LeftEdge;
                if (gadx1 >= w->BorderRight) w->BorderRight = gadx1/* + 1*/;
            }

            if (gad->Activation & GACT_BOTTOMBORDER)
            {
                /* must be GFLG_RELBOTTOM but never GFLG_RELHEIGHT */

                gady1 = -gad->TopEdge;
                if (gady1 >= w->BorderBottom) w->BorderBottom = gady1/* + 1*/;
            }

        } /* for(gad = nw.FirstGadget; gad; gad = gad->NextGadget) */

    } /* if (nw.FirstGadget) */

    if (!(w->Flags & WFLG_SIZEBRIGHT)) if (w->BorderRight > w->WScreen->WBorRight)
    {
        w->Flags |= WFLG_SIZEBRIGHT;
    }

    if (!(w->Flags & WFLG_SIZEBBOTTOM)) if (w->BorderBottom > w->WScreen->WBorBottom)
    {
        w->Flags |= WFLG_SIZEBBOTTOM;
    }

    //    if ((w->Flags & WFLG_SIZEGADGET) &&
    //       (w->Flags & (WFLG_SIZEBRIGHT | WFLG_SIZEBBOTTOM)))
    {
        IPTR                      sizewidth = 16, sizeheight = 16;
        struct Image         *im;
        struct DrawInfo *dri;

        if ((dri = GetScreenDrawInfo(w->WScreen)))
        {
            struct TagItem imtags[] =
            {
                {SYSIA_DrawInfo , (STACKIPTR)dri                                                                                    },
                {SYSIA_Which        , SIZEIMAGE                                                                                             },
                {SYSIA_Size         , w->WScreen->Flags & SCREENHIRES ? SYSISIZE_MEDRES : SYSISIZE_LOWRES        },
                {TAG_DONE                                                                                                                       }
            };

            if ((im = NewObjectA(NULL, SYSICLASS, imtags)))
            {
                GetAttr(IA_Width, (Object *)im, &sizewidth);
                GetAttr(IA_Height, (Object *)im, &sizeheight);

                DisposeObject(im);
            }
            FreeScreenDrawInfo(w->WScreen, dri);
        }

        if (w->Flags & WFLG_SIZEBRIGHT)
        {
            if (w->BorderRight < sizewidth) w->BorderRight = sizewidth;
        }

        if (w->Flags & WFLG_SIZEBBOTTOM)
        {
            if (w->BorderBottom < sizeheight) w->BorderBottom = sizeheight;
        }

        IW(w)->sizeimage_width = sizewidth;
        IW(w)->sizeimage_height = sizeheight;

        /* now increase window size if it's necessary */
    }

    IW(w)->DefaultWindowShapeHook.h_Entry = (HOOKFUNC)DefaultWindowShapeFunc;
    IW(w)->DefaultWindowShapeHook.h_SubEntry = (HOOKFUNC)w;
    IW(w)->DefaultWindowShapeHook.h_Data = (APTR)IntuitionBase;

    if (innerWidth != ~0) nw.Width = innerWidth + w->BorderLeft + w->BorderRight;
    if (innerHeight != ~0) nw.Height = innerHeight + w->BorderTop + w->BorderBottom;

    {
        LONG parentwidth;
        LONG parentheight;

        parentwidth  = parentwin ? parentwin->Width : w->WScreen->Width;
        parentheight = parentwin ? parentwin->Height : w->WScreen->Height;

        w->Width   = (nw.Width  != ~0) ? nw.Width  : parentwidth - nw.LeftEdge;
        w->Height  = (nw.Height != ~0) ? nw.Height : parentheight - nw.TopEdge;

        if (autoAdjust)
        {

            if (w->Width  > parentwidth)  w->Width  = parentwidth;
            if (w->Height > parentheight) w->Height = parentheight;

            if (nw.LeftEdge < 0) nw.LeftEdge = 0;
            if (nw.TopEdge < 0) nw.TopEdge = 0;

            if ((nw.LeftEdge + w->Width) > parentwidth)
                nw.LeftEdge = parentwidth - w->Width;

            if ((nw.TopEdge + w->Height) > parentheight)
                nw.TopEdge = parentheight - w->Height;
        }

        w->GZZWidth  = w->Width  - w->BorderLeft - w->BorderRight;
        w->GZZHeight = w->Height - w->BorderTop  - w->BorderBottom;
    }

    if (nw.LeftEdge < 0 || nw.TopEdge < 0 ||
        nw.LeftEdge + w->Width > w->WScreen->Width ||
        nw.TopEdge + w->Height > w->WScreen->Height) {

        goto failexit;
    }

    if (NULL == parentwin)
    {
        w->LeftEdge    = nw.LeftEdge;
        w->TopEdge     = nw.TopEdge;
    }
    else
    {
        w->LeftEdge    = nw.LeftEdge + parentwin->LeftEdge;
        w->TopEdge     = nw.TopEdge  + parentwin->TopEdge;
    }

    w->RelLeftEdge = nw.LeftEdge;
    w->RelTopEdge  = nw.TopEdge;

    w->MinWidth  = (nw.MinWidth  != 0) ? nw.MinWidth  : w->Width;
    w->MinHeight = (nw.MinHeight != 0) ? nw.MinHeight : w->Height;
    w->MaxWidth  = (nw.MaxWidth  != 0) ? nw.MaxWidth  : w->Width;
    w->MaxHeight = (nw.MaxHeight != 0) ? nw.MaxHeight : w->Height;

    //jDc: tested behavior of intuition68k
    if ((UWORD)w->MaxWidth < w->Width) w->MaxWidth = w->Width;
    if ((UWORD)w->MaxHeight < w->Height) w->MaxHeight = w->Height;

    /* check if maxwidth/height is not bigger than screen */
    if (w->MaxWidth > w->WScreen->Width) w->MaxWidth = w->WScreen->Width;
    if (w->MaxHeight > w->WScreen->Height) w->MaxHeight = w->WScreen->Height;

    if (zoombox)
    {
        ((struct IntWindow *)w)->ZipLeftEdge = zoombox->Left;
        ((struct IntWindow *)w)->ZipTopEdge  = zoombox->Top;
        ((struct IntWindow *)w)->ZipWidth    = zoombox->Width;
        ((struct IntWindow *)w)->ZipHeight   = zoombox->Height;
    }
    else
    {
        ((struct IntWindow *)w)->ZipLeftEdge = nw.LeftEdge;
        ((struct IntWindow *)w)->ZipTopEdge  = nw.TopEdge;
        ((struct IntWindow *)w)->ZipWidth    = (w->Width == w->MinWidth) ? w->MaxWidth : w->MinWidth;
        ((struct IntWindow *)w)->ZipHeight   = (w->Height == w->MinHeight) ? w->MaxHeight : w->MinHeight;
    }

    IW(w)->mousequeue = mousequeue;
    IW(w)->repeatqueue = repeatqueue;

    /* Amiga and checkmark images for menus */

    IW(w)->Checkmark = Checkmark ? Checkmark :
                                           ((struct IntScreen *)(w->WScreen))->DInfo.dri_CheckMark;

    IW(w)->AmigaKey  = AmigaKey  ? AmigaKey  :
                                           ((struct IntScreen *)(w->WScreen))->DInfo.dri_AmigaKey;
    IW(w)->SubMenuImage = ((struct IntScreen *)(w->WScreen))->DInfo.dri_Customize->submenu;

    /* child support */
    if (NULL != parentwin)
    {
        if (parentwin->firstchild)
            parentwin->firstchild->prevchild = w;

        w->nextchild = parentwin->firstchild;
        parentwin->firstchild = w;
        w->parent = parentwin;
    }

    /* Help stuff */

    if (!have_helpgroup && helpgroupwindow)
    {
        if (IW(helpgroupwindow)->helpflags & HELPF_ISHELPGROUP)
        {
            helpgroup = IW(helpgroupwindow)->helpgroup;
            have_helpgroup = TRUE;
        }
    }

    if (have_helpgroup)
    {
        IW(w)->helpflags |= HELPF_ISHELPGROUP;
        IW(w)->helpgroup = helpgroup;
    }

    w->Title = nw.Title;

    /* as mentioned in the OpenScreen() autodoc/SA_SysFont settings the default
       window font will be either screenfont (sysfont=0) or defaultfont (sysfont=1) */
    w->IFont = SafeReopenFont(IntuitionBase, (IS(w->WScreen)->SysFont) ? &GfxBase->DefaultFont : &IS(w->WScreen)->DInfo.dri_Font);

    if (w->IFont == NULL) goto failexit;

    /* jDc: intui68k waits before opening the window until
    ** Move/SizeWindow actions are over (does it mean it's executed on
    ** caller's context?).
    */

    if (!(FindTask(0) == ((struct IIHData *)GetPrivIBase(IntuitionBase)->InputHandler->is_Data)->InputDeviceTask))
    {
        ObtainSemaphore(&GetPrivIBase(IntuitionBase)->WindowLock);
    }

    IW(w)->OutlineShape = NULL;

    if ((shapeti != NULL) || (shapehookti != NULL)) IW(w)->CustomShape = TRUE;
    if (!(IW(w)->CustomShape) && (!(w->Flags & WFLG_BORDERLESS)) && !IS_GZZWINDOW(w))
    {
            shapehook = &IW(w)->DefaultWindowShapeHook;
    }

    msg.window = w;
    msg.bitmap = nw.BitMap;
    //msg.backfillhook = backfillhook == LAYERS_BACKFILL ? &IW(w)->custombackfill : backfillhook;
    msg.backfillhook = backfillhook;
    msg.shape = shape;
    msg.shapehook = shapehook;
    msg.parentlayer = parentl;
    msg.invisible = windowinvisible;
    msg.composited = composited;
    msg.alphasurface = alphasurface;
    msg.opaqueness = opaqueness;
    msg.inputregion = inputregion;
    msg.success = FALSE;

    /* int_openwindow() may activate and render the window before returning.
     * Install WA_ScreenTitle first so that activation never exposes the
     * screen's default title as an intermediate state. */
    if (screenTitle != NULL)
        w->ScreenTitle = (UBYTE *)screenTitle;

    DoSyncAction((APTR)int_openwindow, &msg.msg, IntuitionBase);

    if (!(FindTask(0) == ((struct IIHData *)GetPrivIBase(IntuitionBase)->InputHandler->is_Data)->InputDeviceTask))
    {
        ReleaseSemaphore(&GetPrivIBase(IntuitionBase)->WindowLock);
    }

    if (!msg.success) {

        goto failexit;
    }

    /* nlorentz: The driver has in some way or another allocated a rastport for us,
       which now is ready for us to use. */

    driver_init_done = TRUE;
    rp = w->RPort;

    /* The window RastPort always gets the font from GfxBase->DefaultFont, which
       is the system's default font. Usually topaz 8, but it can be changed with
       the Fonts prefs program to another fixed-sized font. */

    SetFont (rp, w->IFont);

    /* FIXME: Remove workaround! */
    /* lbischoff: The following 4 Setxxx lines are a workaround for the InitRastPort
       problem (Bug #75 in docs/BUGS). They ensure that at least a window's rastport
       is initialized correctly. Remove them if they are not needed any longer!
    */
    SetABPenDrMd (rp, rp->FgPen, rp->BgPen, rp->DrawMode);
    SetWriteMask (rp, rp->Mask);

    /* Send all GA_RelSpecial BOOPSI gadgets in the list the GM_LAYOUT msg */
    /*DoGMLayout(w->FirstGadget, w, NULL, -1, TRUE, IntuitionBase);

    if (NULL != w->FirstGadget)
        RefreshGadgets (w->FirstGadget, w, NULL);
    */

    if (nw.FirstGadget)
    {
            struct IntDrawInfo                   *dri = &((struct IntScreen *)(w->WScreen))->DInfo;
            struct wdpLayoutBorderGadgets  msg;

        msg.MethodID                     = WDM_LAYOUT_BORDERGADGETS;
    msg.wdp_UserBuffer  = ((struct IntWindow *)w)->DecorUserBuffer;;
    msg.wdp_TrueColor        = (((struct IntScreen *)nw.Screen)->DInfo.dri_Flags & DRIF_DIRECTCOLOR) ? TRUE : FALSE;
        msg.wdp_Window                     = w;
        msg.wdp_Gadgets             = nw.FirstGadget;
        msg.wdp_Flags           = WDF_LBG_INITIAL | WDF_LBG_MULTIPLE;
    msg.wdp_ExtraButtons = ((struct IntWindow *)w)->extrabuttons;
        msg.wdp_Dri             = (struct DrawInfo *)dri;

        DoMethodA(((struct IntScreen *)(nw.Screen))->WinDecorObj, (Msg)&msg);

        AddGList(w, nw.FirstGadget, -1, -1, NULL);
    }

    UpdateMouseCoords(w);

    if (do_setwindowpointer)
    {
        //jDc: main for () loop destroys original taglist pointer, we need to get
        //it once again here!
        tagList = (struct TagItem *)((struct ExtNewWindow *)newWindow)->Extension;
        SetWindowPointerA(w, (struct TagItem *)tagList);
    }

    if (GetPrivIBase(IntuitionBase)->pubScrGlobalMode & POPPUBSCREEN) {
        if ((nw.Type == PUBLICSCREEN) ||
            ((GetPrivIBase(IntuitionBase)->pubScrGlobalMode & SHANGHAI) && (nw.Type == WBENCHSCREEN))) {
            ScreenToFront(w->WScreen);
        }
    }

    goto exit;

failexit:

    if (w)
    {
        ModifyIDCMP (w, 0);

        /* nlorentz: Freeing the rasport is now intui_CloseWindow()'s task.

           if (rp)
           {
               FreeRastPort (rp);
           }
        */

        if (driver_init_done)
            intui_CloseWindow(w, IntuitionBase);

        if (w->IFont) CloseFont(w->IFont);

        if (((struct IntWindow *)w)->DecorUserBuffer)
        {
            FreeMem((APTR)((struct IntWindow *)w)->DecorUserBuffer, ((struct IntWindow *)w)->DecorUserBufferSize);
        }

        FreeMem (w, sizeof(struct IntWindow));

        w = NULL;
    }

    if (nw.Screen && (moreFlags & WMFLG_DO_UNLOCKPUBSCREEN))
    {
        UnlockPubScreen(NULL, nw.Screen);
    }

exit:


    return w;
    AROS_LIBFUNC_EXIT
} /* OpenWindow */

/**********************************************************************************/

static VOID int_openwindow(struct OpenWindowActionMsg *msg,
                           struct IntuitionBase *IntuitionBase)
{
    struct GfxBase * GfxBase = GetPrivIBase(IntuitionBase)->GfxBase;
    struct LayersBase * LayersBase = GetPrivIBase(IntuitionBase)->LayersBase;
    struct Window * w = msg->window;
    struct BitMap * SuperBitMap = msg->bitmap;
    struct Hook   * backfillhook = msg->backfillhook;
    struct Region * shape = msg->shape;
    struct Hook   * shapehook = msg->shapehook;
    struct Layer  * parent = msg->parentlayer;
    BOOL            invisible = msg->invisible;

    /* Create a layer for the window */
    LONG layerflags = 0;

    

    /* Just insert some default values, should be taken from
       w->WScreen->WBorxxxx */

    /* Set the layer's flags according to the flags of the
    ** window
    */

    /* refresh type */
    if (w->Flags & WFLG_SIMPLE_REFRESH)
    {
        layerflags |= LAYERSIMPLE;
    }
    else
    {
        if (w->Flags & WFLG_SUPER_BITMAP && SuperBitMap)
        {
            layerflags |= LAYERSUPER;
        }
        else
        {
            layerflags |= LAYERSMART;
        }
    }

    if (w->Flags & WFLG_BACKDROP)
    {
        layerflags |= LAYERBACKDROP;
    }

//  LockLayers(&w->WScreen->LayerInfo);

    /*
     * The tags for the layer of the window. The entries that do not apply
     * to every layer start out as TAG_IGNORE and are filled in below.
     */
    struct TagItem layertags[] =
    {
        {LA_BackfillHook, (IPTR)backfillhook},
        {SuperBitMap ? LA_SuperBitMap : TAG_IGNORE, (IPTR)SuperBitMap},
        {TAG_IGNORE  , 0           }, /* [2]: LA_ShapeHook */
        {TAG_IGNORE  , 0           }, /* [3]: LA_ShapeRegion */
        {LA_WindowPtr, (IPTR)w     },
        {LA_ChildOf  , (IPTR)parent}, /* These two are AROS-specific */
        {LA_Hidden   , invisible   },
        {TAG_IGNORE  , 0           }, /* [7..9]: LA_Composited, LA_Opacity, LA_AlphaSurface */
        {TAG_IGNORE  , 0           },
        {TAG_IGNORE  , 0           },
        {TAG_IGNORE  , 0           }, /* [10]: LA_InputShape */
        {TAG_DONE}
    };

    /* A GimmeZeroZero window??? */
    if (w->Flags & WFLG_GIMMEZEROZERO)
    {
        /*
          A GimmeZeroZero window is to be created:
            - the outer window will be a simple refresh layer
            - the inner window will be a layer according to the flags
          What is the size of the inner/outer window supposed to be???
          I just make it that the outer window has the size of what is requested
        */

        /* First create outer window. Its layer is never backfilled. */
        layertags[0].ti_Data = (IPTR)LAYERS_NOBACKFILL;

        struct Layer * L = CreateUpfrontLayerTagList(
                                   &w->WScreen->LayerInfo
                                   , w->WScreen->RastPort.BitMap
                                   , w->RelLeftEdge
                                   , w->RelTopEdge
                                   , w->RelLeftEdge + w->Width - 1
                                   , w->RelTopEdge  + w->Height - 1
                                   , LAYERSIMPLE | (layerflags & LAYERBACKDROP)
                                   , (struct TagItem *)&layertags);

        /* Could the layer be created. Nothing bad happened so far, so simply leave */
        if (NULL == L)
        {
            msg->success = FALSE;
//            UnlockLayers(&w->WScreen->LayerInfo);
            return;
        }

        /* install it as the BorderRPort */
        w->BorderRPort = L->rp;
        BLAYER(w) = L;

        /* Now comes the inner window */
        layertags[0].ti_Data = (IPTR)backfillhook;
        if (msg->composited)
        {
            layertags[7].ti_Tag  = LA_Composited;
            layertags[7].ti_Data = TRUE;
            layertags[8].ti_Tag  = LA_Opacity;
            layertags[8].ti_Data = msg->opaqueness;
            layertags[9].ti_Tag  = LA_AlphaSurface;
            layertags[9].ti_Data = msg->alphasurface;
        }
        if (shapehook)
        {
            layertags[2].ti_Tag  = LA_ShapeHook;
            layertags[2].ti_Data = (IPTR)shapehook;
        }
        if (shape)
        {
            layertags[3].ti_Tag  = LA_ShapeRegion;
            layertags[3].ti_Data = (IPTR)shape;
        }

        w->WLayer = CreateUpfrontLayerTagList(
                &w->WScreen->LayerInfo
                , w->WScreen->RastPort.BitMap
                , w->RelLeftEdge + w->BorderLeft
                , w->RelTopEdge  + w->BorderTop
                , w->RelLeftEdge + w->BorderLeft + w->GZZWidth - 1
                , w->RelTopEdge  + w->BorderTop + w->GZZHeight - 1
                , layerflags
                , (struct TagItem *)&layertags);

        /* could this layer be created? If not then delete the outer window and exit */
        if (NULL == w->WLayer)
        {
            DeleteLayer(0, L);
            msg->success = FALSE;
//            UnlockLayers(&w->WScreen->LayerInfo);
            return;
        }

        /* That should do it, I guess... */
    }
    else
    {
        if (msg->composited)
        {
            layertags[7].ti_Tag  = LA_Composited;
            layertags[7].ti_Data = TRUE;
            layertags[8].ti_Tag  = LA_Opacity;
            layertags[8].ti_Data = msg->opaqueness;
            layertags[9].ti_Tag  = LA_AlphaSurface;
            layertags[9].ti_Data = msg->alphasurface;
        }
        if (msg->inputregion)
        {
            layertags[10].ti_Tag  = LA_InputShape;
            layertags[10].ti_Data = (IPTR)msg->inputregion;
        }
        if (shapehook)
        {
            layertags[2].ti_Tag  = LA_ShapeHook;
            layertags[2].ti_Data = (IPTR)shapehook;
        }
        if (shape)
        {
            layertags[3].ti_Tag  = LA_ShapeRegion;
            layertags[3].ti_Data = (IPTR)shape;
        }

        w->WLayer = CreateUpfrontLayerTagList(      &w->WScreen->LayerInfo,
                                w->WScreen->RastPort.BitMap,
                                w->RelLeftEdge,
                                w->RelTopEdge,
                                w->RelLeftEdge + w->Width - 1,
                                w->RelTopEdge  + w->Height - 1,
                                layerflags,
                                (struct TagItem *)&layertags);

        /* Install the BorderRPort here! see GZZ window above  */
        if (NULL != w->WLayer)
        {
            /*
               I am installing a totally new RastPort here so window and frame can
               have different fonts etc.
            */
            w->BorderRPort = AllocMem(sizeof(struct RastPort), MEMF_ANY);

            if (w->BorderRPort)
            {
                InitRastPort(w->BorderRPort);
                w->BorderRPort->Layer  = w->WLayer;
                w->BorderRPort->BitMap = w->WLayer->rp->BitMap;
            }
            else
            {
                /* no memory for RastPort! Simply close the window */
                intui_CloseWindow(w, IntuitionBase);
                msg->success = FALSE;
//                UnlockLayers(&w->WScreen->LayerInfo);
                return;
            }
        }
    }

    /* common code for GZZ and regular windows */

    if (w->WLayer)
    {

        if ((layerflags & LAYERBACKDROP) && (w->WScreen->Flags & SHOWTITLE))
        {
            /* backdrop window was created over screen barlayer, but it must be
            under the screen barlayer if screen has flag SHOWTITLE set */

            AROS_ATOMIC_AND(w->WScreen->Flags, ~SHOWTITLE);

            ShowTitle(w->WScreen, TRUE);
        }

        /* Layer gets pointer to the window */
        WLAYER(w) = w->WLayer;

        CheckLayers(w->WScreen,IntuitionBase);

//        UnlockLayers(&w->WScreen->LayerInfo);

        w->WLayer->Window = (APTR)w;
        /* Window needs a rastport */
        w->RPort = w->WLayer->rp;

        /* installation of the correct BorderRPort already happened above !! */

        if (CreateWinSysGadgets(w, IntuitionBase))
        {
            LONG lock;

            lock = LockIBase (0);

            /* insert new window into parent/descendant list
            **
            ** before: parent win xyz
            **            |
            **            |
            **            |
            **        descendant win abc
            **
            ** after:  parent win xyz
            **              \
            **       \
            **         newwindow w
            **      /
            **         /
            **        /
            **   descendant win abc
            */

            {
            struct Window *parent, *descendant_of_parent;

            parent = IntuitionBase->ActiveWindow;
            if (!parent) parent = w->WScreen->FirstWindow;
            if (parent)
            {
                descendant_of_parent = parent->Descendant;
                parent->Descendant = w;
                if (descendant_of_parent) descendant_of_parent->Parent = w;
            }
            else
            {
                descendant_of_parent = NULL;
            }

            w->Descendant = descendant_of_parent;
            w->Parent = parent;
            }

            w->NextWindow = w->WScreen->FirstWindow;
            w->WScreen->FirstWindow = w;

            w->WindowPort = GetPrivIBase(IntuitionBase)->IntuiReplyPort;

            UnlockIBase (lock);

            AddResourceToList(w, RESOURCE_WINDOW, IntuitionBase);

            if ((w->Flags & WFLG_ACTIVATE) && !invisible)
            {
                ActivateWindow(w);
            }
            else
            {
                RefreshWindowFrame(w);
            }

            msg->success = TRUE;
            return;
        }

        CloseWindow(w);
        //int_closewindow(w, IntuitionBase);

    } /* if (layer created) */

//    UnlockLayers(&w->WScreen->LayerInfo);

    msg->success = FALSE;
    return;
}
