/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: Internal information for layers.library.
*/
#ifndef _LAYERS_INTERN_H_
#define _LAYERS_INTERN_H_

#include <exec/types.h>
#include <exec/lists.h>
#include <exec/libraries.h>
#include <exec/memory.h>
#include <aros/libcall.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <graphics/rastport.h>
#include <graphics/clip.h>
#include <graphics/layers.h>
#include <utility/utility.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/utility.h>
#include <proto/layers.h>

#include "intregions.h"

struct LayersBase
{
    struct Library   	    lb_LibNode;

    APTR    	    	    lb_ClipRectPool;

    struct GfxBase *        lb_GfxBase;
    struct UtilityBase *    lb_UtilityBase;
    APTR                    lb_SegList;
};

/* Store library bases in our base, not in .bss. Why not ? */
#define GfxBase     (LayersBase->lb_GfxBase)
#define UtilityBase (LayersBase->lb_UtilityBase)

struct IntLayer
{
    struct Layer    lay;
    struct Hook	   *shapehook;
    ULONG	    intflags;
    struct RastPort rp;		/* lay.rp */

    /* LA_InputShape: own copy, layer relative; NULL: input where it is seen */
    struct Region  *inputshape;
};

#define IL(x) ((struct IntLayer *)(x))

/* Standard layer priorities */
#define ROOTPRIORITY		0
#define BACKDROPPRIORITY	10
#define UPFRONTPRIORITY		20

#define INTFLAG_AVOID_BACKFILL 1

/*
** The smart refresh flag is set for super bitmap as well as smart refresh
** layers 
*/
#define IS_SIMPLEREFRESH(l) (0 != ((l)->Flags & LAYERSIMPLE))
#define IS_SMARTREFRESH(l)  (LAYERSMART == ((l)->Flags & (LAYERSMART|LAYERSUPER)))
#define IS_SUPERREFRESH(l)  (0 != ((l)->Flags & LAYERSUPER))

int _MoveLayerBehind(struct Layer *l,
                     struct Layer *lfront,
                     struct LayersBase *LayersBase);
int _MoveLayerToFront(struct Layer * l,
                      struct Layer * lbehind,
                      struct LayersBase *LayersBase);

struct Region *_NewRectRegion(WORD MinX, WORD MinY, WORD MaxX, WORD MaxY, struct LayersBase *LayersBase);
#define NewRectRegion(MinX, MinY, MaxX, MaxY) _NewRectRegion(MinX, MinY, MaxX, MaxY, LayersBase)

#endif /* _LAYERS_INTERN_H */
