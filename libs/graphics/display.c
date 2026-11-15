/* SPDX-License-Identifier: MIT */
/*
 * The display: the driver of the console's card, which graphics opens
 * from LIBS: the first time it is needed and whose modes are the display
 * database; what a ViewPort shows on it; and the 100 Hz tick WaitTOF()
 * waits for, as there is no vertical blank.
 */
#include <exec/memory.h>
#include <exec/tasks.h>
#include <graphics/modeid.h>
#include <proto/exec.h>
#include <resources/framebuf.h>

#include "graphics_intern.h"
#include "gfxfuncsupport.h"

static struct GfxDriver *formats[GDF_COUNT] = { &gfx_Planar, &gfx_Chunky4 };

struct GfxDriver *gfx_Display(struct GfxBase *GfxBase)
{
    struct GfxBase_intern *gb = PrivGBase(GfxBase);
    APTR FrameBufBase;

    ObtainSemaphore(&gb->view_sema);
    if (!gb->probed)
    {
        gb->probed = TRUE;
        FrameBufBase = OpenResource(FRAMEBUFNAME);
        if (FrameBufBase && GetFBInfo()->fbi_Card == FBC_HR)
            gb->drvlib = OpenLibrary("hr.gfxdriver", 0);
        else if (FrameBufBase && GetFBInfo()->fbi_Card == FBC_LC)
            gb->drvlib = OpenLibrary("lc.gfxdriver", 0);
        if (gb->drvlib)
        {
            gb->display = GFX_GETDRIVER(gb->drvlib, formats);
            if (!gb->display)
            {
                CloseLibrary(gb->drvlib);
                gb->drvlib = NULL;
            }
        }
    }
    ReleaseSemaphore(&gb->view_sema);
    return gb->display;
}

/* The display mode of a ModeID, or NULL */
const struct GfxMode *gfx_FindMode(ULONG id, struct GfxBase *GfxBase)
{
    struct GfxDriver *gd = gfx_Display(GfxBase);
    UWORD i;

    if (gd && id != INVALID_ID)
        for (i = 0; i < gd->gd_NumModes; i++)
            if (gd->gd_Modes[i].gm_ModeID == id)
                return &gd->gd_Modes[i];
    return NULL;
}

/* Every tick wakes the tasks in WaitTOF() */
int gfx_TOFServer(struct GfxBase *GfxBase, UWORD intnum)
{
    struct Node *n;

    for (n = GfxBase->TOF_WaitQ.lh_Head; n->ln_Succ; n = n->ln_Succ)
        Signal((struct Task *)n->ln_Name, SIGF_SINGLE);
    return 0;
}

/*
 * Under ActiViewCprSemaphore: the display bitmap that shows vp, or NULL.
 * That is vp's own bitmap when it is a display's; a bitmap in RAM of the
 * mode of the display bitmap shown is copied into it from vp's offsets,
 * as there is no display start to move.
 */
struct BitMap *gfx_ViewPortDisplay(struct ViewPort *vp, struct GfxBase *GfxBase)
{
    struct BitMap *bm = vp->RasInfo ? vp->RasInfo->BitMap : NULL;
    struct BitMap *dbm = PrivGBase(GfxBase)->shown;
    WORD x, y, w, h;

    if (!bm || !IS_GFXBM(bm))
        return NULL;
    if (GFXBM(bm)->gb_Flags & GBF_DISPLAY)
        return bm;
    if (!dbm || GFXBM(bm)->gb_ModeID == INVALID_ID ||
        GFXBM(bm)->gb_ModeID != GFXBM(dbm)->gb_ModeID)
        return NULL;
    x = vp->RasInfo->RxOffset;
    y = vp->RasInfo->RyOffset;
    w = MIN(GetBitMapAttr(dbm, BMA_WIDTH), GetBitMapAttr(bm, BMA_WIDTH) - x);
    h = MIN(dbm->Rows, bm->Rows - y);
    if (x >= 0 && y >= 0 && w > 0 && h > 0)
        BltBitMap(bm, x, y, dbm, 0, 0, w, h, 0xC0, 0xFF, NULL);
    return dbm;
}

/* Whether LoadView() of the View on display showed vp */
BOOL gfx_ViewPortShown(struct ViewPort *vp, struct GfxBase *GfxBase)
{
    struct ViewPort *v;

    if (!GfxBase->ActiView)
        return FALSE;
    for (v = GfxBase->ActiView->ViewPort; v; v = v->Next)
        if (!(v->Modes & VP_HIDE))
            break;
    return v == vp;
}
