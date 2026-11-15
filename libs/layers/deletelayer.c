/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc:
*/

#include <aros/libcall.h>
#include <exec/types.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <graphics/layers.h>
#include <graphics/regions.h>
#include "layers_intern.h"
#include "basicfuncs.h"

/*****************************************************************************

    NAME */
#include <proto/layers.h>

        AROS_LH2(LONG, DeleteLayer,

/*  SYNOPSIS */
        AROS_LHA(LONG          , dummy, A0),
        AROS_LHA(struct Layer *, l    , A1),

/*  LOCATION */
        struct LayersBase *, LayersBase, 15, Layers)

/*  FUNCTION
        Deletes the layer. Other layers that were hidden (partially)
        will become visible. If parts of a simple layer become
        visible those parts are added to the damagelist of the
        layer and the LAYERREFRESH flags is set.

    INPUTS
        dummy - nothing special
        LD    - layer to be deleted

    RESULT
        TRUE  - layer was successfully deleted
        FALSE - layer could not be delete (out of memory)

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

    HISTORY
        27-11-96    digulla automatically created from
                            layers_lib.fd and clib/layers_protos.h

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  struct Layer * lparent;
  /*
   * all children must have been destroyed before.
   */
  LockLayers(l->LayerInfo);
  
  if (l != GetFirstFamilyMember(l))
  {
    UnlockLayers(l->LayerInfo);
    return FALSE;
  }
    
  if (IS_VISIBLE(l))
  {
    struct Region r;
    InitRegion(&r);

    SetRegion(l->VisibleRegion, &r);
    lparent = _ShowLayersBehind(l, &r, l->shape, &r.bounds, NULL, FALSE, LayersBase);

    ClearRegion(&r);

    if (!IS_EMPTYREGION(l->shape))
    {
      if (lparent && IS_ROOTLAYER(lparent))
        _BackFillRegion(lparent, l->shape, FALSE, LayersBase);
    }
  }
  
  
  /*
   * Take the layer out of the list
   */
  if (l->front)
    l->front->back = l->back;
  else
    l->LayerInfo->top_layer = l->back;
  
  if (l->back)
    l->back->front = l->front;

  
  UnlockLayers(l->LayerInfo);

  _FreeLayer(l, LayersBase);

  return TRUE;
  
  AROS_LIBFUNC_EXIT
} /* DeleteLayer */
