/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: Basic support functions for layers.library.
*/

#include <exec/memory.h>
#include <graphics/rastport.h>
#include <graphics/clip.h>
#include <graphics/regions.h>
#include <graphics/layers.h>
#include <graphics/gfx.h>
#include <utility/hooks.h>

#include "layers_intern.h"
#include "basicfuncs.h"


/*
 *  Sections:
 *
 *  + Hook
 *  + Layer
 *  + LayerInfo
 *  + Rectangle
 *  + Resource Handling
 *  + Miscellaneous
 *
 */

/***************************************************************************/
/*                                  HOOK                                   */
/***************************************************************************/

struct layerhookmsg
{
    struct Layer *l;
/*  struct Rectangle rect; (replaced by the next line!) */
    WORD MinX, MinY, MaxX, MaxY;
    LONG OffsetX, OffsetY;
};

/*
 * The RastPort a hook renders into bm through: the application's RastPort
 * rp, its BitMap swapped for the duration. The layer lock keeps everyone
 * else away from it.
 */
struct RastPort *_ObtainHookRastPort(struct HookRastPort *h, struct RastPort *rp,
                                     struct BitMap *bm, struct LayersBase *LayersBase)
{
  h->rp = rp;
  h->bm = rp->BitMap;
  rp->BitMap = bm;
  return rp;
}

void _ReleaseHookRastPort(struct HookRastPort *h, struct LayersBase *LayersBase)
{
  h->rp->BitMap = h->bm;
}

void _CallLayerHook(struct Hook * h,
                    struct RastPort * rp,
                    struct Layer * L,
                    struct Rectangle * R,
                    WORD offsetX,
                    WORD offsetY,
                    struct LayersBase * LayersBase)
{
  struct BitMap * bm = rp->BitMap;

  if (L)
  {
    if (IL(L)->intflags & INTFLAG_AVOID_BACKFILL) return;
  }
  
  if (h == LAYERS_BACKFILL)
  {
    /* Use default backfill, which means that I will clear the area */
    BltBitMap(bm,
              0,
              0,
              bm,
              R->MinX,
              R->MinY,
              R->MaxX - R->MinX + 1,
              R->MaxY - R->MinY + 1,
              0x000,
              0xff,
              NULL);
    /* that's it */
    return;
  }
  
  if (h != LAYERS_NOBACKFILL)
  {
    struct layerhookmsg msg;
    struct Layer *rplayer = rp->Layer;

    msg.l    = L;
    msg.MinX = R->MinX;
    msg.MinY = R->MinY;
    msg.MaxX = R->MaxX;
    msg.MaxY = R->MaxY;
    msg.OffsetX = offsetX;
    msg.OffsetY = offsetY;
    
    /* The hook renders with bitmap coordinates: no layer in its RastPort */
    rp->Layer = NULL;
    (*h->h_Entry)(h, rp, &msg);
    rp->Layer = rplayer;
  }
  
}

/***************************************************************************/
/*                                 LAYER                                   */
/***************************************************************************/

/*
 * Free a layer and all its associated structures
 */
void _FreeLayer(struct Layer * l, struct LayersBase *LayersBase)
{
  struct ClipRect * cr = l->ClipRect, * _cr;
  
  while (cr)
  {
    if (cr->BitMap && !(cr->Flags & CR_SHAREDBITMAP))
      FreeBitMap(cr->BitMap);
    _cr = cr->Next;

    FreePooled(LayersBase->lb_ClipRectPool, cr, sizeof(struct ClipRect));

    cr = _cr;
  }

  /*
   * also free all backed up cliprects.
   */
  cr = l->SuperSaveClipRects;

  while (cr)
  {
    _cr = cr->Next;
    FreePooled(LayersBase->lb_ClipRectPool, cr, sizeof(struct ClipRect));

    cr = _cr;
  }

  if (IL(l)->inputshape)
    DisposeRegion(IL(l)->inputshape);
  DisposeRegion(l->DamageList);
  DisposeRegion(l->VisibleRegion);
  DisposeRegion(l->shape);
  DisposeRegion(l->visibleshape);
  FreeMem(l, sizeof(struct IntLayer));
}

/***************************************************************************/
/*                               LAYERINFO                                 */
/***************************************************************************/

/*
 * Count a user of the Layer_Info (NewLayerInfo(), FattenLayerInfo()).
 */
void _AllocExtLayerInfo(struct Layer_Info * li, struct LayersBase *LayersBase)
{
    ++li->fatten_count;
}

/*
 * The last user is gone: delete the root layer.
 */
void _FreeExtLayerInfo(struct Layer_Info * li, struct LayersBase *LayersBase)
{
    if(--li->fatten_count >= 0)
        return;

    /* Kill Root Layer */
    
    if (li->check_lp)
        DeleteLayer(0UL, li->check_lp);
    li->check_lp = NULL;
}

/***************************************************************************/
/*                                RECTANGLE                                */
/***************************************************************************/

/* The intersection of two rectangles: FALSE when they do not overlap */
BOOL AndRectRect(struct Rectangle *rect1, struct Rectangle *rect2, struct Rectangle *intersect)
{
  if (!DO_OVERLAP(rect1, rect2))
    return FALSE;

  intersect->MinX = (rect1->MinX > rect2->MinX) ? rect1->MinX : rect2->MinX;
  intersect->MinY = (rect1->MinY > rect2->MinY) ? rect1->MinY : rect2->MinY;
  intersect->MaxX = (rect1->MaxX < rect2->MaxX) ? rect1->MaxX : rect2->MaxX;
  intersect->MaxY = (rect1->MaxY < rect2->MaxY) ? rect1->MaxY : rect2->MaxY;
  return TRUE;
}

/* A new region holding one rectangle */
struct Region *_NewRectRegion(WORD MinX, WORD MinY, WORD MaxX, WORD MaxY, struct LayersBase *LayersBase)
{
    struct Region *region = NewRegion();
    
    if (region)
    {
    	struct Rectangle rect = {MinX, MinY, MaxX, MaxY};

    	if (OrRectRegion(region, &rect))
	    return region;

	DisposeRegion(region);
    }

    return NULL;

} /* NewRectRegion */


/***************************************************************************/
/*                            RESOURCE HANDLING                            */
/***************************************************************************/

/*
 * Allocate memory for a ClipRect.
 */

struct ClipRect * _AllocClipRect(struct Layer * L, struct LayersBase *LayersBase)
{
  struct ClipRect * CR;
  
  CR =  L->SuperSaveClipRects;

  if (NULL != CR)
  {
   /* I want to access the list of free ClipRects alone */
    L->SuperSaveClipRects = CR->Next;
    L->SuperSaveClipRectCounter--;
  }
  else
  {
    CR = AllocPooled(LayersBase->lb_ClipRectPool, sizeof(struct ClipRect));
    if (NULL == CR)
      return NULL;
  }

  CR->Flags  = 0;
  CR->Next   = NULL;
  CR->lobs   = NULL;
  CR->BitMap = NULL;
  CR->_p1    = NULL;
  CR->_p2    = NULL;
  return CR;
}

/*
 * Return memory of a ClipRect for later use.
 */

void _FreeClipRect(struct ClipRect   * CR,
                   struct Layer      * L,
                   struct LayersBase * LayersBase)
{
  if (L->SuperSaveClipRectCounter < MAXSUPERSAVECLIPRECTS)
  {
    /* Add the ClipRect to the front of the list */
    CR -> Next = L -> SuperSaveClipRects;
    L -> SuperSaveClipRects = CR;
    L -> SuperSaveClipRectCounter++;
  }
  else
  {
    FreePooled(LayersBase->lb_ClipRectPool, CR, sizeof(struct ClipRect));
  }
}

/*
 * Free a whole list of cliprects including the allocated bitmaps (if any)
 */

void _FreeClipRectListBM(struct Layer * L,
                         struct ClipRect * CR,
                         struct LayersBase *LayersBase)
{
  struct ClipRect * _CR = CR;
  BOOL isSmart;
  if ((L->Flags & (LAYERSUPER|LAYERSMART)) == LAYERSMART)
    isSmart = TRUE;
  else
    isSmart = FALSE;
 
  /*
   * This function is not watching for the upper limit of
   * pre allocated cliprects.
   */
  L->SuperSaveClipRectCounter++;
  
  while (TRUE)
  {
    if (NULL != _CR->BitMap && TRUE == isSmart)
    {
      if (!(_CR->Flags & CR_SHAREDBITMAP))
        FreeBitMap(_CR->BitMap);
      _CR->BitMap = NULL;
    }
    if (NULL != _CR->Next)
    {
      L->SuperSaveClipRectCounter++;
      _CR = _CR->Next;
    }
    else
      break;
  }
  /* _CR is the last ClipRect in the list. I concatenate the
     currently preallocated list of ClipRects with that list. */
  _CR->Next = L->SuperSaveClipRects;
  
  /* CR is the head of the ClipRect list now */
  L->SuperSaveClipRects = CR;
}

/***************************************************************************/
/*                              MISCELLANEOUS                              */
/***************************************************************************/

/*
 * Make a hidden smart refresh ClipRect a view into the bitmap of the
 * ClipRect 'owner' that it lies in: no bitmap of its own, no copying.
 */
static void _MakeClipRectView(struct ClipRect *cr, struct ClipRect *owner)
{
  cr->BitMap = owner->BitMap;
  CR_SETBMOFF(cr, CR_BMOFFX(owner) + cr->bounds.MinX - owner->bounds.MinX,
                  CR_BMOFFY(owner) + cr->bounds.MinY - owner->bounds.MinY);
  cr->Flags |= CR_SHAREDBITMAP;
}

/*
 * Give a hidden ClipRect a (not cleared) bitmap of its own. 'aligndx' is
 * a translation the ClipRect undergoes later (MoveLayer): the content is
 * placed so that it is aligned for the final position.
 */
BOOL _AllocClipRectBitMap(struct ClipRect *cr, struct BitMap *display_bm, int aligndx, struct LayersBase *LayersBase)
{
  cr->BitMap = AllocBitMap(cr->bounds.MaxX - cr->bounds.MinX + 1 + ALIGN_CLIPRECT,
                           cr->bounds.MaxY - cr->bounds.MinY + 1,
                           display_bm->Depth,
                           0,
                           display_bm);
  CR_SETBMOFF(cr, ALIGN_OFFSET(cr->bounds.MinX + aligndx), 0);
  cr->Flags &= ~CR_SHAREDBITMAP;
  return (NULL != cr->BitMap);
}

/*
 * Does the point (screen coordinates) hit the layer? Visible and inside
 * the shape, and inside its input shape if it has one. WhichLayer().
 */
BOOL _IsLayerHitAt(struct Layer *l, LONG x, LONG y, struct LayersBase *LayersBase)
{
  if (!IS_VISIBLE(l) || !IsPointInRegion(l->visibleshape, x, y))
    return FALSE;

  /* An input shape decides alone where the layer takes input (LA_InputShape) */
  if (NULL != IL(l)->inputshape)
    return IsPointInRegion(IL(l)->inputshape, x - l->bounds.MinX, y - l->bounds.MinY);

  return TRUE;
}

/*
 * From region r create a list of ClipRects: visible ones for r itself,
 * hidden ones for the rest of the layer (or of 'inverter' if given).
 * Hidden ClipRects of a smart refresh layer get a bitmap of their own,
 * or, if 'viewsof' is given (a clip region or damage list being derived
 * from the layer's regular ClipRects), become views into the hidden
 * ClipRects of that list, which they lie within.
 */
struct ClipRect * _CreateClipRectsFromRegion(struct Region *r,
                                             struct Layer * l,
                                             int invisible,
                                             struct Region * inverter,
                                             int aligndx,
                                             struct ClipRect * viewsof,
                                             struct LayersBase *LayersBase)
{
  int looped = FALSE;
  struct ClipRect * firstcr = NULL, * cr;
  struct BitMap * display_bm = l->rp->BitMap;

  while (1)
  {
    struct RegionRectangle * rr = r->RegionRectangle;

    while (rr)
    {
      struct Rectangle bounds;

      bounds.MinX = rr->bounds.MinX + r->bounds.MinX;
      bounds.MinY = rr->bounds.MinY + r->bounds.MinY;
      bounds.MaxX = rr->bounds.MaxX + r->bounds.MinX;
      bounds.MaxY = rr->bounds.MaxY + r->bounds.MinY;

      if (TRUE == invisible && IS_SMARTREFRESH(l) && NULL != viewsof)
      {
        /*
         * Views: one per hidden ClipRect of 'viewsof' overlapping this
         * rectangle. Should they not cover it completely (they always
         * do), fall back to a ClipRect with a bitmap of its own.
         */
        struct ClipRect * ocr;
        LONG covered = 0;

        for (ocr = viewsof; NULL != ocr; ocr = ocr->Next)
        {
          struct Rectangle is;

          if (NULL != ocr->lobs && NULL != ocr->BitMap && AndRectRect(&bounds, &ocr->bounds, &is))
            covered += RECTAREA(&is);
        }

        if (covered >= RECTAREA(&bounds))
        {
          for (ocr = viewsof; NULL != ocr; ocr = ocr->Next)
          {
            struct Rectangle is;

            if (NULL == ocr->lobs || NULL == ocr->BitMap || !AndRectRect(&bounds, &ocr->bounds, &is))
              continue;
            cr = _AllocClipRect(l, LayersBase);
            cr->bounds = is;
            cr->lobs   = (struct Layer *)(IPTR)TRUE;
            _MakeClipRectView(cr, ocr);
            cr->Next = firstcr;
            firstcr  = cr;
          }
          rr = rr->Next;
          continue;
        }
      }

      cr = _AllocClipRect(l, LayersBase);
      cr->bounds = bounds;
      cr->lobs   = (struct Layer *)(IPTR)invisible;
      if (TRUE == invisible && IS_SMARTREFRESH(l))
        _AllocClipRectBitMap(cr, display_bm, aligndx, LayersBase);
      cr->Next = firstcr;
      firstcr  = cr;

      rr = rr->Next;
    }

    if (FALSE == looped)
    {
      /*
       * Flip the shape to the opposite part and
       * limit it to its own shape.
       */
      if (inverter)
        XorRegionRegion(inverter, r);
      else
        XorRectRegion(r,&l->bounds);

      AndRectRegion(r,&l->bounds);
      if (TRUE == invisible)
        invisible = FALSE;
      else
        invisible = TRUE;
      looped = TRUE;
    }
    else
      break;
  } /* while (1) */

  return firstcr;
}

/*
 * A (screen relative) rectangle of the layer got exposed in the screen
 * bitmap. A superbitmap layer has no backfill: the area shows the
 * superbitmap (rastport coordinates, shown from Scroll_X/Y). Any other
 * layer gets its backfill hook called.
 */
static void _ExposeRect(struct Layer *l, struct Rectangle *rect, struct LayersBase *LayersBase)
{
  if (IS_SUPERREFRESH(l))
  {
    BltBitMap(l->SuperBitMap,
              rect->MinX - l->bounds.MinX + l->Scroll_X,
              rect->MinY - l->bounds.MinY + l->Scroll_Y,
              l->rp->BitMap,
              rect->MinX,
              rect->MinY,
              rect->MaxX - rect->MinX + 1,
              rect->MaxY - rect->MinY + 1,
              0x0c0,
              0xff,
              NULL);
  }
  else
  {
    _CallLayerHook(l->BackFill,
                   l->rp,
                   l,
                   rect,
                   rect->MinX,
                   rect->MinY,
                   LayersBase);
  }
}

int _CopyClipRectsToClipRects(struct Layer * l,
                              struct ClipRect * oldcr,
                              struct ClipRect * newcr,
                              int destdx,
                              int backupmode,
                              int freelist,
                              int addtodamagelist,
                              int fresh,
                              struct LayersBase *LayersBase)
{
  struct BitMap * display_bm = l->rp->BitMap;

  /*
   * New backing bitmaps are allocated uncleared. Normally the old
   * ClipRects tile the whole layer and fill them completely below. The
   * parts they do not cover (the layer grew while covered) are
   * backfilled into the backing bitmap, as AmigaOS does: the hook is
   * called with the ClipRect's bitmap in the RastPort, the bounds in
   * bitmap coordinates and the offsets in layer coordinates. Only for
   * freshly created lists: when copying back into an existing list
   * (EndUpdate, clip region removal) the destination already holds
   * valid content outside the source.
   */
  if (fresh && IS_SMARTREFRESH(l))
  {
    struct ClipRect * _cr;

    for (_cr = newcr; NULL != _cr; _cr = _cr->Next)
    {
      struct ClipRect * ocr;
      struct Region * uncovered;
      struct RegionRectangle * rr;
      LONG covered = 0, area;

      if (NULL == _cr->lobs || NULL == _cr->BitMap)
        continue;

      area = RECTAREA(&_cr->bounds);
      for (ocr = oldcr; NULL != ocr && covered < area; ocr = ocr->Next)
      {
        struct Rectangle is;

        if (AndRectRect(&_cr->bounds, &ocr->bounds, &is))
          covered += RECTAREA(&is);
      }
      if (covered >= area)
        continue;

      if (l->BackFill == LAYERS_NOBACKFILL || NULL == (uncovered = NewRegion()))
      {
        /* nothing will paint it: at least do not show stale memory */
        BltBitMap(_cr->BitMap, 0, 0, _cr->BitMap, 0, 0,
                  _cr->bounds.MaxX - _cr->bounds.MinX + 1 + ALIGN_CLIPRECT,
                  _cr->bounds.MaxY - _cr->bounds.MinY + 1,
                  0x000, 0xff, NULL);
        continue;
      }

      OrRectRegion(uncovered, &_cr->bounds);
      for (ocr = oldcr; NULL != ocr; ocr = ocr->Next)
      {
        struct Rectangle is;

        if (AndRectRect(&_cr->bounds, &ocr->bounds, &is))
          ClearRectRegion(uncovered, &is);
      }

      for (rr = uncovered->RegionRectangle; NULL != rr; rr = rr->Next)
      {
        struct Rectangle screenrect, bmrect;

        screenrect.MinX = rr->bounds.MinX + uncovered->bounds.MinX;
        screenrect.MinY = rr->bounds.MinY + uncovered->bounds.MinY;
        screenrect.MaxX = rr->bounds.MaxX + uncovered->bounds.MinX;
        screenrect.MaxY = rr->bounds.MaxY + uncovered->bounds.MinY;

        bmrect.MinX = screenrect.MinX - _cr->bounds.MinX + CR_BMOFFX(_cr);
        bmrect.MinY = screenrect.MinY - _cr->bounds.MinY + CR_BMOFFY(_cr);
        bmrect.MaxX = screenrect.MaxX - _cr->bounds.MinX + CR_BMOFFX(_cr);
        bmrect.MaxY = screenrect.MaxY - _cr->bounds.MinY + CR_BMOFFY(_cr);

        {
          struct HookRastPort hrp;

          _CallLayerHook(l->BackFill,
                         _ObtainHookRastPort(&hrp, l->rp, _cr->BitMap, LayersBase),
                         l,
                         &bmrect,
                         screenrect.MinX - l->bounds.MinX + l->Scroll_X,
                         screenrect.MinY - l->bounds.MinY + l->Scroll_Y,
                         LayersBase);
          _ReleaseHookRastPort(&hrp, LayersBase);
        }
      }
      DisposeRegion(uncovered);
    }
  }

  while (NULL != oldcr)
  {
    struct ClipRect * _cr = newcr;
    LONG area = RECTAREA(&oldcr->bounds);
    while ((NULL != _cr) &&  (0 != area) )
    {
      struct Rectangle intersect;
      
      /*
       * Do the two rectangles overlap?
       */
      if (AndRectRect(&_cr->bounds, &oldcr->bounds, &intersect))
      {
        LONG xSize = intersect.MaxX - intersect.MinX + 1;
        LONG ySize = intersect.MaxY - intersect.MinY + 1;
        
        /*
         * Is this new one supposed to be invisible?
         */
        if (NULL != _cr->lobs)
        {
          struct BitMap * srcbm;
          /*
           * The new one is supposed to be invisible.
           * So for SIMPLEREFRESH layers I don't have to
           * do anything if
           * a) not in backupmode or
           * b) old cr was invisible
           */
          if (IS_SIMPLEREFRESH(l) && FALSE == backupmode && NULL == _cr->BitMap)
          {
            if (FALSE == addtodamagelist)
            {
              struct Rectangle rect = intersect;
            
              _TranslateRect(&rect, -l->bounds.MinX, -l->bounds.MinY);

/* FIXME:
 * stegerg: Not sure if this is a good idea. What for example if
 * updating is done in several passes? And CopyClipRectsToClipRects is
 * used by all kinds of functions including BeginUpdate/EndUpdate/InstallClipRegion/etc.
 */
              ClearRectRegion(l->DamageList, &rect);
            }
          }
          else if (IS_SIMPLEREFRESH(l) && TRUE == backupmode && NULL  != oldcr->lobs)
          {
            if (TRUE == addtodamagelist)
            {
              struct Rectangle rect = intersect;
              _TranslateRect(&rect, -l->bounds.MinX, -l->bounds.MinY);
            
              // FIXME (if possible)
              // !!! Also areas where a child disappears beyond the
              // boundaries of its parent are added here!
              OrRectRegion(l->DamageList, &rect);
            } else {
            }
          }
          else
          {
            LONG xSrc, xDest;
            LONG ySrc, yDest;
            struct BitMap * destbm;

            if (IS_SUPERREFRESH(l))
            {
              /*
               * Superbitmap layer: the superbitmap is the only backing
               * store. A part that was already hidden is in it; a part
               * that was on screen is copied to where it belongs. The
               * superbitmap coordinates are the rastport coordinates,
               * and the layer shows the superbitmap from Scroll_X/Y.
               */
              if (NULL == oldcr->lobs)
              {
                BltBitMap(l->rp->BitMap,
                          intersect.MinX,
                          intersect.MinY,
                          l->SuperBitMap,
                          intersect.MinX - l->bounds.MinX + l->Scroll_X,
                          intersect.MinY - l->bounds.MinY + l->Scroll_Y,
                          xSize,
                          ySize,
                          0x0c0,
                          0xff,
                          NULL);
              }
              area -= (xSize * ySize);
            }
            else
            {

              /*
               * Copy the intersection into the new hidden ClipRect's
               * bitmap, from the old ClipRect's bitmap or from the screen.
               */
              if (IS_SIMPLEREFRESH(l) && NULL == _cr->BitMap && TRUE == backupmode)
              {
                /* Get a bitmap (if not there) and make a backup */
                _AllocClipRectBitMap(_cr, display_bm, destdx, LayersBase);
              }
              destbm = _cr->BitMap;
              xDest  = intersect.MinX - _cr->bounds.MinX + CR_BMOFFX(_cr);
              yDest  = intersect.MinY - _cr->bounds.MinY + CR_BMOFFY(_cr);

              if (oldcr->lobs)
              {
                srcbm = oldcr->BitMap;
                xSrc  = intersect.MinX - oldcr->bounds.MinX + CR_BMOFFX(oldcr);
                ySrc  = intersect.MinY - oldcr->bounds.MinY + CR_BMOFFY(oldcr);
              }
              else
              {
                srcbm = l->rp->BitMap;
                xSrc  = intersect.MinX;
                ySrc  = intersect.MinY;
              }

              /* A view of the same content needs no copy */
              if (NULL != srcbm && NULL != destbm &&
                  !(srcbm == destbm && xSrc == xDest && ySrc == yDest))
              {
                BltBitMap(srcbm,
                          xSrc,
                          ySrc,
                          destbm,
                          xDest,
                          yDest,
                          xSize,
                          ySize,
                          0x0c0,
                          0xff,
                          NULL);
              }
              area -= (xSize * ySize);
            }
            
          }
        }
        else //if (FALSE == backupmode)
        {
          /*
           * The new one is visible. if the old one was not visible
           * then I have to show it.
           * If it is a simple refresh layer and it has it
           * backed up (only when moving!) then I must show
           * this. If it has nothing backed up then I must
           * add a part to the damage list.
           */
          if (IS_SIMPLEREFRESH(l) &&
              (NULL != oldcr->lobs) &&
              (NULL == oldcr->BitMap))
          {
            if (NULL != oldcr->lobs && NULL == oldcr->BitMap)
            {
              struct Rectangle rect = intersect;
           
              _CallLayerHook(l->BackFill,
                             l->rp,
                             l,
                             &rect,
                             rect.MinX,
                             rect.MinY,
                             LayersBase);
              _TranslateRect(&rect, -l->bounds.MinX, -l->bounds.MinY);
              OrRectRegion(l->DamageList, &rect);
            }
          }
          else
          {
            if (NULL != oldcr->lobs)
            {
              /*
               * Copy out of hidden bitmap
               */
              LONG xSrc, xDest;
              LONG ySrc, yDest;
              struct BitMap * srcbm;
              
              xDest = intersect.MinX;
              yDest = intersect.MinY;
              if (IS_SUPERREFRESH(l))
              {
                /*
                 * Superbitmap layer: the part is in the superbitmap at
                 * its rastport coordinates; the layer shows the
                 * superbitmap from Scroll_X/Y.
                 */
                srcbm = l->SuperBitMap;
                xSrc  = intersect.MinX - l->bounds.MinX + l->Scroll_X;
                ySrc  = intersect.MinY - l->bounds.MinY + l->Scroll_Y;
              }
              else
              {
                /*
                 * From the hidden bitmap (a moved simple refresh layer
                 * may have one too).
                 */
                srcbm = oldcr->BitMap;
                xSrc  = intersect.MinX - oldcr->bounds.MinX + CR_BMOFFX(oldcr);
                ySrc  = intersect.MinY - oldcr->bounds.MinY + CR_BMOFFY(oldcr);
              }
              

              if (NULL != srcbm)
              BltBitMap(srcbm,
                        xSrc,
                        ySrc,
                        l->rp->BitMap,
                        xDest,
                        yDest,
                        xSize,
                        ySize,
                        0x0c0,
                        0xff,
                        NULL);
              
            } /* if was hidden cliprect */
          } /* if is simple else ... */
        } /* if new cliprect is visible or invisible   */
      } /* if rectangles overlap */
      else
      {

        if (IS_SMARTREFRESH(l) && TRUE == backupmode && NULL == _cr->BitMap && NULL != _cr->lobs)
        {
           _cr->BitMap = AllocBitMap(_cr->bounds.MaxX - _cr->bounds.MinX + 1 + ALIGN_CLIPRECT ,
                                     _cr->bounds.MaxY - _cr->bounds.MinY + 1,
                                     display_bm->Depth,
                                     BMF_CLEAR,
                                     display_bm);
        }
      }
      _cr = _cr->Next;
    } /* all new cliprects */
    
    if (TRUE == freelist)
    {
      _cr = oldcr->Next;
      if (oldcr->BitMap && !(oldcr->Flags & CR_SHAREDBITMAP))
        FreeBitMap(oldcr->BitMap);
      _FreeClipRect(oldcr, l, LayersBase);
      oldcr = _cr;
    }
    else
      oldcr = oldcr->Next;
  } /* for all old cliprects */

  if (IS_EMPTYREGION(l->DamageList))
    l->Flags &= ~LAYERREFRESH;
  else
    l->Flags |=  LAYERREFRESH;

  /*
   * If this is a simple refresh layer and I am not in
   * backup mode and I am not adding to the damagelist
   * then I must call the backfillhook for the
   * area of the damage list of a simple refresh layer
   */

  if (IS_SIMPLEREFRESH(l) &&
      (l->Flags & LAYERREFRESH) &&
      FALSE == backupmode &&
      FALSE == addtodamagelist)
  {
    struct Region * dr = l->DamageList;
    struct RegionRectangle * rr;

    _TranslateRect(&dr->bounds, l->bounds.MinX, l->bounds.MinY);
    AndRectRegion(dr, &l->bounds);
    AndRegionRegion(l->VisibleRegion, dr);
    AndRegionRegion(l->visibleshape, dr);
    
    _TranslateRect(&dr->bounds, -l->bounds.MinX, -l->bounds.MinY);
    
    rr = dr->RegionRectangle;
    while (rr)
    {
      _TranslateRect(&rr->bounds,
                     dr->bounds.MinX + l->bounds.MinX,
                     dr->bounds.MinY + l->bounds.MinY);

      _CallLayerHook(l->BackFill,
                     l->rp,
                     l,
                     &rr->bounds,
                     rr->bounds.MinX,
                     rr->bounds.MinY,
                     LayersBase);

      _TranslateRect(&rr->bounds,
                     -dr->bounds.MinX-l->bounds.MinX,
                     -dr->bounds.MinY-l->bounds.MinY);
      rr = rr->Next;
 
    }
  }

  return TRUE;
}

/*
 * Backup any parts of the layer that overlap with the backup_region
 * and that are not already backed up. Create the cliprects and
 * bitmaps if necessary.
 * Assumption: Only visible parts become invisible,
 *             invisible parts will not become visible.
 *
 * This function MUST not manipulate hide_region!!!!
 */
int _BackupPartsOfLayer(struct Layer * l,
                        struct Region * hide_region,
                        int dx,
                        int backupsimplerefresh,
                        struct LayersBase * LayersBase)
{
  struct ClipRect * newcr;
  struct Region *r, * clipregion;

  /*
   * Uninstall clipping region. This causes all pixels to
   * be copied into the cliprects that cover the complete
   * area of the layer.
   */

  clipregion = _InternalInstallClipRegion(l, NULL, 0, LayersBase);

  ClearRegionRegion(hide_region,l->VisibleRegion);

  r = AndRegionRegionND(l->visibleshape, l->VisibleRegion);
  
  if (r)
  {
     newcr = _CreateClipRectsFromRegion(r,l,FALSE,NULL,dx,NULL,LayersBase);
     DisposeRegion(r);
     if (newcr)
     {
         _CopyClipRectsToClipRects(l,
                                   l->ClipRect /* source */,
                                   newcr  /* destination */,
                                   dx,
                                   backupsimplerefresh,
                                   TRUE,
                                   TRUE,
                                   TRUE,
                                   LayersBase);

          l->ClipRect = newcr;
     }
  }
  
  /*
   * Reinstall the clipping region. This causes the
   * whole visible area of the layer to be copied
   * into the clipping regions cliprects. The
   * regular list of cliprects is still maintained.
   */
  if (clipregion)
    _InternalInstallClipRegion(l, clipregion, dx, LayersBase);

  return TRUE;
}

/*
 * Show any parts of the layer that overlap with the backup_region
 * and that are not already show.
 *
 * This function MUST not manipulate show_region!!!!
 */

int _ShowPartsOfLayer(struct Layer * l,
                      struct Region * show_region,
                      struct LayersBase * LayersBase)
{
  struct ClipRect * newcr;
  struct Region *r;
  struct Region * clipregion;


  /*
   * If there is a clipping region then the whole
   * window is currently backed up in l->ClipRect
   * That covers the complete area. I must first
   * make these visible, move them back to
   * l->_cliprects and recreate the clipping cliprects
   * according to the clipregion
   */

  clipregion = InstallClipRegion(l, NULL);

  OrRegionRegion(show_region,l->VisibleRegion);

  r = AndRegionRegionND(l->visibleshape, l->VisibleRegion);
  if (r != NULL)
  {
      newcr = _CreateClipRectsFromRegion(r,l,FALSE,NULL,0,NULL,LayersBase);
      DisposeRegion(r);
      _CopyClipRectsToClipRects(l,
                                l->ClipRect /* source */,
                                newcr /* destination */,
                                0,
                                FALSE,
                                TRUE,
                                FALSE,
                                TRUE,
                                LayersBase);

      l->ClipRect = newcr;
  }
  if (clipregion)
    InstallClipRegion(l, clipregion);

  return TRUE;
}

/*
 * A layer l (or its new shape) starts hiding 'hide' from the layers behind
 * it: walk from l->back to the first visible parent, back up what they lose
 * (only where 'ovl' overlaps their shape) or just take it out of their
 * VisibleRegion.
 */
void _HideLayersBehind(struct Layer *l, struct Region *hide, struct Rectangle *ovl, struct LayersBase *LayersBase)
{
  struct Layer * lparent = l->parent;
  struct Layer * _l = l->back;

  while (1)
  {
    if (IS_VISIBLE(_l) && DO_OVERLAP(ovl, &_l->shape->bounds))
      _BackupPartsOfLayer(_l, hide, 0, FALSE, LayersBase);
    else
      ClearRegionRegion(hide, _l->VisibleRegion);

    if (_l == lparent)
    {
      if (IS_VISIBLE(_l) || (NULL == lparent->parent))
        break;
      else
        lparent = lparent->parent;
    }
    _l = _l->back;
  }
}

/*
 * A layer l stops hiding things from the layers behind it: walk from
 * l->back to the first visible parent, r being the VisibleRegion handed
 * down. A visible layer whose (visible)shape overlaps ovl1 or ovl2 is shown
 * from it, the others just get it as their VisibleRegion. 'clear' collects
 * what nobody showed. Returns the parent the walk ended at.
 */
struct Layer *_ShowLayersBehind(struct Layer *l, struct Region *r, struct Region *clear,
                                struct Rectangle *ovl1, struct Rectangle *ovl2, int visshape,
                                struct LayersBase *LayersBase)
{
  struct Layer * lparent = l->parent;
  struct Layer * _l = l->back;

  while (1)
  {
    struct Rectangle * lb = visshape ? &_l->visibleshape->bounds : &_l->shape->bounds;

    if (IS_VISIBLE(_l) &&
        (DO_OVERLAP(ovl1, lb) || (NULL != ovl2 && DO_OVERLAP(ovl2, lb))))
    {
      ClearRegion(_l->VisibleRegion);
      _ShowPartsOfLayer(_l, r, LayersBase);
    }
    else
      SetRegion(r, _l->VisibleRegion);

    if (IS_VISIBLE(_l) || IS_ROOTLAYER(_l))
      AndRegionRegion(_l->VisibleRegion, clear);

    if (_l == lparent)
    {
      if (IS_VISIBLE(_l) || (NULL == lparent->parent))
        break;
      else
        lparent = lparent->parent;
    }

    if (IS_VISIBLE(_l))
      ClearRegionRegion(_l->visibleshape, r);

    _l = _l->back;
  }
  return lparent;
}

int _ShowLayer(struct Layer * l, struct LayersBase *LayersBase)
{
  struct Region *r;
  struct RegionRectangle * rr;
  struct ClipRect * prevcr = NULL;
  int invisible = FALSE;

  r = AndRegionRegionND(l->shape, l->VisibleRegion);
  AndRegionRegion(l->parent->shape, r);

  while (1)
  {
    rr = r->RegionRectangle;

    while (NULL != rr)
    {
      struct ClipRect * cr;

      cr = _AllocClipRect(l, LayersBase);


      MinX(cr) = MinX(rr) + MinX(r);
      MinY(cr) = MinY(rr) + MinY(r);
      MaxX(cr) = MaxX(rr) + MinX(r);
      MaxY(cr) = MaxY(rr) + MinY(r);
      cr->lobs = (struct Layer *)(IPTR)invisible;
      if (prevcr)
        prevcr->Next = cr;
      else
        l->ClipRect = cr;

      prevcr = cr;

      if (FALSE == invisible)
      {
        _ExposeRect(l, &cr->bounds, LayersBase);
      }
      else
      {
        /*
         * This part is to be invisible! The backfill hook is run into
         * the backing bitmap, as AmigaOS does (verified on Kickstart
         * 3.1: CreateBehindHookLayer() under another layer calls the
         * hook for the hidden part too).
         */
        if (IS_SMARTREFRESH(l))
        {
          if (_AllocClipRectBitMap(cr, l->rp->BitMap, 0, LayersBase))
          {
            struct Rectangle bmrect;

            bmrect.MinX = CR_BMOFFX(cr);
            bmrect.MinY = CR_BMOFFY(cr);
            bmrect.MaxX = bmrect.MinX + cr->bounds.MaxX - cr->bounds.MinX;
            bmrect.MaxY = bmrect.MinY + cr->bounds.MaxY - cr->bounds.MinY;

            struct HookRastPort hrp;

            _CallLayerHook(l->BackFill,
                           _ObtainHookRastPort(&hrp, l->rp, cr->BitMap, LayersBase),
                           l,
                           &bmrect,
                           cr->bounds.MinX - l->bounds.MinX + l->Scroll_X,
                           cr->bounds.MinY - l->bounds.MinY + l->Scroll_Y,
                           LayersBase);
            _ReleaseHookRastPort(&hrp, LayersBase);
          }
        }
      }

      rr=rr->Next;
    }

    if (FALSE == invisible)
    {
      XorRectRegion(r, &l->bounds);
      invisible = TRUE;
    }
    else
      break;
  }

  DisposeRegion(r);

  return TRUE;
}

/*
 * Limit a (screen relative) region to the layer's shaperegion, if it has
 * one. The shaperegion is layer relative.
 */
void _AndShapeRegion(struct Layer *l, struct Region *r, struct LayersBase *LayersBase)
{
  if (l->shaperegion)
  {
    _TranslateRect(&r->bounds, -l->bounds.MinX, -l->bounds.MinY);
    AndRegionRegion(l->shaperegion, r);
    _TranslateRect(&r->bounds, l->bounds.MinX, l->bounds.MinY);
  }
}

/*
 * It is assumed that the region r is not needed anymore.
 */
void _BackFillRegion(struct Layer * l,
                     struct Region * r,
                     int addtodamagelist,
                     struct LayersBase * LayersBase)
{
  struct RegionRectangle * RR;

  RR = r->RegionRectangle;
  if (NULL == RR) return;
  
  if (IS_SIMPLEREFRESH(l))
  {
    /* Only for simple refresh layers, becuase smart refresh layers
       may have damage outside of visibleshape, like when being dragged
       off screen */
       
    AndRegionRegion(l->visibleshape, r);
  }
  else
  {
    /* Maybe not needed, but to be sure ... */
    
    AndRectRegion(r, &l->bounds);
  }
  
  
  if (TRUE == addtodamagelist)
  {
    l->Flags |= LAYERREFRESH;

     /* Region coords are screen relative, but damagelist coords are layer relative! */
      
    _TranslateRect(&r->bounds, -l->bounds.MinX, -l->bounds.MinY);
    OrRegionRegion(r, l->DamageList);
    _TranslateRect(&r->bounds, l->bounds.MinX, l->bounds.MinY);
    
    
  } /* if (TRUE == addtodamagelist) */

  AndRegionRegion(l->VisibleRegion, r);
  _AndShapeRegion(l, r, LayersBase);
  
  RR  = r->RegionRectangle;
  /* check if a region is empty */
  while (NULL != RR)
  {
     _TranslateRect(&RR->bounds, r->bounds.MinX, r->bounds.MinY);

     _ExposeRect(l, &RR->bounds, LayersBase);
    RR = RR->Next;
  }

}

struct Region *_InternalInstallClipRegion(struct Layer *l, struct Region *region,
                                          WORD destdx,
                                          struct LayersBase *LayersBase)
{
  struct Region * OldRegion;
  BOOL updating = FALSE;
  OldRegion = l->ClipRegion;

  if ((OldRegion != NULL) || (region != NULL))
  {
    if (l->Flags & LAYERUPDATING)
    {
      /* InstallClipRegion does not work if the layer is in update state (BeginUpdate) */

      updating = TRUE;
      EndUpdate(l, FALSE);
      
      OldRegion = l->ClipRegion;
    }

    /* is there a clipregion currently installed? */
    if (NULL != OldRegion)
    {
      /*
       *  Copy the contents of the region cliprects to the regular
       *  cliprects if layer is a SMARTLAYER. Also free the list of
       *  region cliprects.
       */
      if (NULL != l->ClipRect)
      {
        if (IS_SMARTREFRESH(l))
          _CopyClipRectsToClipRects(l,
                                    l->ClipRect,
                                    l->_cliprects,
                                    destdx,
                                    FALSE,
                                    TRUE,
                                    FALSE,
                                    FALSE,
                                    LayersBase);
        else
          _FreeClipRectListBM(l, l->ClipRect, LayersBase);
      }

      /* restore the regular ClipRects */
      l->ClipRect = l->_cliprects;

    }

    /* at this point the regular cliprects are in l->ClipRect in any case !*/

    /* if there's no new region to install then there's not much to do */
    l->ClipRegion = region;

    if (NULL == region)
      l->_cliprects = NULL;
    else
    {
      struct Region *r;

      /* convert the region to a list of ClipRects */
      /* backup the old cliprects */
      l->_cliprects = l->ClipRect;

      _TranslateRect(&region->bounds, l->bounds.MinX, l->bounds.MinY);

      r = AndRegionRegionND(l->VisibleRegion, region);
      AndRegionRegion(l->shape, r);
      
      l->ClipRect = _CreateClipRectsFromRegion(r,
                                               l,
                                               FALSE,
                                               region,
                                               destdx,
                                               l->_cliprects,
                                               LayersBase);
      DisposeRegion(r);

      _CopyClipRectsToClipRects(l,
                                l->_cliprects,
                                l->ClipRect,
                                destdx,
                                FALSE,
                                FALSE,
                                TRUE,
                                TRUE,
                                LayersBase); /* stegerg: should be FALSE. but that does not work??? */

      _TranslateRect(&region->bounds, -l->bounds.MinX, -l->bounds.MinY);

      /* right now I am assuming that everything went alright */
    }

    if (updating)
      BeginUpdate(l);

  } /* if ((OldRegion != NULL) || (region != NULL)) */

  return OldRegion;
}
