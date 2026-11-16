/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.
*/

#include <aros/libcall.h>
#include <graphics/clip.h>
#include <graphics/layers.h>
#include <utility/tagitem.h>
#include <proto/exec.h>
#include <proto/utility.h>
#include <proto/graphics.h>
#include <proto/layers.h>

#include "basicfuncs.h"
#include "layers_intern.h"

/*
 * The tags CreateLayerTagList() understands. Their values are collected in
 * an array indexed by LATAG_xxx; latags[] maps each index to its tag.
 */
enum
{
  LATAG_BackfillHook, LATAG_SuperBitMap, LATAG_ChildOf, LATAG_InFrontOf, LATAG_Behind,
  LATAG_Hidden, LATAG_ShapeRegion, LATAG_ShapeHook, LATAG_WindowPtr,
  LATAG_InputShape, LATAG_COUNT
};

/*
 * LA_Composited, LA_Opacity and LA_AlphaSurface are ignored: the displays
 * cannot present surfaces, so such a layer is an ordinary one.
 */
static const Tag latags[LATAG_COUNT] =
{
  LA_BackfillHook,
  LA_SuperBitMap,
  LA_ChildOf,
  LA_InFrontOf,
  LA_Behind,
  LA_Hidden,
  LA_ShapeRegion,
  LA_ShapeHook,
  LA_WindowPtr,
  LA_InputShape
};

/*
 * LA_InFrontOf and LA_Behind are intentionally left
 * because they allow to implement AmigaOS4-compatible functionality.
 */
struct Layer *CreateLayerTagList(struct Layer_Info *li, struct BitMap *bm, LONG x0, LONG y0,
                                 LONG x1, LONG y1, LONG flags, int priority, struct TagItem *tagList,
                                 struct LayersBase *LayersBase)
{
  struct BitMap * superbitmap;
  struct Hook * hook, *shapehook;
  APTR win;
  int visible;
  struct Layer * behind, * infrontof, * parent;
  struct Layer * l;
  struct Region * layershape, *shape;
  struct Region * inputshape;
  struct TagItem *tag, *tstate = tagList;
  IPTR tv[LATAG_COUNT];
  int i;

  for (i = 0; i < LATAG_COUNT; i++)
    tv[i] = 0;

  while((tag = NextTagItem(&tstate)))
  {
    for (i = 0; i < LATAG_COUNT; i++)
    {
      if (tag->ti_Tag == latags[i])
      {
        tv[i] = tag->ti_Data;
        break;
      }
    }
  } /* while((tag = NextTagItem(&tstate))) */

  hook         = (struct Hook *)tv[LATAG_BackfillHook];
  superbitmap  = (struct BitMap *)tv[LATAG_SuperBitMap];
  parent       = (struct Layer *)tv[LATAG_ChildOf];
  infrontof    = (struct Layer *)tv[LATAG_InFrontOf];
  behind       = (struct Layer *)tv[LATAG_Behind];
  visible      = !tv[LATAG_Hidden];
  layershape   = (struct Region *)tv[LATAG_ShapeRegion];
  shapehook    = (struct Hook *)tv[LATAG_ShapeHook];
  win          = (APTR)tv[LATAG_WindowPtr];
  inputshape   = (struct Region *)tv[LATAG_InputShape];

  if ((flags & LAYERSUPER) && (NULL == superbitmap))
    return NULL;

  if (!parent)
  {
    parent = li->check_lp;
    if (!parent)
    {
      /* Root layer not yet installed */
      
      if (priority != ROOTPRIORITY) /* avoid endless recursion */
      {
        /*
         * The root layer is what gets backfilled where no other layer
         * is. Its backfill hook is the Layer_Info's BlankHook, which
         * InstallLayerInfoHook() may already have set before any layer
         * existed (Intuition installs SA_BackFill right after InitLayers()).
         */
        struct TagItem tags[3];

        tags[0].ti_Tag  = LA_Hidden;
        tags[0].ti_Data = TRUE;
        tags[1].ti_Tag  = LA_BackfillHook;
        tags[1].ti_Data = (IPTR)li->BlankHook;
        tags[2].ti_Tag  = TAG_DONE;
        tags[2].ti_Data = 0;

        if (!(CreateLayerTagList(li,
                                 bm,
                                 0,
                                 0,
                                 GetBitMapAttr(bm, BMA_WIDTH) - 1,
                                 GetBitMapAttr(bm, BMA_HEIGHT) - 1,
                                 0,
                                 ROOTPRIORITY,
                                 tags,
                                 LayersBase)))
        {
          li->check_lp = NULL;
          return NULL;
        }
        
        parent = li->check_lp;
      }
    }
  }
  
  /*
   * User gives coordinates reltive to parent.
   * Adjust the shape to the absolute coordinates
   * If this is the root layer, I don't have to
   * do anything. I recognize the root layer if there
   * is no parent.
   */

  if (infrontof && infrontof->priority > priority)
    return NULL;
    
  if (behind && behind->priority < priority)
    return NULL;
  
  /* First create the shape region relative to nothing, that is 0,0 origin */

  shape = NewRectRegion(0, 0, x1 - x0, y1 - y0);
  if (!shape)
    return NULL;
  l = NULL;

  if (shapehook)
  {
        struct ShapeHookMsg msg;
        struct Rectangle NewBounds, OldBounds;

        msg.Action    = SHAPEHOOKACTION_CREATELAYER;
        msg.NewShape  = layershape;
        msg.OldShape  = layershape;
        msg.NewBounds = &NewBounds;
        msg.OldBounds = &OldBounds;

        NewBounds.MinX = x0;
        NewBounds.MinY = y0;
        NewBounds.MaxX = x1;
        NewBounds.MaxY = y1;
        OldBounds.MinX = 0;
        OldBounds.MinY = 0;
        OldBounds.MaxX = 0;
        OldBounds.MaxY = 0;

        if (CallHookPkt(shapehook, NULL, &msg))
            layershape = msg.NewShape;
  }

  if (layershape) if (!AndRegionRegion(layershape, shape))
  {
    DisposeRegion(shape);
    return NULL;
  }

  if (parent)
  {
    /* Convert from parent layer relative coords to screen relative coords */
    
    x0 += parent->bounds.MinX;
    y0 += parent->bounds.MinY;
    x1 += parent->bounds.MinX;
    y1 += parent->bounds.MinY;
  }
  
  /* Make shape region relative to screen */
  _TranslateRect(&shape->bounds, x0, y0);
  
  l  = AllocMem(sizeof(struct IntLayer), MEMF_CLEAR|MEMF_PUBLIC);
  if (l)
  {
    IL(l)->shapehook = shapehook;
    l->Window = win;
    l->shaperegion = layershape;
    l->rp = &IL(l)->rp;

    InitRastPort(l->rp);
    l->rp->Layer = l;
    l->rp->BitMap = bm;

    l->Flags = (WORD) flags;
    l->LayerInfo = li;
    
    l->SuperBitMap = superbitmap;
    l->BackFill = hook;
    l->priority = priority;
    
    InitSemaphore(&l->Lock);

    l->shape = shape;
    l->bounds.MinX = x0;
    l->bounds.MaxX = x1;
    l->bounds.MinY = y0;
    l->bounds.MaxY = y1;
    l->Width  = x1 - x0 + 1;
    l->Height = y1 - y0 + 1;
    if (inputshape)
    {
      /* A copy: the caller's region may go away with the call */
      if (NULL == (IL(l)->inputshape = NewRegion()) ||
          !OrRegionRegion(inputshape, IL(l)->inputshape))
        goto failexit;
    }
    LockLayer(0, l);
    
    if (NULL == (l->DamageList = NewRegion()))
      goto failexit;
    if (NULL == (l->VisibleRegion = NewRegion()))
      goto failexit;
    if (NULL == (l->visibleshape = NewRegion()))
      goto failexit;
    
    l->visible = visible;

    if (NULL == li->check_lp)
      li->check_lp = l;

    l->parent = parent;

    if (parent)
      l->nesting = parent->nesting+1;
    else
      l->nesting = 0;

    LockLayers(li);

    
    /*
     * If neither a layer in front or behind is
     * given the search for the place according to
     * the priority and insert it there. I
     * determine behind or infrontof here!
     */
    if (NULL != li->top_layer)
    {
      if (!infrontof && !behind)
      {
        int end = FALSE;
        struct Layer * lastgood;
        infrontof = parent;
 
        do
        {
          lastgood = infrontof;
          infrontof = infrontof->front;
          if (NULL == infrontof)
          {
            infrontof = lastgood;
            break;
          }
          if (infrontof->parent != l->parent)
          {
            infrontof = lastgood;
            break;
          }
          while (infrontof->nesting >= l->nesting)
          {
            lastgood = infrontof;
            infrontof = infrontof->front;
            if (NULL == infrontof ||
                infrontof->priority > l->priority)
            {
              infrontof = lastgood;
              end = TRUE;
              break;
            }
          }
          
          
        }
        while (FALSE == end);
      
      }
    }

    if (infrontof || (NULL == behind))
    {
      if (NULL == infrontof)
        infrontof = li->top_layer;

      if (li->top_layer == infrontof)
      {
        li->top_layer = l;
        l->front  = NULL;
        l->back   = infrontof;
        if (NULL != infrontof)
          infrontof->front = l;
      }
      else
      {
        l->front = infrontof->front;
        l->back  = infrontof;
        infrontof->front->back = l;
        infrontof->front = l;
      }
    }
    else if (behind)
    {
      l->front = behind;
      l->back  = behind->back;
      if (l->back)
        l->back->front = l;
      behind->back = l;
    }

    
    
    SetRegion(l->shape, l->visibleshape);
    if (l->parent)
      AndRegionRegion(l->parent->shape, l->visibleshape);

    if (IS_VISIBLE(l))
    {
      /*
       * layer is to be visible
       */

      /*
       * Does this layer have a layer in front of it?
       * If yes, then take that layer's VisibleRegion and
       * cut out that layer's shape. This is then the
       * VisibleRegion of my layer.
       */
      if (l->front)
      {
        SetRegion(l->front->VisibleRegion, l->VisibleRegion);
        ClearRegionRegion(l->front->visibleshape, l->VisibleRegion);
      }
      else
        SetRegion(li->check_lp->shape, l->VisibleRegion);
     
     
      /*
       * First tell all layers behind this layer to
       * back up their parts that the new layer will
       * be hiding.
       */
      _HideLayersBehind(l, l->visibleshape, &l->shape->bounds, LayersBase);

    }
    /*
     * Show the layer according to its visible area
     * This function creates the cliprects in the area
     * of the layer.
     * This also works for invisible layers since their Visible
     * Region is non existent.
     */
    if (!IS_ROOTLAYER(l))
      _ShowLayer(l, LayersBase);

  }
  else
    goto failexit;

  UnlockLayers(li);
  
  return l;
  
failexit:
  DisposeRegion(shape);
  if (l)
  {
    if (IL(l)->inputshape)
      DisposeRegion(IL(l)->inputshape);
    if (l->VisibleRegion)
      DisposeRegion(l->VisibleRegion);
    if (l->DamageList)
      DisposeRegion(l->DamageList);
    if (l->visibleshape)
      DisposeRegion(l->visibleshape);
    FreeMem(l, sizeof(struct IntLayer));
  }

  return NULL;
} /* CreateBehindHookLayer */
