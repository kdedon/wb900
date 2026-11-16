/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.
    Copyright (C) 2001-2013, The MorphOS Development Team. All Rights Reserved.

    Open a new screen.
*/

/*
 * Things added by AROS, which needs to be kept when merging with newer MorphOS releases:
 *
 * 1. Explicit library bases
 * 3. AddResourceToList() call
 * 4. Decoration calls (see intuition_customize.h)
 * 5. Hardcoded initial color table is not used and removed.
 *    AmigaOS-compatible palette is used instead.
 * 6. Other placed marked by 'AROS:' in comments.
 * 7. Check #ifdef's. Some of them were rearranged or completely deleted.
 *    We reuse MorphOS skin code where appropriate.
 */

#include <exec/memory.h>
#include <utility/tagitem.h>
#include <intuition/screens.h>
#include <intuition/intuition.h>
#include <intuition/imageclass.h>
#include <intuition/gadgetclass.h>
#include <graphics/modeid.h>
#include <graphics/videocontrol.h>
#include <prefs/screenmode.h>
#include <graphics/rpattr.h>
#include <graphics/layers.h>
#include <graphics/gfxmacros.h>
#include <proto/input.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/layers.h>
#include <proto/utility.h>
#include <proto/intuition.h>

#include "intuition_intern.h"
#include "intuition_customize.h"
#include "inputhandler.h"
#include "inputhandler_support.h"
#include "inputhandler_actions.h"
#include "menus.h"

struct OpenScreenActionMsg
{
    struct IntuiActionMsg msg;
    struct IntScreen *Screen;
    struct NewScreen *NewScreen;
    struct List *List;
    BOOL Success;
};

VOID int_openscreen(struct OpenScreenActionMsg *msg,struct IntuitionBase *IntuitionBase);

/*
 * The pens of a screen of more than one bitplane in the 1.x look: as on
 * one bitplane, colour 1 draws the frames, title bars and text over
 * colour 0, and HIGHLIGHTTEXTPEN is colour 3.
 */
static CONST UWORD OldLookPens[NUMDRIPENS] = { 0, 1, 1, 1, 1, 1, 0, 0, 3, 0, 1, 1 };

/*
 * The Workbench screen's background: FILLPEN over BACKGROUNDPEN in a 50%
 * pattern of 2-pixel steps.  The pattern is anchored to the screen, so
 * repaired damage meets what is around it wherever windows move.
 */
static ULONG WBBackFillFunc(struct Hook *hook, struct RastPort *rp,
                            struct BackFillMessage *msg)
{
    static UWORD halftone[] = { 0x3333, 0xCCCC };
    struct IntScreen *screen = (struct IntScreen *)
        ((UBYTE *)hook - offsetof(struct IntScreen, BackFillHook));
    struct GfxBase *GfxBase = GetPrivIBase((struct IntuitionBase *)hook->h_Data)->GfxBase;
    struct RastPort fill;

    fill = *rp;
    SetABPenDrMd(&fill, screen->Pens[FILLPEN], screen->Pens[BACKGROUNDPEN], JAM2);
    SetAfPt(&fill, halftone, 1);
    RectFill(&fill, msg->Bounds.MinX, msg->Bounds.MinY,
             msg->Bounds.MaxX, msg->Bounds.MaxY);

    return 0;
}

/*
 * What the display database would say of a screen in RAM when there is
 * no display: the nominal size, square pixels in the ticks of a
 * high resolution interlaced mode.
 */
static void NoDisplayInfo(struct DimensionInfo *dims, struct DisplayInfo *disp,
                          struct IntuitionBase *IntuitionBase)
{
    if (!GetPrivIBase(IntuitionBase)->ScreenModePrefs)
        SetDisplayDefaults(IntuitionBase);
    dims->MaxDepth = 8;
    dims->MinRasterWidth = 16;
    dims->MinRasterHeight = 1;
    dims->MaxRasterWidth = GetPrivIBase(IntuitionBase)->ScreenModePrefs->smp_Width;
    dims->MaxRasterHeight = GetPrivIBase(IntuitionBase)->ScreenModePrefs->smp_Height;
    dims->Nominal.MinX = 0;
    dims->Nominal.MinY = 0;
    dims->Nominal.MaxX = dims->MaxRasterWidth - 1;
    dims->Nominal.MaxY = dims->MaxRasterHeight - 1;
    dims->MaxOScan = dims->VideoOScan = dims->TxtOScan = dims->StdOScan = dims->Nominal;
    disp->Resolution.x = 22;
    disp->Resolution.y = 22;
}

/*****************************************************************************

    NAME */
        AROS_LH1(struct Screen *, OpenScreen,

/*  SYNOPSIS */
        AROS_LHA(struct NewScreen *, newScreen, A0),

/*  LOCATION */
        struct IntuitionBase *, IntuitionBase, 33, Intuition)

/*  FUNCTION

    INPUTS

    RESULT

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct GfxBase        *GfxBase = GetPrivIBase(IntuitionBase)->GfxBase;
    struct LayersBase     *LayersBase = GetPrivIBase(IntuitionBase)->LayersBase;
    struct Library        *UtilityBase = GetPrivIBase(IntuitionBase)->UtilityBase;
    struct NewScreen       ns;
    struct TagItem        *tag, *tagList;
    struct IntScreen      *screen;
    int                    success;
    struct Hook           *layer_info_hook = NULL;
    struct ColorSpec      *colors = NULL;
    ULONG                 *errorPtr;   /* Store error at user specified location */
    UWORD                 *customdripens = NULL;
    ULONG                 *colors32 = NULL;
    WORD                   sysfont = -1;
    BOOL                   ok = TRUE, rp_inited = FALSE, li_inited = FALSE, sharepens = FALSE, likewb = FALSE;
    BOOL                   windowlock = FALSE;
    BOOL                   dowindowlock = TRUE;
    struct Rectangle      *dclip = NULL;
    LONG                   overscan = OSCAN_TEXT;
    DisplayInfoHandle      displayinfo = NULL;
    struct DisplayInfo     dispinfo;
    struct DimensionInfo   dimensions;
    BOOL                   nodisplay = FALSE;
    ULONG                  allocbitmapflags = BMF_DISPLAYABLE;
    char                  *skinname = 0;
    IPTR                   vctl = 0;
    BOOL                   showpointer = TRUE;
    BOOL                   adaptsize = FALSE;
    //ULONG                  lock;
    WORD                   numcolors = 0;
    UWORD                  spritebase;
    BOOL                   workbench = FALSE;
    ULONG                  requesteddepth = 1;
    BOOL                   draggable = TRUE;
    BOOL                   oldlook = FALSE;

    struct TagItem   modetags[] =
    {
        { BIDTAG_Depth        , 0UL   },
        { BIDTAG_DesiredWidth , 0UL   },
        { BIDTAG_DesiredHeight, 0UL   },
        { BIDTAG_NominalWidth , 0UL   },
        { BIDTAG_NominalHeight, 0UL   },
        { BIDTAG_DIPFMustHave , 0UL   },
        { TAG_DONE                    }
    };

    ULONG modeid = INVALID_ID;

#define COPY(x)     screen->Screen.x = ns.x
#define SetError(x) if (errorPtr != NULL) *errorPtr = x;

    ns = *newScreen;

    if (newScreen->Type & NS_EXTENDED)
    {
        tagList = ((struct ExtNewScreen *)newScreen)->Extension;
    } else {
        tagList = NULL;
    }

    screen = NewObjectA(IBase->screenclass,0,0);

    /* Do this really early to be able to report errors */
    errorPtr = (ULONG *)GetTagData((Tag)SA_ErrorCode, (IPTR)NULL, (struct TagItem *)tagList);

    if (screen == NULL)
    {
        SetError(OSERR_NOMEM);
        ok = FALSE;
    }

    /* Without a screen object there is nothing the tags could set up */
    if (!ok)
        tagList = NULL;

    /*
     * Evaluate the tags. The tag functions accept a NULL list: GetTagData()
     * returns the default and NextTagItem() returns NULL at once, so without
     * tags everything keeps the value taken from the NewScreen structure.
     */
    {
        char *pubname = NULL;

        modeid = GetTagData(SA_DisplayID,INVALID_ID, tagList);

        if (GetTagData(SA_LikeWorkbench, FALSE, tagList))
        {

            /*
             * AROS: Use oldstyle ScreenModePrefs structure.
             * CHECKME: Can we merge better ?
             */
            if (!GetPrivIBase(IntuitionBase)->ScreenModePrefs)
                SetDisplayDefaults(IntuitionBase);

            ns.Width = GetPrivIBase(IntuitionBase)->ScreenModePrefs->smp_Width;
            ns.Height = GetPrivIBase(IntuitionBase)->ScreenModePrefs->smp_Height;
            ns.Depth = GetPrivIBase(IntuitionBase)->ScreenModePrefs->smp_Depth;
            modeid = GetPrivIBase(IntuitionBase)->ScreenModePrefs->smp_DisplayID;

            if (GetPrivIBase(IntuitionBase)->ScreenModePrefs->smp_Control & SMF_AUTOSCROLL)
            {
                /* need to mark autoscroll */
                ns.Type |= AUTOSCROLL;
            }

            likewb = TRUE;
            sharepens = TRUE; /* not sure */
        }

        if ((pubname = (char*)GetTagData(SA_PubName, 0, tagList)))
        {
            /* Name of this public screen. */
            struct PubScreenNode *oldpsn;
            struct List *list;

            list = LockPubScreenList();

            if ((strcmp(pubname, "Workbench") == 0) || workbench)
            {
                if (IBase->WorkBench)
                {
                    ok = FALSE;
                }

                workbench = TRUE;
            }
            else
            {
                /* jDc: LockPubScreen is not safe here as it does not lock
                screens in private state so we could end up with two
                screens with the same name */
                oldpsn = (struct PubScreenNode *)FindName(list,(UBYTE*)pubname);

                if (oldpsn)
                {
                    SetError(OSERR_PUBNOTUNIQUE);
                    ok = FALSE;
                }
            }

            UnlockPubScreenList();

            if (ok)
            {
                screen->pubScrNode = AllocMem(sizeof(struct PubScreenNode), MEMF_CLEAR);

                if (screen->pubScrNode == NULL)
                {
                    SetError(OSERR_NOMEM);
                    ok = FALSE;
                }

                if (ok && (screen->pubScrNode->psn_Node.ln_Name = AllocVec(MAXPUBSCREENNAME + 1,MEMF_ANY)))
                {
                    UBYTE sigbit;

                    if ((ns.Type & SCREENTYPE) == CUSTOMSCREEN)
                    {
                        ns.Type &= ~SCREENTYPE;
                        ns.Type |= PUBLICSCREEN;
                    }

                    /* Always open public screens in private mode. */
                    screen->pubScrNode->psn_Flags |= PSNF_PRIVATE;
                    strcpy(screen->pubScrNode->psn_Node.ln_Name, pubname);

                    /* Task that should be signalled when the public screen loses
                       its last visitor window. */

                    screen->pubScrNode->psn_SigTask    = (struct Task *)GetTagData(SA_PubTask, 0, tagList);

                    /* Signal bit number to use when signalling public screen
                       signal task. */

                    sigbit = GetTagData(SA_PubSig,(ULONG)-1,tagList);

                    if (sigbit != (UBYTE)-1)
                    {
                        if (!screen->pubScrNode->psn_SigTask) screen->pubScrNode->psn_SigTask = FindTask(NULL);
                        screen->pubScrNode->psn_SigBit = sigbit;
                    }
                    else
                    {
                        /* do not signal with -1 ! */
                        screen->pubScrNode->psn_SigTask = NULL;
                    }
                }
                else
                {
                    SetError(OSERR_NOMEM);
                    FreeMem(screen->pubScrNode, sizeof(struct PubScreenNode));
                    screen->pubScrNode = NULL;
                    ok = FALSE;
                }
            }

        }

        while((tag = NextTagItem ((struct TagItem **)&tagList)))
        {

            switch(tag->ti_Tag)
            {
            case SA_Left:

                ns.LeftEdge  = tag->ti_Data;
                break;
            case SA_Top:

                ns.TopEdge   = tag->ti_Data;
                break;
            case SA_Width:

                if ((tag->ti_Data != ~0) && (tag->ti_Data > 32767))
                {
                    ok = FALSE;
                }
                ns.Width     = tag->ti_Data;
                break;
            case SA_Height:

                if ((tag->ti_Data != ~0) && (tag->ti_Data > 32767))
                {
                    ok = FALSE;
                }
                ns.Height    = tag->ti_Data;
                break;
            case SA_Depth:

                ns.Depth     = tag->ti_Data;
                break;
            case SA_DetailPen:

                ns.DetailPen = tag->ti_Data;
                break;
            case SA_BlockPen:

                ns.BlockPen  = tag->ti_Data;
                break;
            case SA_Type:

                ns.Type    &= ~SCREENTYPE;
                ns.Type |= tag->ti_Data;
                break;

            case SA_Title:

                ns.DefaultTitle = (UBYTE *)tag->ti_Data;
                break;

            case SA_Font:

                ns.Font = (struct TextAttr *)tag->ti_Data;
                break;

            case SA_Colors32:

                colors32 = (ULONG *)tag->ti_Data;
                break;

            case SA_Colors:

                colors = (struct ColorSpec *)tag->ti_Data;
                break;

            case SA_SysFont:

                sysfont = (WORD)tag->ti_Data;
                break;

            case SA_BitMap:

                if(tag->ti_Data)
                {
                    ns.Type |= CUSTOMBITMAP;
                    ns.CustomBitMap = (struct BitMap *)tag->ti_Data;
                }
                else
                {

                }
                break;

            case SA_BackFill:

                layer_info_hook = (struct Hook *)tag->ti_Data;
                break;

            case SA_Quiet:

                if (tag->ti_Data)
                {
                    ns.Type |= SCREENQUIET;
                }
                else
                {
                    ns.Type &= ~SCREENQUIET;
                }
                break;

            case SA_ShowTitle:

                if (tag->ti_Data)
                {
                    ns.Type |= SHOWTITLE;
                }
                else
                {
                    ns.Type &= ~SHOWTITLE;
                }
                break;

            case SA_Pens:

                customdripens = (UWORD *)tag->ti_Data;
                break;

            case SA_SharePens:

                sharepens = tag->ti_Data ? TRUE : FALSE;
                if (tag->ti_Data)
                {
                    ns.Type |= PENSHARED;
                } else {
                    ns.Type &= ~PENSHARED;
                }
                break;

            case SA_Interleaved:

                if (tag->ti_Data)
                {
                    allocbitmapflags |= BMF_INTERLEAVED;
                }
                else
                {
                    allocbitmapflags &= ~BMF_INTERLEAVED;
                }
                break;

            case SA_Behind:

                if (tag->ti_Data)
                {
                    ns.Type |= SCREENBEHIND;
                }
                else
                {
                    ns.Type &= ~SCREENBEHIND;
                }
                break;

            case SA_DClip:

                dclip = (struct Rectangle *)tag->ti_Data;
                break;

            case SA_Overscan:

                overscan = tag->ti_Data;
                break;

            case SA_DisplayID:
            case SA_LikeWorkbench:
            case SA_ErrorCode:
            case SA_PubName:
            case SA_PubSig:
            case SA_PubTask:
                /*
                 * handled elsewhere
                 */
                break;

            case SA_AutoScroll:

                if (tag->ti_Data)
                {
                    ns.Type |= AUTOSCROLL;
                }
                else
                {
                    ns.Type &= ~AUTOSCROLL;
                }
                break;

            case SA_FullPalette:

                break;
            case SA_ColorMapEntries:

                numcolors = tag->ti_Data; /* AROS: Added support for this tag */
                break;
            case SA_Parent:

                break;
            case SA_Draggable:

                draggable = tag->ti_Data; /* AROS: Added support for this tag */
                break;
            case SA_Exclusive:

                break;
            case SA_VideoControl:

                vctl = tag->ti_Data; /* AROS: Added support for this tag */
                break;
            case SA_FrontChild:

                break;
            case SA_BackChild:

                break;
            case SA_MinimizeISG:

                break;

            default:
                /*
                 * The tags numbered from SA_Dummy + 100 up lie apart from the
                 * ones above. They have a switch of their own, so that each of
                 * the two covers a dense range of numbers.
                 */
                switch(tag->ti_Tag)
                {
                case SA_ShowPointer:
                    showpointer = tag->ti_Data ? TRUE : FALSE;
                    break;

                case SA_AdaptSize:
                    adaptsize = tag->ti_Data ? TRUE : FALSE;
                    break;

                /* TODO: Missing SA_ Tags */
                default:

                    break;

                } /* switch (tag->ti_Tag), SA_Dummy + 100 up */
                break;

            } /* switch (tag->ti_Tag) */

        } /* while ((tag = NextTagItem (&tagList))) */

    }

    requesteddepth = ns.Depth;

    /* Fail if bogus size is requested - Piru/jDc */
    if ((ns.Width < -1) || (ns.Height < -1))
    {

        ok = FALSE;
    }

    if (INVALID_ID == modeid)
    {
        modetags[0].ti_Data = ns.Depth;
        modetags[1].ti_Data = ns.Width;
        modetags[2].ti_Data = ns.Height;
        modetags[3].ti_Data = ns.Width;
        modetags[4].ti_Data = ns.Height;

        /*
         * AROS: We don't have FindBestWidthAndHeight().
         * TODO: Can we merge better here ?
         */
        if (ns.Width == STDSCREENWIDTH || ns.Width == 0 || adaptsize) {modetags[1].ti_Tag = TAG_IGNORE; modetags[3].ti_Tag = TAG_IGNORE;}
        if (ns.Height == STDSCREENHEIGHT || ns.Height == 0 || adaptsize) {modetags[2].ti_Tag = TAG_IGNORE; modetags[4].ti_Tag = TAG_IGNORE;}

        /* AROS: Support old-style HAM or EHB request */
        if (newScreen->ViewModes & (HAM | EXTRA_HALFBRITE))
        {
            if (newScreen->ViewModes & HAM)         modetags[5].ti_Data |= DIPF_IS_HAM;
            if (newScreen->ViewModes & EXTRA_HALFBRITE) modetags[5].ti_Data |= DIPF_IS_EXTRAHALFBRITE;
        }
        else
        {
            modetags[5].ti_Tag = TAG_IGNORE;
        }

        modeid = BestModeIDA(modetags);

        if (INVALID_ID == modeid && NextDisplayInfo(INVALID_ID) == INVALID_ID)
        {
            /* No display at all: a screen in RAM */
            nodisplay = TRUE;
        }
        else if (INVALID_ID == modeid)
        {
            // wtf? no monitors??
            SetError(OSERR_UNKNOWNMODE); /* AROS: Added error core setting, fixed MorphOS bug */
            ok = FALSE;
        }
    }

    InitRastPort(&screen->Screen.RastPort);
    rp_inited = TRUE;
    success = FALSE;

    if (ok && nodisplay)
        NoDisplayInfo(&dimensions, &dispinfo, IntuitionBase);

    if (ok && (nodisplay || ((displayinfo = FindDisplayInfo(modeid)) != NULL &&
        GetDisplayInfoData(displayinfo, (APTR)&dimensions, sizeof(dimensions), DTAG_DIMS, modeid)
        && GetDisplayInfoData(displayinfo, (APTR)&dispinfo, sizeof(dispinfo), DTAG_DISP, modeid)))
       )
    {
        success = TRUE;

        if (draggable) screen->SpecialFlags |= SF_Draggable;
        if (dclip == NULL)
        {
            switch (overscan)
            {
            case OSCAN_STANDARD:
                dclip = &dimensions.StdOScan;
                break;

            case OSCAN_MAX:
                dclip = &dimensions.MaxOScan;
                break;

            case OSCAN_VIDEO:
                dclip = &dimensions.VideoOScan;
                break;

            default:
                dclip = &dimensions.TxtOScan;
                break;
            }
        }

        if (ns.Width == STDSCREENWIDTH || ns.Width == 0 || adaptsize)
            ns.Width = dclip->MaxX - dclip->MinX + 1;
        /* AROS: Added raster size limit support */
        else if (ns.Width < dimensions.MinRasterWidth)
            ns.Width = dimensions.MinRasterWidth;
        else if (ns.Width > dimensions.MaxRasterWidth)
            ns.Width = dimensions.MaxRasterWidth;

        if (ns.Height == STDSCREENHEIGHT || ns.Height == 0 || adaptsize)
            ns.Height = dclip->MaxY - dclip->MinY + 1;
        /* AROS: Added raster size limit support */
        else if (ns.Height < dimensions.MinRasterHeight)
            ns.Height = dimensions.MinRasterHeight;
        else if (ns.Height > dimensions.MaxRasterHeight)
            ns.Height = dimensions.MaxRasterHeight;

        if (ns.Type & CUSTOMBITMAP)
        {
            screen->Screen.RastPort.BitMap = ns.CustomBitMap;
            ns.Depth = GetBitMapAttr(ns.CustomBitMap, BMA_DEPTH);
        } else {
            screen->Screen.RastPort.BitMap = NULL;
        }

        if(ok && (screen->Screen.RastPort.BitMap == NULL))
        {
            /*
             * BitMap needs ModeID. The display's own bitmap when it
             * is free, else one in RAM of its mode. Not cleared: the
             * screen's background is painted over all of it below.
             */
            struct TagItem bmtags[2];

            bmtags[0].ti_Tag  = BMATags_DisplayID;
            bmtags[0].ti_Data = modeid;
            bmtags[1].ti_Tag  = TAG_DONE;
            screen->Screen.RastPort.BitMap = AllocBitMap(ns.Width, ns.Height, ns.Depth,
                                                         allocbitmapflags | BMF_CHECKVALUE,
                                                         (struct BitMap *)bmtags);
            screen->AllocatedBitMap = screen->Screen.RastPort.BitMap;

            if (screen->Screen.RastPort.BitMap)
                UpdateScreenBitMap(&screen->Screen, IntuitionBase);
        }

    }

    /* Init screen's viewport */
    InitVPort(&screen->Screen.ViewPort);

    /* Allocate a RasInfo struct in which we have  a pointer
       to the struct BitMap, into which the driver can
       store its stuff.
    */
    screen->Screen.ViewPort.RasInfo = AllocMem(sizeof(struct RasInfo), MEMF_ANY | MEMF_CLEAR);

    if (!success ||
        (screen->Screen.RastPort.BitMap == NULL) ||
        (screen->Screen.ViewPort.RasInfo == NULL))
    {
        ok = FALSE;
    } else {
        /* Store pointer to BitMap, so we can get hold of it
           from withing LoadRGBxx() functions
        */
        screen->Screen.ViewPort.RasInfo->BitMap = screen->Screen.RastPort.BitMap;
    }

    if (ok)
    {
        /* Read depth from the BitMap to avoid AttachPalExtra/ObtainPen getting
         * confused if cgx decided to allocate a higher depth BitMap than what
         * was asked.
         */
        ns.Depth = GetBitMapAttr(screen->Screen.RastPort.BitMap,BMA_DEPTH);

        if (!numcolors) /* AROS: Added support for SA_ColorMapEntries */
        {
            numcolors = (ns.Depth <= 8) ? (1L << ns.Depth) : 256;
        }

        /* Get a color map structure. Sufficient colors?? */

        if ((screen->Screen.ViewPort.ColorMap = GetColorMap(numcolors < 32 ? 32 : numcolors)) != NULL)
        {
            if (0 == AttachPalExtra(screen->Screen.ViewPort.ColorMap,
                        &screen->Screen.ViewPort))
            {
            }
            else
            {
                ok = FALSE;
            }
        }
        else
        {
            ok = FALSE;
        }

    }

    screen->ModeID = modeid;

    if (ok)
    {
        struct ViewPortExtra *vpe = (struct ViewPortExtra *)GfxNew(VIEWPORT_EXTRA_TYPE);

        ok = FALSE;

        if (vpe)
        {
            struct TagItem tags[6];

            memcpy(&vpe->DisplayClip, dclip,sizeof(struct Rectangle));
            /*
             * AROS: Set ViewPort offset and get size from dclip.
             * CHECKME: MorphOS source code always sets ViewPort's DWidth and DHeight to
             * nominal size, while ViewPortExtra's DisplayClip is still set to OverScan
             * rectangle. Whose approach is correct ?
             */
            screen->Screen.ViewPort.DxOffset = ns.LeftEdge;
            screen->Screen.ViewPort.DyOffset = ns.TopEdge;
            screen->Screen.ViewPort.DWidth   = dclip->MaxX - dclip->MinX + 1;
            screen->Screen.ViewPort.DHeight  = dclip->MaxY - dclip->MinY + 1;
            tags[0].ti_Tag  = VTAG_ATTACH_CM_SET;
            tags[0].ti_Data = (IPTR)&screen->Screen.ViewPort;
            tags[1].ti_Tag  = VTAG_VIEWPORTEXTRA_SET;
            tags[1].ti_Data = (IPTR)vpe;
            tags[2].ti_Tag  = VTAG_NORMAL_DISP_SET;
            tags[2].ti_Data = (IPTR)NULL;
            tags[3].ti_Tag  = VTAG_VPMODEID_SET;
            tags[3].ti_Data = modeid;
            tags[4].ti_Tag  = VTAG_SPEVEN_BASE_SET; /* AROS: Added sprite base and user-supplied tags */
            tags[5].ti_Tag  = VTAG_NEXTBUF_CM; /* if vctl is 0, this will terminate the list */
            tags[5].ti_Data = vctl;

            /*
             * graphics.library's software sprite takes pens 17-19 at every
             * depth: the ColorMap has at least 32 colours, and the pens
             * beyond the screen's depth map through the display's pixels.
             */
            spritebase = 16;

            tags[4].ti_Data = ((spritebase & 0x0F) << 8 ) | (spritebase >> 4);

            if (VideoControl(screen->Screen.ViewPort.ColorMap, tags) == 0)
                ok = TRUE;
        }
    }

    if (ok)
    {
        struct Color32 *p;
        int k;
        UWORD *q;

        p = IBase->Colors;
        for (k = 0; k < 4 && k < numcolors; ++k)
        {

            SetRGB32(&screen->Screen.ViewPort, k, p[k].red, p[k].green, p[k].blue);
        }

        if (ns.Depth >= 3)
        {
            /*
             * AROS: Use screen depth instead of 'numcolors' in order to calculate
             * numbers of last 4 colors of the screen.
             * This is necessary because we can have 8- or 16- color screen whose
             * ColorMap will still have 32 colors for AmigaOS(tm) compatibility
             * reasons.
             */
            ULONG lastcol = ((ns.Depth > 8) ? 256 : (1 << ns.Depth)) - 4;

            for (k = 0; k < 4; ++k)
            {

                if (k + lastcol < numcolors && !sharepens)
                {
                    ObtainPen(screen->Screen.ViewPort.ColorMap,
                              k + lastcol,
                              p[k+4].red,
                              p[k+4].green,
                              p[k+4].blue,
                              PEN_EXCLUSIVE);
                }
                else
                {
                    SetRGB32(&screen->Screen.ViewPort, k + lastcol, p[k+4].red, p[k+4].green, p[k+4].blue);
                }
            }
        }

        /*
         * AROS: AmigaOS(tm) 3.1-compatible mouse colors handling
         * 1. Allocate pens for the mouse pointer only on LUT screens.
         *    On hi- and truecolor screens sprite colors come from colormap attached
         *    to the sprite BitMap itself. See pointerclass::New() for details.
         * 2. Use 32-bit pointer colors instead of KS1.3 ActivePreferences->color17
         */
        if (ns.Depth < 9)
        {

            for (k = 1; k < 4; ++k, ++q)
            {

                if (k + spritebase < numcolors)
                {
                    ObtainPen(screen->Screen.ViewPort.ColorMap,
                              k + spritebase,
                              p[k+7].red, p[k+7].green, p[k+7].blue,
                              PEN_EXCLUSIVE);
                }
                else
                {
                    /* Can't be allocated, but can still be set. */
                    SetRGB32(&screen->Screen.ViewPort,
                             k + spritebase,
                             p[k+7].red, p[k+7].green, p[k+7].blue);
                }
            }
        }

        if (colors)  /* if SA_Colors tag exists */
        {

            for(; colors->ColorIndex != (WORD)~0; colors++)
            {

                SetRGB4(&screen->Screen.ViewPort,
                    colors->ColorIndex,
                    colors->Red,
                    colors->Green,
                    colors->Blue);
            }
        }

        if (colors32)  /* if SA_Colors32 tag exists */
        {

            LoadRGB32(&screen->Screen.ViewPort, (const ULONG *)colors32);
        }

        COPY(LeftEdge);
        COPY(TopEdge);
        COPY(Width);
        COPY(Height);
        COPY(DetailPen);
        COPY(BlockPen);
        COPY(Font);
        COPY(DefaultTitle);

        //intui68k filters this
        screen->Screen.Flags = (ns.Type & ~NS_EXTENDED);

        /* Temporary hack */
        if (ns.Width >= 500 || ns.Height >= 300)
            screen->Screen.Flags |= SCREENHIRES;

        /*
           Copy the data from the rastport's BitMap
           to the screen's BitMap structure
        */
        UpdateScreenBitMap(&screen->Screen, IntuitionBase);

        screen->Screen.WBorTop    = 6;  /* Amiga default is 2 */
        screen->Screen.WBorLeft   = 4;
        screen->Screen.WBorRight  = 4;
        screen->Screen.WBorBottom = 4;  /* Amiga default is 2 */

        screen->Screen.Title = ns.DefaultTitle;

        InitLayers(&screen->Screen.LayerInfo);
        li_inited = TRUE;

        /* With OLD_LOOK a screen without SA_Pens has the 1.x look */
        oldlook = !customdripens &&
                  (IBase->ActivePreferences.EnableCLI & OLD_LOOK) != 0;

        if (layer_info_hook == LAYERS_BACKFILL && (ns.Type & SCREENTYPE) == WBENCHSCREEN && !oldlook)
        {
            screen->BackFillHook.h_Entry = (HOOKFUNC)WBBackFillFunc;
            screen->BackFillHook.h_Data = IntuitionBase;
            layer_info_hook = &screen->BackFillHook;
        }

        if (NULL != layer_info_hook)
        {

            InstallLayerInfoHook(&screen->Screen.LayerInfo, layer_info_hook);
        }

        /* AROS: We have already obtained resolution data from DisplayInfo */
        screen->DInfo.dri_Version = DRI_VERSION;
        screen->DInfo.dri_NumPens = NUMDRIPENS;
        screen->DInfo.dri_Pens = screen->Pens;
        /* dri_Depth is 8 on hi/true color screens like in AmigaOS with picasso96/cybergraphx */
        screen->DInfo.dri_Depth = (ns.Depth <= 8) ? ns.Depth : 8;
        screen->DInfo.dri_Resolution.X = dispinfo.Resolution.x;
        screen->DInfo.dri_Resolution.Y = dispinfo.Resolution.y;
        screen->DInfo.dri_Flags = 0;

        /* SA_SysFont overrides SA_Font! */

        if (sysfont == 0)
        {
            /* Is handled below */

        } else if (sysfont == 1) {

            screen->DInfo.dri_Font = SafeReopenFont(IntuitionBase, &IBase->ScreenFont);

            if (screen->DInfo.dri_Font)
            {
                screen->SysFont = TRUE;
            }

        } else if (ns.Font) {
            screen->DInfo.dri_Font = OpenFont(ns.Font);

        } else if (likewb) {
            screen->DInfo.dri_Font = SafeReopenFont(IntuitionBase, &IBase->ScreenFont);

            if (screen->DInfo.dri_Font)
            {
                screen->SysFont = TRUE;
            }

        }

        if (!screen->DInfo.dri_Font)
        {
            /* GfxBase->DefaultFont is *not* always topaz 8. It
               can be set with the Font prefs program!! */

            screen->DInfo.dri_Font = SafeReopenFont(IntuitionBase, &GfxBase->DefaultFont);
        }

        if (!screen->DInfo.dri_Font) ok = FALSE;

    } /* if (ok) */

    if (ok)
    {
        ULONG *dripens = NULL;

        if (oldlook && ns.Depth > 1)
            dripens = (ULONG*)OldLookPens;
        else switch (ns.Depth)
        {
            case 1:
                dripens = (ULONG*)&IBase->DriPens2;
                break;

            case 2:
                dripens = (ULONG*)&IBase->DriPens4;
                break;

            default:
                if (colors || colors32)
                {
                    /* jDc: apps are used to use non-std colors for pen # >=4
                    dripens4 will be a good fallback for this */
                    dripens = (ULONG*)&IBase->DriPens4;
                } else {
                    dripens = (ULONG*)&IBase->DriPens8;
                }
                break;
        }

        /* set default values for pens */

        CopyMem(dripens,screen->Pens,sizeof(screen->Pens));

        if (ns.Depth > 1 && !oldlook)
        {

            screen->DInfo.dri_Flags |= DRIF_NEWLOOK;
        }

        if (customdripens)
        {
            WORD i;

            screen->Pens[DETAILPEN] = screen->Screen.DetailPen;
            screen->Pens[BLOCKPEN] = screen->Screen.BlockPen;

            for(i = 0; (i < NUMDRIPENS) && (customdripens[i] != (UWORD)~0) && (screen->Pens[i] != (UWORD)~0); i++)
            {

                screen->Pens[i] = customdripens[i];
            }
        }

        /*
         * Let's do some broken software validation of the pens
         * so we may not run into a black desktop.
         */

        if (screen->Screen.DetailPen == screen->Screen.BlockPen)
        {

            screen->Screen.DetailPen = 0;
            screen->Screen.BlockPen = 1;
        } else if (screen->Screen.BlockPen == 0)
        {

            screen->Screen.BlockPen = screen->Screen.DetailPen;
            screen->Screen.DetailPen = 0;
        }

        screen->Pens[DETAILPEN] = screen->Screen.DetailPen;
        screen->Pens[BLOCKPEN] = screen->Screen.BlockPen;

        /* Allocate shared/exclusive colors */

        {
            BYTE color_alloced[256];
            UWORD mask;
            WORD i;

            /*for(i = 0; i < 256; i++)
            {
                color_alloced[i] = FALSE;
            }*/
            memclr(&color_alloced,256);

            /* Mouse pointer colors */

            color_alloced[17] = TRUE;
            color_alloced[18] = TRUE;
            color_alloced[19] = TRUE;

            /* Clamp the pen numbers. This makes sure that values such as -4 or -3
               won't result in random memory trashing, but rather index colours from
               the end of the shareable palette. - Piru */
            mask = screen->Screen.ViewPort.ColorMap->PalExtra->pe_SharableColors & 0xff;

            /* The Pens in the DrawInfo must be allocated as shared */

            for(i = 0; (i < NUMDRIPENS) && (screen->Pens[i] != (UWORD)~0); i++)
            {
                int pen = screen->Pens[i] & mask;
                if (!color_alloced[pen])
                {

                    ObtainPen(screen->Screen.ViewPort.ColorMap,
                          pen,
                          0,
                          0,
                          0,
                          PENF_NO_SETCOLOR);
                    color_alloced[pen] = TRUE;
                }
            }

            /* If SA_SharePens is FALSE then allocate the rest of the colors
               in the colormap as exclusive */

            if (!sharepens)
            {
                ULONG shnumcolors = (requesteddepth <= 8) ? (1L << requesteddepth) : 256;

                /* jDc: hack, colors above requested screen depth are not locked! */

                for(i = 0; i < shnumcolors; i++)
                {
                    if (!color_alloced[i])
                    {

                        ObtainPen(screen->Screen.ViewPort.ColorMap,
                              i,
                              0,
                              0,
                              0,
                              PENF_EXCLUSIVE | PENF_NO_SETCOLOR);
                    }
                }

            } /* if (!sharepens) */

        }
    }

    if (ok)
    {
        screen->DInfo.dri_Screen = &screen->Screen; //useful sometimes ;)

        screen->realdepth = GetBitMapAttr( screen->Screen.RastPort.BitMap, BMA_DEPTH );

        if (screen->realdepth > 8)
        {
            screen->DInfo.dri_Flags |= DRIF_DIRECTCOLOR;
        } else {
            screen->DInfo.dri_Flags &= ~DRIF_DIRECTCOLOR;
        }

        if (ok)
        {

            if ((screen->DInfo.dri_Customize = AllocMem(sizeof (struct IntuitionCustomize),MEMF_PUBLIC|MEMF_CLEAR)))
            {
                /* AROS: Use less error-prone skinname handling */
                ok = int_LoadDecorator(skinname, screen, IntuitionBase);
            }
            else
            {
                ok = FALSE;
            }
        }
    }

    if (ok)
    {
        struct TagItem sysi_tags[] =
        {
            {SYSIA_Which,      MENUCHECK          },
            {SYSIA_DrawInfo,   (IPTR)&screen->DInfo   },
            {TAG_DONE                 }
        };

        screen->DInfo.dri_CheckMark = NewObjectA(NULL, "sysiclass", sysi_tags);

        sysi_tags[0].ti_Data = AMIGAKEY;

        screen->DInfo.dri_AmigaKey  = NewObjectA(NULL, "sysiclass", sysi_tags);

        /* AROS: We also use submenu image */
        sysi_tags[0].ti_Data = SUBMENUIMAGE;
        screen->DInfo.dri_Customize->submenu  = NewObjectA(NULL, "sysiclass", sysi_tags);
        if (!screen->DInfo.dri_Customize->submenu) ok =FALSE;

        if (!screen->DInfo.dri_CheckMark || !screen->DInfo.dri_AmigaKey) ok = FALSE;
    }

    if (ok)
    {
        SetFont(&screen->Screen.RastPort, screen->DInfo.dri_Font);

        AskFont(&screen->Screen.RastPort, (struct TextAttr *) &screen->textattr);

        screen->Screen.Font = (struct TextAttr *) &screen->textattr;

        screen->Screen.BarVBorder  = 1;
        screen->Screen.BarHBorder  = 5;
        screen->Screen.MenuVBorder = 2;
        screen->Screen.MenuHBorder = 4;

        screen->Screen.BarHeight   = screen->DInfo.dri_Font->tf_YSize + screen->Screen.WBorTop-2 +
                         screen->Screen.BarVBorder * 2; /* real layer will be 1 pixel higher! */

        /*
         * AROS: Get size information from decorator.
         * This has to be done before we create gadgets because here we may adjust BarHeight.
         */
        ok = int_InitDecorator(&screen->Screen);
        if (ok)
        {
#define SDEPTH_HEIGHT (screen->Screen.BarHeight + 1)
#ifdef IA_Screen
#undef IA_Screen
#endif
#define IA_Screen    (IA_Dummy + 0x1f) /* OS v44 includes!*/

            struct TagItem sdepth_tags[] =
            {
                {GA_Image,     0            },
                {GA_Top,       0            },
                {GA_Height,    SDEPTH_HEIGHT},
                {GA_SysGadget, TRUE         },
                {GA_SysGType,  GTYP_SDEPTH  },
                {GA_RelVerify, TRUE         },
                {TAG_DONE,     0UL          }
            };

            struct TagItem image_tags[] =
            {
                {IA_Left         , 0                   },
                //{IA_Width        , SDEPTH_WIDTH + 1    },
                {IA_Height       , SDEPTH_HEIGHT       },
                {SYSIA_Which     , SDEPTHIMAGE         },
                {SYSIA_DrawInfo  , (IPTR)&screen->DInfo},
                {SYSIA_Size      , screen->Screen.Flags & SCREENHIRES ?
                                   SYSISIZE_MEDRES : SYSISIZE_LOWRES},
                {TAG_DONE                              }
            };

            struct Object *im = 0;

            if (!(screen->Screen.Flags & SCREENQUIET))
            {
                im = NewObjectA(NULL, SYSICLASS, image_tags);
            }

            sdepth_tags[0].ti_Data = (IPTR)im;

            screen->depthgadget = NewObjectA(IBase->windowsysiclass, NULL, sdepth_tags );

            screen->Screen.FirstGadget = (struct Gadget *)screen->depthgadget;
            if (screen->Screen.FirstGadget)
            {
                struct TagItem gadtags[] =
                {
                        {GA_RelRight, 0},
                        {TAG_DONE  }
                };
                IPTR width; /* AROS: Changed from int to IPTR, 64-bit fix */

                GetAttr(GA_Width, screen->depthgadget, &width);

                gadtags[0].ti_Data = -width + 1;
                SetAttrsA(screen->depthgadget, gadtags);
                screen->Screen.FirstGadget->GadgetType |= GTYP_SCRGADGET;
            } else {
                if (im) DisposeObject(im);
            }

            /* let's wait for user to finish window drag/size actions to avoid
            deadlocks and not break user's input */
            if (dowindowlock && (!(FindTask(NULL) == ((struct IIHData *)IBase->InputHandler->is_Data)->InputDeviceTask)))
            {
                LOCKWINDOW;
                windowlock = TRUE;
                screen->WindowLock = TRUE;
            }

            int_CalcSkinInfo(&screen->Screen,IntuitionBase);

            /* An installed backfill hook owns the background, including a
             * no-op hook used to retain a preloaded custom bitmap. SetRast
             * bypasses that hook and would erase the caller's picture. */
            if (layer_info_hook == LAYERS_BACKFILL)
                SetRast(&screen->Screen.RastPort, screen->Pens[BACKGROUNDPEN]);
            else
            {
                struct Rectangle all;

                all.MinX = 0;
                all.MinY = 0;
                all.MaxX = screen->Screen.Width - 1;
                all.MaxY = screen->Screen.Height - 1;
                DoHookClipRects(layer_info_hook, &screen->Screen.RastPort, &all);
            }

            //jDc: ALL screens MUST have BarLayer!
            CreateScreenBar(&screen->Screen, IntuitionBase);

            if (!screen->Screen.BarLayer) ok = FALSE;
        }

        if (ok)
        {
            if ((ns.Type & SCREENTYPE) == CUSTOMSCREEN)
            {
                screen->ShowPointer = showpointer;
            } else {
                screen->ShowPointer = TRUE;
            }
            GetAttr(POINTERA_SharedPointer, IBase->DefaultPointer, (IPTR *)&screen->Pointer);
            ObtainSharedPointer(screen->Pointer, IntuitionBase);

        }

        /*
        ** jDc: better modify the screen list in sync with inputhandler, this for example allows us to scan the list
        ** without any locks when we are on input.device context
        */
        if (ok)
        {
            struct PubScreenNode *ps = NULL;
            struct List *list = LockPubScreenList();

            /* Additional dupe check, just to be sure */
            if (!workbench && screen->pubScrNode && (ps = (struct PubScreenNode *)FindName(list,screen->pubScrNode->psn_Node.ln_Name)))
            {
                ok = FALSE;
            }

            if (ok)
            {
                struct OpenScreenActionMsg msg;

                msg.Screen = screen;
                msg.NewScreen = &ns;
                msg.List = list;

                DoSyncAction((APTR)int_openscreen,&msg.msg,IntuitionBase);

                ok = msg.Success;
            }

            UnlockPubScreenList();
        }

    } /* if (ok) */

    if (windowlock) UNLOCKWINDOW;

    if (!ok)
    {
        /* The bar layer is a child of the root layer: delete it before ThinLayerInfo() drops the root */
        if (screen->Screen.BarLayer)
        {

            KillScreenBar(&screen->Screen, IntuitionBase);
        }

        if (li_inited)
        {

            ThinLayerInfo(&screen->Screen.LayerInfo);
        }

        if (screen->Screen.ViewPort.ColorMap)
        {

            FreeColorMap(screen->Screen.ViewPort.ColorMap);
        }

        if (screen->DInfo.dri_Customize)
        {
            /* AROS: submenu image handling moved out of #ifdef */
            DisposeObject(screen->DInfo.dri_Customize->submenu);
            FreeMem(screen->DInfo.dri_Customize,sizeof (struct IntuitionCustomize));
        }
        if (screen->DInfo.dri_AmigaKey)
        {

            DisposeObject(screen->DInfo.dri_AmigaKey);
        }
        if (screen->DInfo.dri_CheckMark)
        {

            DisposeObject(screen->DInfo.dri_CheckMark);
        }

        if (screen->DInfo.dri_Font)
        {

            CloseFont(screen->DInfo.dri_Font);
        }

        if (screen->AllocatedBitMap && !(ns.Type & CUSTOMBITMAP))
        {

            FreeBitMap(screen->AllocatedBitMap);
        }

        if (screen->Screen.ViewPort.RasInfo)
        {

            FreeMem(screen->Screen.ViewPort.RasInfo, sizeof (struct RasInfo));
        }

        if (rp_inited)
        {

            DeinitRastPort(&screen->Screen.RastPort);
        }

        DisposeObject((APTR)screen);
        screen = 0;

    } /* if (!ok) */
    else if (screen->Screen.RastPort.BitMap &&
             screen->Screen.RastPort.BitMap != &screen->Screen.BitMap_OBSOLETE)
    {
        /* Managed bitmaps retain their drawing/display header and expose a
         * legacy snapshot. Raw custom screens already use the embedded one. */
        CopyMem(screen->Screen.RastPort.BitMap, &screen->Screen.BitMap_OBSOLETE, sizeof(struct BitMap));
    }


    return (struct Screen *)screen;

    AROS_LIBFUNC_EXIT

} /* OpenScreen */

VOID int_openscreen(struct OpenScreenActionMsg *msg,
                            struct IntuitionBase *IntuitionBase)
{
    struct GfxBase *GfxBase = GetPrivIBase(IntuitionBase)->GfxBase;
    struct IntScreen *screen = msg->Screen;
    struct NewScreen *ns = msg->NewScreen;
    struct List *list = msg->List;
    struct Screen *oldFirstScreen;
    struct RethinkDisplayActionMsg rmsg;
    ULONG lock = 0;

    /* If this is a public screen, we link it into the intuition global
       public screen list */
    if (screen->pubScrNode != NULL)
    {
        /* Set the pointer to ourselves */
        IS(screen)->pubScrNode->psn_Screen = &screen->Screen;

        AddTail(list, (struct Node *)IS(screen)->pubScrNode);
    }

    if (!ILOCKCHECK(((struct IntuiActionMsg *)msg))) lock = LockIBase((IPTR)NULL);

    oldFirstScreen = IntuitionBase->FirstScreen;
    if (ns->Type & SCREENBEHIND)
    {
        struct Screen **ptr = &IntuitionBase->FirstScreen;

        if (!*ptr)
            IntuitionBase->ActiveScreen = &screen->Screen;
        while (*ptr)
            ptr = &(*ptr)->NextScreen;
        *ptr = &screen->Screen;
    }
    else
    {
        screen->Screen.NextScreen = IntuitionBase->FirstScreen;
        IntuitionBase->FirstScreen =
            IntuitionBase->ActiveScreen = &screen->Screen;

    }

    /* The pointer goes to the frontmost screen */
    ShowPointer(IntuitionBase);

    /* set the default pub screen */
    if (IBase->IControlPrefs.ic_Flags & ICF_DEFPUBSCREEN)
    {
        if ((IntuitionBase->FirstScreen == &screen->Screen) && screen->pubScrNode && (screen->Screen.Flags & (PUBLICSCREEN | WBENCHSCREEN)))
        {
            IBase->DefaultPubScreen = &screen->Screen;
        }
    }

    msg->Success = MakeVPort(&IntuitionBase->ViewLord, &screen->Screen.ViewPort) ? FALSE : TRUE;

    if (msg->Success)
    {
        /* AROS: Put offsets validated by MakeVPort() back into screen structure */
        screen->Screen.LeftEdge = screen->Screen.ViewPort.DxOffset;
        screen->Screen.TopEdge = screen->Screen.ViewPort.DyOffset;

        rmsg.lock = FALSE;
        int_RethinkDisplay(&rmsg,IntuitionBase);
    }

    if (!ILOCKCHECK(((struct IntuiActionMsg *)msg))) UnlockIBase(lock);

    AddResourceToList(screen, RESOURCE_SCREEN, IntuitionBase);
}
