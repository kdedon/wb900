/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: AROS Graphics function DeinitRastPort()
*/

#include <graphics/rastport.h>
#include <proto/exec.h>

#include "graphics_intern.h"

/*
 * RP_Extra is graphics.library's private data of a RastPort; its first
 * field is the RastPort it belongs to. A copy of a RastPort (CloneRastPort(),
 * or one copied by hand) shares the pointer but not the data.
 */
#define RP_EXTRA_OWNED(rp)  ((rp)->RP_Extra && (*(struct RastPort **)(rp)->RP_Extra == (rp)))

/*****************************************************************************

    NAME */
#include <proto/graphics.h>

        AROS_LH1(void, DeinitRastPort,

/*  SYNOPSIS */
        AROS_LHA(struct RastPort *, rp, A1),

/*  LOCATION */
        struct GfxBase *, GfxBase, 178, Graphics)

/*  FUNCTION

        Frees the extra contents associated with a RastPort structure.
        The structure itself is not freed.

    INPUTS

        rp - The RastPort which contents are to be freed.

    RESULT

        None.

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO

        graphics.library/InitRastPort()

    INTERNALS

    HISTORY
        29-10-95    digulla automatically created from
                            graphics_lib.fd and clib/graphics_protos.h

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    if (RP_EXTRA_OWNED(rp))
        FreeVec(rp->RP_Extra);
    rp->RP_Extra = NULL;

    AROS_LIBFUNC_EXIT
} /* DeinitRastPort */
