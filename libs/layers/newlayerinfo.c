/*
    Copyright (C) 1995-2007, The AROS Development Team. All rights reserved.

    Desc:
*/
#include <aros/libcall.h>
#include <exec/memory.h>
#include <proto/exec.h>

/*****************************************************************************

    NAME */
#include <proto/layers.h>
#include "layers_intern.h"
#include "basicfuncs.h"

        AROS_LH0(struct Layer_Info *, NewLayerInfo,

/*  SYNOPSIS */
        /* void */

/*  LOCATION */
        struct LayersBase *, LayersBase, 24, Layers)

/*  FUNCTION

    INPUTS

    RESULT

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

    struct Layer_Info *li;

    if(!(li = (struct Layer_Info *)AllocMem(sizeof(struct Layer_Info), MEMF_PUBLIC)))
        return NULL;

    InitLayers(li);

    _AllocExtLayerInfo(li, LayersBase);

    li->Flags |= NEWLAYERINFO_CALLED;

    return li;

    AROS_LIBFUNC_EXIT
} /* NewLayerInfo */
