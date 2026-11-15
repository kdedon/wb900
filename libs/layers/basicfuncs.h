/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: Basic support functions for layers.library.
*/

#include "layers_intern.h"
#include <exec/types.h>
#include <graphics/gfx.h>
#include <graphics/clip.h>

/***************************************************************************/
/*                                  HOOK                                   */
/***************************************************************************/

void _CallLayerHook(struct Hook * h,
                    struct RastPort * rp,
                    struct Layer * L,
                    struct Rectangle * R,
                    WORD offsetx,
                    WORD offsety,
                    struct LayersBase * LayersBase);

/* The RastPort a hook renders into a bitmap other than the application's
   through - see _ObtainHookRastPort() */
struct HookRastPort
{
    struct RastPort * rp;
    struct BitMap   * bm;
};

struct RastPort *_ObtainHookRastPort(struct HookRastPort *h, struct RastPort *rp,
                                     struct BitMap *bm, struct LayersBase *LayersBase);
void _ReleaseHookRastPort(struct HookRastPort *h, struct LayersBase *LayersBase);

/***************************************************************************/
/*                                 LAYER                                   */
/***************************************************************************/

struct Layer *CreateLayerTagList(struct Layer_Info *li, struct BitMap *bm, LONG x0, LONG y0, 
				 LONG x1, LONG y1, LONG flags, int priority, struct TagItem *tagList,
				 struct LayersBase *LayersBase);
void _FreeLayer(struct Layer * l, struct LayersBase *LayersBase);
struct Layer *GetFirstFamilyMember(struct Layer *l);

/***************************************************************************/
/*                               LAYERINFO                                 */
/***************************************************************************/

void _AllocExtLayerInfo
    (struct Layer_Info * li, struct LayersBase *LayersBase);

void _FreeExtLayerInfo
    (struct Layer_Info * li, struct LayersBase *LayersBase);

/***************************************************************************/
/*                                RECTANGLE                                */
/***************************************************************************/

BOOL AndRectRect(struct Rectangle *rect1, struct Rectangle *rect2, struct Rectangle *intersect);

/***************************************************************************/
/*                            RESOURCE HANDLING                            */
/***************************************************************************/

struct ClipRect * _AllocClipRect(struct Layer * L, struct LayersBase *LayersBase);
void _FreeClipRect(struct ClipRect * CR, struct Layer * L, struct LayersBase *LayersBase);
void _FreeClipRectListBM(struct Layer * L, struct ClipRect * CR, struct LayersBase *LayersBase);

/***************************************************************************/
/*                              MISCELLANEOUS                              */
/***************************************************************************/

int _CopyClipRectsToClipRects(struct Layer * l,
                              struct ClipRect * oldcr,
                              struct ClipRect * newcr,
			      int destdx,
                              int backupmode,
                              int freelist,
                              int addtodamagelist,
                              int fresh,
			      struct LayersBase *LayersBase);

/*-----------------------------------END-----------------------------------*/

#define RECTAREA(r) ((LONG)((r)->MaxX - (r)->MinX + 1) * ((r)->MaxY - (r)->MinY + 1))

#define DO_OVERLAP(r1,r2) (!( ((r1)->MinX > (r2)->MaxX) ||	\
                              ((r1)->MinY > (r2)->MaxY) ||	\
                              ((r1)->MaxX < (r2)->MinX) ||	\
                              ((r1)->MaxY < (r2)->MinY) ))

#define IS_EMPTYREGION(r) (NULL == (r)->RegionRectangle)

#define IS_ROOTLAYER(l) ((l) == (l)->LayerInfo->check_lp)

int _BackupPartsOfLayer(struct Layer * l, 
                        struct Region * hide_region,
                        int dx,
                        int backupsimplerefresh,
                        struct LayersBase *);

int _ShowPartsOfLayer(struct Layer * l, 
                      struct Region * show_region,
                      struct LayersBase *);

int _ShowLayer(struct Layer * l, struct LayersBase *LayersBase);
void _AndShapeRegion(struct Layer *l, struct Region *r, struct LayersBase *LayersBase);
struct Layer *_ShowLayersBehind(struct Layer *l, struct Region *r, struct Region *clear,
                                struct Rectangle *ovl1, struct Rectangle *ovl2, int visshape,
                                struct LayersBase *LayersBase);

void _HideLayersBehind(struct Layer *l, struct Region *hide, struct Rectangle *ovl, struct LayersBase *LayersBase);

void _BackFillRegion(struct Layer * l, 
                     struct Region * r,
                     int addtodamagelist,
                     struct LayersBase * LayersBase);

BOOL _AllocClipRectBitMap(struct ClipRect *cr, struct BitMap *display_bm, int aligndx, struct LayersBase *LayersBase);
BOOL _IsLayerHitAt(struct Layer *l, LONG x, LONG y, struct LayersBase *LayersBase);
struct ClipRect * _CreateClipRectsFromRegion(struct Region *r,
                                             struct Layer * l,
                                             int invisible,
                                             struct Region * inverter,
                                             int aligndx,
                                             struct ClipRect * viewsof,
					     struct LayersBase *LayersBase);

struct Region *_InternalInstallClipRegion(struct Layer *l, struct Region *region,
    	    	    	    	    	  WORD destdx,
    	    	    	    	    	  struct LayersBase *LayersBase);
