/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.
    Copyright (C) 2001-2013, The MorphOS Development Team. All Rights Reserved.
*/

#include <exec/types.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/layers.h>
#include <proto/intuition.h>
#include <clib/macros.h>
#include <intuition/intuitionbase.h>
#include <intuition/gadgetclass.h>
#include <intuition/imageclass.h>
#include <intuition/windecorclass.h>
#include <intuition/scrdecorclass.h>
#include <intuition/preferences.h>
#include <intuition/extensions.h>
#include <graphics/layers.h>
#include <graphics/rpattr.h>
#include <graphics/gfxmacros.h>

#include "intuition_intern.h"
#include "intuition_preferences.h"

#include "boopsigadgets.h"
#include "showhide.h"

ULONG addextragadget(struct Window *w,BOOL is_gzz,struct DrawInfo *dri,LONG relright,ULONG imagetype,ULONG gadgetid,ULONG gadgettype,struct IntuitionBase *IntuitionBase);

/**********************************************************************************/

void SetDisplayDefaults(struct IntuitionBase * IntuitionBase)
{
    struct IntIntuitionBase *_intuitionBase = GetPrivIBase(IntuitionBase);

    if (!_intuitionBase->ScreenModePrefs)
        _intuitionBase->ScreenModePrefs = AllocMem(sizeof(struct IScreenModePrefs), MEMF_ANY);

    if (_intuitionBase->ScreenModePrefs)
    {
        _intuitionBase->ScreenModePrefs->smp_DisplayID  = INVALID_ID;
        _intuitionBase->ScreenModePrefs->smp_Control    = 0;
        _intuitionBase->ScreenModePrefs->smp_Width      = AROS_NOMINAL_WIDTH;
        _intuitionBase->ScreenModePrefs->smp_Height     = AROS_NOMINAL_HEIGHT;
        _intuitionBase->ScreenModePrefs->smp_Depth      = AROS_NOMINAL_DEPTH;
    }
}

void LoadDefaultPreferences(struct IntuitionBase * IntuitionBase)
{
    struct IntIntuitionBase *_intuitionBase = GetPrivIBase(IntuitionBase);
        static CONST UWORD DriPens2[NUMDRIPENS] = { 1, 0, 1, 1, 1, 1, 0, 0, 1, 0, 1, 1};
        static CONST UWORD DriPens4[NUMDRIPENS] = { 1, 0, 1, 2, 1, 3, 1, 0, 2, 1, 2, 1};
        /* 8 colours and more: white shines and paper, grey fills */
        static CONST UWORD DriPens8[NUMDRIPENS] = { 1, 0, 1, 0, 1, 2, 1, 0, 3, 1, 0, 1};

    /* Default IControl prefs are AROS addition. Keep while backporting. */
    _intuitionBase->IControlPrefs.ic_TimeOut  = 50;
    _intuitionBase->IControlPrefs.ic_MetaDrag = IEQUALIFIER_LCOMMAND;
    _intuitionBase->IControlPrefs.ic_Flags    = ICF_3DMENUS |
                                                                                         ICF_OFFSCREENLAYERS |
                                                             ICF_AVOIDWINBORDERERASE |
                                                                         ICF_MODEPROMOTE |
                                                                         ICF_MENUSNAP |
                                                             ICF_STRGAD_FILTER |
                                                             ICF_COERCE_LACE;
    _intuitionBase->IControlPrefs.ic_WBtoFront   = 'N';
    _intuitionBase->IControlPrefs.ic_FrontToBack = 'M';
    _intuitionBase->IControlPrefs.ic_ReqTrue     = 'V';
    _intuitionBase->IControlPrefs.ic_ReqFalse    = 'B';

    CopyMem(&IntuitionDefaultPreferences,
                &_intuitionBase->DefaultPreferences,
                sizeof(struct Preferences));

    /*
     * Mouse default. A pointer that tracks the reported movement one to
     * one is unusable on anything but the lowest-resolution mice.
     */
    _intuitionBase->DefaultPreferences.PointerTicks = 2;

    CopyMem(&_intuitionBase->DefaultPreferences,
            &_intuitionBase->ActivePreferences,
            sizeof(struct Preferences));

    CopyMem(DriPens2, _intuitionBase->DriPens2, sizeof(DriPens2));
    CopyMem(DriPens4, _intuitionBase->DriPens4, sizeof(DriPens4));
    CopyMem(DriPens8, _intuitionBase->DriPens8, sizeof(DriPens8));
}

/**********************************************************************************/

void CheckRectFill(struct RastPort *rp, WORD x1, WORD y1, WORD x2, WORD y2,
                   struct IntuitionBase * IntuitionBase)
{
    struct IntIntuitionBase *_intuitionBase = GetPrivIBase(IntuitionBase);
    struct GfxBase *GfxBase = _intuitionBase->GfxBase;

    if ((x2 >= x1) && (y2 >= y1))
    {
        RectFill(rp, x1, y1, x2, y2);
    }
}

/**********************************************************************************/

/*
 * The image of a system gadget, shared by the windows of the screen: width
 * 0 keeps the image's aspect at the height.  Images past the screen's
 * table go to the window alone; sysimageshared() tells them apart.
 */
static Object *sysimage(struct Screen *scr, struct DrawInfo *dri, ULONG which,
                        WORD width, WORD height, struct IntuitionBase *IntuitionBase)
{
    struct IntScreen *is = IS(scr);
    struct Image *im;
    Object *o;
    UWORD i, slot = NUM_SYSIMAGES;
    struct TagItem image_tags[] =
    {
        {SYSIA_Which    , which                                     },
        {SYSIA_DrawInfo , (IPTR)dri                                 },
        {SYSIA_Size     , scr->Flags & SCREENHIRES ?
                          SYSISIZE_MEDRES : SYSISIZE_LOWRES         },
        {IA_Height      , height                                    },
        {width ? IA_Width : TAG_DONE, width                         },
        {TAG_DONE                                                   }
    };

    for (i = 0; i < NUM_SYSIMAGES; i++)
    {
        im = (struct Image *)is->SysImages[i];
        if (!im)
        {
            if (slot == NUM_SYSIMAGES)
                slot = i;
        }
        else if (is->SysImageWhich[i] == which && im->Height == height &&
                 (!width || im->Width == width))
            return (Object *)im;
    }

    o = NewObjectA(NULL, SYSICLASS, image_tags);
    if (o && slot < NUM_SYSIMAGES)
    {
        is->SysImages[slot] = o;
        is->SysImageWhich[slot] = which;
    }

    return o;
}

static BOOL sysimageshared(struct Screen *scr, Object *im)
{
    UWORD i;

    for (i = 0; i < NUM_SYSIMAGES; i++)
        if (IS(scr)->SysImages[i] == im)
            return TRUE;
    return FALSE;
}

void FreeSysImages(struct Screen *scr, struct IntuitionBase *IntuitionBase)
{
    UWORD i;

    for (i = 0; i < NUM_SYSIMAGES; i++)
        DisposeObject(IS(scr)->SysImages[i]);
}

/**********************************************************************************/

/* The gadgets a window can have in its title bar in addition to the standard
   ones: the WA_ExtraTitlebarGadgets flag that asks for it, its image, the
   offset of its gadget ID from the window's first extra gadget ID, and its
   slot in the window's system gadget array. */
static const struct
{
    UBYTE flag;
    UBYTE imagetype;
    UBYTE id;
    UBYTE gadgettype;
}
extragadgets[] =
{
    { ETG_LOCK    , LOCKIMAGE    , ETD_Lock    , LOCKGAD     },
    { ETG_ICONIFY , ICONIFYIMAGE , ETD_Iconify , ICONIFYGAD  },
    { ETG_JUMP    , JUMPIMAGE    , ETD_Jump    , JUMPGAD     },
    { ETG_SNAPSHOT, SNAPSHOTIMAGE, ETD_Snapshot, SNAPSHOTGAD },
    { ETG_MUI     , MUIIMAGE     , ETD_MUI     , MUIGAD      },
    { ETG_POPUP   , POPUPIMAGE   , ETD_PopUp   , POPUPGAD    }
};

/* Create the depth, zoom or close gadget of a window together with its image.
   The depth and zoom gadgets are placed to the left of *relright, which is
   moved on by their width; the close gadget (relright == NULL) sits at the
   left edge of the window. Returns FALSE if the gadget could not be created. */
static BOOL addtitlegadget(struct Window *w, BOOL is_gzz, struct DrawInfo *dri,
                           ULONG imagetype, ULONG gtyp, ULONG gadgettype, LONG *relright,
                           struct IntuitionBase *IntuitionBase)
{
    ULONG TitleHeight = w->BorderTop;
    struct TagItem gadget_tags[] =
    {
        {GA_Image           , 0                 },
        {relright ? GA_RelRight : GA_Left, 0    },
        {GA_Top             , 0                 },
        {GA_Width           , TitleHeight       },
        {GA_Height          , TitleHeight       },
        {GA_SysGadget   , TRUE              },
        {GA_SysGType    , gtyp              },
        {GA_TopBorder   , TRUE              },
        {GA_GZZGadget   , is_gzz            },
        {GA_RelVerify   , TRUE              },
        {TAG_DONE                               }
    };
    Object *im;

    im = sysimage(w->WScreen, dri, imagetype, 0, TitleHeight, IntuitionBase);
    if (!im)
        return FALSE;

    gadget_tags[0].ti_Data = (IPTR)im;
    /* the gadget takes the image's width */
    if (relright)
    {
        *relright -= ((struct Image *)im)->Width;
        gadget_tags[1].ti_Data = *relright;
    }

    SYSGAD(w, gadgettype) = NewObjectA(NULL, BUTTONGCLASS, gadget_tags);

    if (!SYSGAD(w, gadgettype))
    {
        if (!sysimageshared(w->WScreen, im))
            DisposeObject(im);
        return FALSE;
    }

    return TRUE;
}

/**********************************************************************************/

BOOL CreateWinSysGadgets(struct Window *w, struct IntuitionBase *IntuitionBase)
{

    struct DrawInfo *dri;
    BOOL                 is_gzz;
    ULONG            TitleHeight = w->BorderTop;

    

    is_gzz = (w->Flags & WFLG_GIMMEZEROZERO) ? TRUE : FALSE;

    dri = GetScreenDrawInfo(w->WScreen);
    if (dri)
    {
        LONG relright;
        BOOL sysgads_ok = TRUE;
        UWORD extra;

        /* Relright of rightmost button */
        //relright = - (TitleHeight - 1);

        relright = 1;

        /* Now try to create the various gadgets */

        if (w->Flags & WFLG_SIZEGADGET)
        {
            /* this code must not change the 'relright' variable */
            WORD width  = ((struct IntWindow *)w)->sizeimage_width;
            WORD height = ((struct IntWindow *)w)->sizeimage_height;

            struct TagItem size_tags[] =
            {
                {GA_Image           , 0             },
                {GA_RelRight    , -width  + 1   },
                {GA_RelBottom   , -height + 1   },
                {GA_Width           , width             },
                {GA_Height          , height            },
                {GA_SysGadget   , TRUE              },
                {GA_SysGType    , GTYP_SIZING   },
                {GA_BottomBorder, TRUE              },
                {GA_RightBorder , TRUE              },
                {GA_GZZGadget   , is_gzz            },
                {TAG_DONE                               }
            };

            Object *im;

            im = sysimage(w->WScreen, dri, SIZEIMAGE, width, height, IntuitionBase);
            if (!im)
            {
                sysgads_ok = FALSE;
            }
            else
            {
                size_tags[0].ti_Data = (IPTR)im;

                SYSGAD(w, SIZEGAD) = NewObjectA(NULL, BUTTONGCLASS, size_tags);

                if (!SYSGAD(w, SIZEGAD))
                {
                    if (!sysimageshared(w->WScreen, im))
                        DisposeObject(im);
                    sysgads_ok = FALSE;
                }
            }
        }

        if (w->Flags & WFLG_DEPTHGADGET)
        {
            if (!addtitlegadget(w, is_gzz, dri, DEPTHIMAGE, GTYP_WDEPTH, DEPTHGAD, &relright, IntuitionBase))
                sysgads_ok = FALSE;
        }

            /* RKRMs: window gets zoom gadget if WA_Zoom tag was used,
           or if window has both a sizegadget and a depthgadget */

        if ((w->Flags & WFLG_HASZOOM) ||
            ((w->Flags & WFLG_SIZEGADGET) && (w->Flags & WFLG_DEPTHGADGET)))
        {
            if (!addtitlegadget(w, is_gzz, dri, ZOOMIMAGE, GTYP_WZOOM, ZOOMGAD, &relright, IntuitionBase))
                sysgads_ok = FALSE;
        }

        /* The extra title bar gadgets, from right to left */
        for (extra = 0; extra < sizeof(extragadgets) / sizeof(extragadgets[0]); extra++)
        {
            if (((struct IntWindow *)(w))->extrabuttons & extragadgets[extra].flag)
            {
                relright -= addextragadget(w, is_gzz, dri, relright, extragadgets[extra].imagetype,
                                           ((struct IntWindow *)w)->extrabuttonsid + extragadgets[extra].id,
                                           extragadgets[extra].gadgettype, IntuitionBase);
            }
        }

        if (w->Flags & WFLG_CLOSEGADGET)
        {
            if (!addtitlegadget(w, is_gzz, dri, CLOSEIMAGE, GTYP_CLOSE, CLOSEGAD, NULL, IntuitionBase))
                sysgads_ok = FALSE;
        }

        if (w->Flags & WFLG_DRAGBAR)
        {

            struct TagItem dragbar_tags[] =
            {
                {GA_Left            , 0/*db_left*/            },
                {GA_Top             , 0                     },
                {GA_RelWidth    , 0/*db_width*/     },
                {GA_Height          , TitleHeight       },
                {GA_SysGadget   , TRUE              },
                {GA_SysGType    , GTYP_WDRAGGING    },
                {GA_TopBorder   , TRUE              },
                {GA_GZZGadget   , is_gzz            },
                {TAG_DONE                                }
            };
            SYSGAD(w, DRAGBAR) = NewObjectA(NULL, BUTTONGCLASS, dragbar_tags);

            if (!SYSGAD(w, DRAGBAR))
                sysgads_ok = FALSE;

        }

        /* Don't need drawinfo anymore */
        FreeScreenDrawInfo(w->WScreen, dri);

        if (sysgads_ok)
        {
            int i;

            for (i = NUM_SYSGADS; --i >= 0; )
            {
                if (SYSGAD(w, i))
                {
                        struct wdpLayoutBorderGadgets  msg;

                    msg.MethodID    = WDM_LAYOUT_BORDERGADGETS;
                    msg.wdp_Window  = w;
                    msg.wdp_Gadgets = (struct Gadget *)SYSGAD(w, i);
                    msg.wdp_Flags   = WDF_LBG_SYSTEMGADGET | WDF_LBG_INITIAL;
                    msg.wdp_UserBuffer  = ((struct IntWindow *)(w))->DecorUserBuffer;
                    msg.wdp_ExtraButtons = ((struct IntWindow *)w)->extrabuttons;

                    msg.wdp_TrueColor = (((struct IntScreen *)w->WScreen)->DInfo.dri_Flags & DRIF_DIRECTCOLOR) ? TRUE : FALSE;
                    msg.wdp_Dri = dri;

                    DoMethodA(((struct IntScreen *)(w->WScreen))->WinDecorObj, (Msg)&msg);

                    AddGadget(w, (struct Gadget *)SYSGAD(w, i), 0);
                }
            }

            return TRUE;

        } /* if (sysgads created) */

        KillWinSysGadgets(w, IntuitionBase);

    } /* if (got DrawInfo) */
    return FALSE;

}

/**********************************************************************************/

VOID KillWinSysGadgets(struct Window *w, struct IntuitionBase *IntuitionBase)
{
    /* Free system gadgets */
    UWORD i;

    for (i = 0; i < NUM_SYSGADS; i ++)
    {
        if (SYSGAD(w, i))
        {
            Object *im = (Object *)((struct Gadget *)SYSGAD(w, i))->GadgetRender;

            RemoveGadget( w, (struct Gadget *)SYSGAD(w, i));
            if (!sysimageshared(w->WScreen, im))
                DisposeObject(im);
            DisposeObject( SYSGAD(w, i) );
        }
    }
}

/**********************************************************************************/

void CreateScreenBar(struct Screen *scr, struct IntuitionBase *IntuitionBase)
{
    struct IntIntuitionBase *_intuitionBase = GetPrivIBase(IntuitionBase);
    struct GfxBase *GfxBase = _intuitionBase->GfxBase;
    struct LayersBase *LayersBase = _intuitionBase->LayersBase;
    BOOL  front = TRUE;
    ULONG backdrop = LAYERBACKDROP;
    WORD  ypos = 0;

    if (scr->Flags & SCREENQUIET) front = FALSE;

    if (!scr->BarLayer)
    {

        if (front)
        {
            scr->BarLayer = CreateUpfrontHookLayer(&scr->LayerInfo,
                                                   scr->RastPort.BitMap,
                                                   0,
                                                   ypos,
                                                   scr->Width - 1,
                                                   scr->BarHeight + ypos, /* 1 pixel heigher than scr->BarHeight */
                                                   LAYERSIMPLE | backdrop,
                                                   LAYERS_NOBACKFILL,
                                                   NULL);
        }
        else
        {
            scr->BarLayer = CreateBehindHookLayer(&scr->LayerInfo,
                                                   scr->RastPort.BitMap,
                                                   0,
                                                   ypos,
                                                   scr->Width - 1,
                                                   scr->BarHeight + ypos, /* 1 pixel heigher than scr->BarHeight */
                                                   LAYERSIMPLE | backdrop,
                                                   LAYERS_NOBACKFILL,
                                                   NULL);
        }

        if (scr->BarLayer)
        {

            SetFont(scr->BarLayer->rp, ((struct IntScreen *)scr)->DInfo.dri_Font);
            if (!(scr->Flags & SCREENQUIET)) {

                RenderScreenBar(scr, FALSE, IntuitionBase);
            }

        }
        else
        {

        }
    }
    else
    {

    }
}

/**********************************************************************************/

void KillScreenBar(struct Screen *scr, struct IntuitionBase *IntuitionBase)
{
    struct IntIntuitionBase *_intuitionBase = GetPrivIBase(IntuitionBase);
    struct LayersBase *LayersBase = _intuitionBase->LayersBase;

    if (scr->BarLayer)
    {
        DeleteLayer(0, scr->BarLayer);
        scr->BarLayer = FALSE;
    }

}

/**********************************************************************************/

static void renderScreenBar(struct Screen *scr, BOOL refresh, ULONG flags,
    struct IntuitionBase *IntuitionBase)
{
    struct IntIntuitionBase *_intuitionBase = GetPrivIBase(IntuitionBase);
    struct GfxBase *GfxBase = _intuitionBase->GfxBase;
    struct LayersBase *LayersBase = _intuitionBase->LayersBase;
    struct RastPort *rp;

    if (scr->BarLayer)
    {
        BOOL beeping;

        rp = scr->BarLayer->rp;

        /* must lock GadgetLock to avoid deadlocks with ObtainGIRPort
           when calling refreshgadget inside layer update state */
        LockLayerInfo(scr->BarLayer->LayerInfo);
        LOCKGADGET(IntuitionBase)
        LockLayer(0, scr->BarLayer);

        beeping = (scr->Flags & BEEPING) && GetBitMapAttr(rp->BitMap, BMA_DEPTH) > 8;

        if (refresh) BeginUpdate(scr->BarLayer);

        {
                struct sdpDrawScreenBar  msg;

            msg.MethodID        = SDM_DRAW_SCREENBAR;
            msg.sdp_Layer        = scr->BarLayer;
            msg.sdp_RPort        = rp;
                msg.sdp_Flags        = flags;
            msg.sdp_Screen        = scr;
            msg.sdp_Dri           = (struct DrawInfo *)&((struct IntScreen *)scr)->DInfo;
            msg.sdp_UserBuffer = ((struct IntScreen *)(scr))->DecorUserBuffer;
            msg.sdp_TrueColor   = (((struct IntScreen *)(scr))->DInfo.dri_Flags & DRIF_DIRECTCOLOR) ? TRUE : FALSE;

            DoMethodA(((struct IntScreen *)(scr))->ScrDecorObj, (Msg)&msg);
        }

        if (!(flags & SDF_DSB_TITLEONLY) && scr->FirstGadget)
        {
            RefreshBoopsiGadget(scr->FirstGadget, (struct Window *)scr, NULL, IntuitionBase);
        }

        if (refresh)
        {

            scr->BarLayer->Flags &= ~LAYERREFRESH;
            EndUpdate(scr->BarLayer, TRUE);
        }

        if (beeping) {
            /* FIXME: Shouldn't we 'beep' at this point? */

        }

        UnlockLayer(scr->BarLayer);
        UNLOCKGADGET(IntuitionBase)
        UnlockLayerInfo(scr->BarLayer->LayerInfo);

    } /* if (scr->BarLayer) */

}

void RenderScreenBar(struct Screen *scr, BOOL refresh, struct IntuitionBase *IntuitionBase)
{
    renderScreenBar(scr, refresh, 0, IntuitionBase);
}

void RenderScreenTitle(struct Screen *scr, struct IntuitionBase *IntuitionBase)
{
    renderScreenBar(scr, FALSE, SDF_DSB_TITLEONLY, IntuitionBase);
}

/**********************************************************************************/

void UpdateMouseCoords(struct Window *win)
{
    WORD scrmousex = win->WScreen->MouseX;
    WORD scrmousey = win->WScreen->MouseY;

    win->MouseX    = scrmousex - win->LeftEdge;
    win->MouseY    = scrmousey - win->TopEdge;

    /* stegerg: AmigaOS sets this even if window is not GZZ
       so we do the same as they are handy also for non-GZZ
       windows */

    win->GZZMouseX = scrmousex - (win->LeftEdge + win->BorderLeft);
    win->GZZMouseY = scrmousey - (win->TopEdge + win->BorderTop);
}

/**********************************************************************************/

/* subtract rectangle b from rectangle b. resulting rectangles will be put into
   destrectarray which must have place for at least 4 rectangles. Returns number
   of resulting rectangles */

WORD SubtractRectFromRect(struct Rectangle *a, struct Rectangle *b, struct Rectangle *destrectarray)
{
    struct Rectangle    intersect;
    BOOL                    intersecting = FALSE;
    WORD                    numrects = 0;

    /* calc. intersection between a and b */

    if (a->MinX <= b->MaxX)
    {
        if (a->MinY <= b->MaxY)
        {
            if (a->MaxX >= b->MinX)
            {
                if (a->MaxY >= b->MinY)
                {
                    intersect.MinX = MAX(a->MinX, b->MinX);
                    intersect.MinY = MAX(a->MinY, b->MinY);
                    intersect.MaxX = MIN(a->MaxX, b->MaxX);
                    intersect.MaxY = MIN(a->MaxY, b->MaxY);

                    intersecting = TRUE;
                }
            }
        }
    }

    if (!intersecting)
    {
        destrectarray[numrects++] = *a;

    } /* not intersecting */
    else
    {
        if (intersect.MinY > a->MinY) /* upper */
        {
            destrectarray->MinX = a->MinX;
            destrectarray->MinY = a->MinY;
            destrectarray->MaxX = a->MaxX;
            destrectarray->MaxY = intersect.MinY - 1;

            numrects++;
            destrectarray++;
        }

        if (intersect.MaxY < a->MaxY) /* lower */
        {
            destrectarray->MinX = a->MinX;
            destrectarray->MinY = intersect.MaxY + 1;
            destrectarray->MaxX = a->MaxX;
            destrectarray->MaxY = a->MaxY;

            numrects++;
            destrectarray++;
        }

        if (intersect.MinX > a->MinX) /* left */
        {
            destrectarray->MinX = a->MinX;
            destrectarray->MinY = intersect.MinY;
            destrectarray->MaxX = intersect.MinX - 1;
            destrectarray->MaxY = intersect.MaxY;

            numrects++;
            destrectarray++;
        }

        if (intersect.MaxX < a->MaxX) /* right */
        {
            destrectarray->MinX = intersect.MaxX + 1;
            destrectarray->MinY = intersect.MinY;
            destrectarray->MaxX = a->MaxX;
            destrectarray->MaxY = intersect.MaxY;

            numrects++;
            destrectarray++;
        }

    } /* intersecting */

    return numrects;

}

ULONG addextragadget(struct Window *w,BOOL is_gzz,struct DrawInfo *dri,LONG relright,ULONG imagetype,ULONG gadgetid,ULONG gadgettype,struct IntuitionBase *IntuitionBase)
{
    ULONG TitleHeight = w->BorderTop;
    struct TagItem gadget_tags[] =
    {
        {GA_Image           , 0                 },
        {GA_ToggleSelect, FALSE             },
        {GA_Top             , 0                 },
        {GA_Height          , TitleHeight       },
        {GA_TopBorder   , TRUE              },
        {GA_GZZGadget   , is_gzz            },
        {GA_ID              , gadgetid          },
        {GA_RelVerify        , TRUE              },
        {TAG_DONE                           }
    };

    Object *im;

    if (gadgettype == LOCKGAD)
        gadget_tags[1].ti_Data = TRUE;

    im = sysimage(w->WScreen, dri, imagetype, 0, TitleHeight, IntuitionBase);
    if (im)
    {
        gadget_tags[0].ti_Data = (IPTR)im;

        SYSGAD(w, gadgettype) = NewObjectA(NULL, BUTTONGCLASS, gadget_tags);

        if (!SYSGAD(w, gadgettype))
        {
            if (!sysimageshared(w->WScreen, im))
                DisposeObject(im);
        }
        else
        {
            IPTR width;
            GetAttr(GA_Width, SYSGAD(w, gadgettype), &width);

            --width;

            {
                struct TagItem gadtags[] =
                {
                    {GA_RelRight, relright - width  },
                    {TAG_DONE                           }
                };

                SetAttrsA(SYSGAD(w, gadgettype), gadtags);
            }

            return width;
        }
    }

    return 0;

}

/**********************************************************************************/

/* Use the FNV-1 hash function over the object's pointer.
 * http://en.wikipedia.org/wiki/Fowler-Noll-Vo_hash_function
 */
LONG CalcResourceHash(APTR resource)
{
    const ULONG FNV1_32_Offset = 2166136261UL;
    const ULONG FNV1_32_Prime  = 16777619UL;
    IPTR data = (IPTR)resource;
    ULONG hash;
    int i;

    hash = FNV1_32_Offset;
    for (i = 0; i < sizeof(APTR); i++) {
                hash *= FNV1_32_Prime;
                hash ^= data & 0xff;
                data >>= 8;
    }

    return hash & (RESOURCELIST_HASHSIZE-1);
}

/**********************************************************************************/

void AddResourceToList(APTR resource, UWORD resourcetype, struct IntuitionBase *IntuitionBase)
{
    struct IntIntuitionBase *_intuitionBase = GetPrivIBase(IntuitionBase);
    struct HashNode *hn = NULL;
    LONG                 hash;
    ULONG                ilock;

    switch(resourcetype)
    {
            case RESOURCE_WINDOW:
            hn = &((struct IntWindow *)resource)->hashnode;
            hn->type = RESOURCE_WINDOW;
            break;

            case RESOURCE_SCREEN:
            hn = &((struct IntScreen *)resource)->hashnode;
            hn->type = RESOURCE_SCREEN;
            break;

        default:

            return;
    }

    hash = CalcResourceHash(resource);

    hn->resource = resource;

    ilock = LockIBase(0);
    AddTail((struct List *)&_intuitionBase->ResourceList[hash], (struct Node *)hn);
    UnlockIBase(ilock);
}

/**********************************************************************************/

void RemoveResourceFromList(APTR resource, UWORD resourcetype, struct IntuitionBase *IntuitionBase)
{
    struct HashNode *hn = NULL;
    ULONG                ilock;

    switch(resourcetype)
    {
            case RESOURCE_WINDOW:
            hn = &((struct IntWindow *)resource)->hashnode;
            break;

            case RESOURCE_SCREEN:
            hn = &((struct IntScreen *)resource)->hashnode;
            break;

        default:

            return;
    }

    if (hn->type != resourcetype)
    {

    }

    ilock = LockIBase(0);
    Remove((struct Node *)hn);
    UnlockIBase(ilock);
}

/**********************************************************************************/

BOOL ResourceExisting(APTR resource, UWORD resourcetype, struct IntuitionBase *IntuitionBase)
{
    struct IntIntuitionBase *_intuitionBase = GetPrivIBase(IntuitionBase);
    struct HashNode *hn = NULL;
    LONG                 hash;
    ULONG                ilock;
    BOOL                 exists = FALSE;

    hash = CalcResourceHash(resource);

    ilock = LockIBase(0);
    for (hn = (struct HashNode *)_intuitionBase->ResourceList[hash].mlh_Head; hn->node.mln_Succ; hn = (struct HashNode *)hn->node.mln_Succ)
    {
        if ((hn->resource == resource) && (hn->type == resourcetype))
        {
            exists = TRUE;
            break;
        }
    }
    UnlockIBase(ilock);

    return exists;
}

AROS_UFH3(BOOL, DefaultWindowShapeFunc,
    AROS_UFHA(struct Hook *, hook, A0),
    AROS_UFHA(struct Layer *, lay, A2),
    AROS_UFHA(struct ShapeHookMsg *, msg, A1))
{
    AROS_USERFUNC_INIT

    struct IntuitionBase    *IntuitionBase = (struct IntuitionBase *)hook->h_Data;
    struct IntIntuitionBase *_intuitionBase = GetPrivIBase(IntuitionBase);
    struct GfxBase *GfxBase = _intuitionBase->GfxBase;
    struct Window               *win = (struct Window *)hook->h_SubEntry;
    struct Region               *shape;
    struct wdpWindowShape    shapemsg;

    shapemsg.MethodID            = WDM_WINDOWSHAPE;
    shapemsg.wdp_TrueColor  = (GetPrivScreen(win->WScreen)->DInfo.dri_Flags & DRIF_DIRECTCOLOR) ? TRUE : FALSE;
    shapemsg.wdp_Width             = msg->NewBounds->MaxX - msg->NewBounds->MinX + 1;
    shapemsg.wdp_Height     = msg->NewBounds->MaxY - msg->NewBounds->MinY + 1;
    shapemsg.wdp_Window = win;
    shapemsg.wdp_UserBuffer = IW(win)->DecorUserBuffer;

    shape = (struct Region *)DoMethodA(GetPrivScreen(win->WScreen)->WinDecorObj, (Msg)&shapemsg);

    if (IW(win)->OutlineShape) DisposeRegion(IW(win)->OutlineShape);
    IW(win)->OutlineShape = shape;
    IW(win)->CustomShape = FALSE;

    msg->NewShape = shape;
    return TRUE;

    AROS_USERFUNC_EXIT
}

/**********************************************************************************/
