/*
    Copyright (C) 1995-2007, The AROS Development Team. All rights reserved.

    Desc:
*/
#include <aros/libcall.h>

/*****************************************************************************

    NAME */
#include <proto/layers.h>
#include "layers_intern.h"
#include "basicfuncs.h"

        AROS_LH1(LONG, FattenLayerInfo,

/*  SYNOPSIS */
        AROS_LHA(struct Layer_Info *, li, A0),

/*  LOCATION */
        struct LayersBase *, LayersBase, 26, Layers)

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

    _AllocExtLayerInfo(li, LayersBase);

    li->Flags |= NEWLAYERINFO_CALLED;

    return TRUE;

    AROS_LIBFUNC_EXIT
} /* FattenLayerInfo */
