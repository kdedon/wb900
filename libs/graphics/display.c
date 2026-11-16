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
#include <graphics/view.h>
#include <proto/exec.h>
#include <resources/framebuf.h>

#include "graphics_intern.h"
#include "gfxfuncsupport.h"
#include "gfxformat.h"

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
 * Under ActiViewCprSemaphore: bm, a RAM bitmap of the format and size of
 * the bitmap with the display's planes, trades planes with it, each
 * keeping its pixels.  The sprite's lock keeps drawing out of both.
 */
static void gfx_Trade(struct BitMap *bm, struct GfxBase *GfxBase)
{
    struct GfxBase_intern *gb = PrivGBase(GfxBase);
    struct GfxBitMap *d = GFXBM(gb->vram), *r = GFXBM(bm);
    struct GfxDriver *gd = d->gb_Driver;
    ULONG n = (ULONG)bm->BytesPerRow * bm->Rows;
    UWORD p, planes = gd->gd_Format == GDF_PLANAR ? bm->Depth : 1;
    PLANEPTR t;
    ULONG size;
    UWORD f;

    ObtainSemaphore(&gb->sprite.ss_Lock);
    if (gd->gd_Adopt)
        gd->gd_Adopt(gd, bm);
    else {
        for (p = 0; p < planes; p++) {
            gfx_WSwap((UWORD *)d->gb_BitMap.Planes[p], (UWORD *)bm->Planes[p],
                (UWORD)((n < 0x10000L ? n : 0x10000L) >> 1));
            t = d->gb_BitMap.Planes[p];
            d->gb_BitMap.Planes[p] = bm->Planes[p];
            bm->Planes[p] = t;
        }
        if (n > 0x10000L)
            gfx_WSwap((UWORD *)d->gb_Half1, (UWORD *)r->gb_Half1,
                (UWORD)((n - 0x10000L) >> 1));
        t = d->gb_Half1;
        d->gb_Half1 = r->gb_Half1;
        r->gb_Half1 = t;
        size = d->gb_PlaneSize;
        d->gb_PlaneSize = r->gb_PlaneSize;
        r->gb_PlaneSize = size;
    }
    r->gb_Driver = gd;
    d->gb_Driver = formats[gd->gd_Format - 1];
    f = d->gb_Flags;
    d->gb_Flags = r->gb_Flags;
    r->gb_Flags = f;
    gb->vram = bm;
    ReleaseSemaphore(&gb->sprite.ss_Lock);
}

/* vp's colours to a programmable palette, now showing its bitmap */
static void gfx_LoadColours(struct ViewPort *vp, struct GfxBase *GfxBase)
{
    struct ColorMap *cm = vp->ColorMap;
    UWORD i, n = 1 << vp->RasInfo->BitMap->Depth;
    ULONG c[3];
    UBYTE rgb[3];

    if (!cm || !PrivGBase(GfxBase)->display->gd_SetColor)
        return;
    if (n > cm->Count)
        n = cm->Count;
    for (i = 0; i < n; i++) {
        GetRGB32(cm, i, 1, c);
        rgb[0] = c[0] >> 24;
        rgb[1] = c[1] >> 24;
        rgb[2] = c[2] >> 24;
        internal_SetRGB32Colors(vp, rgb, i, 1, GfxBase);
    }
}

/* A RAM bitmap of the size of vram, in its mode or with the display's planes */
static struct BitMap *gfx_Spare(struct BitMap *vram, ULONG id, ULONG flags,
    struct GfxBase *GfxBase)
{
    struct TagItem t[2];

    t[0].ti_Tag = BMATags_DisplayID;
    t[0].ti_Data = id;
    t[1].ti_Tag = TAG_DONE;
    return AllocBitMap(GetBitMapAttr(vram, BMA_WIDTH), vram->Rows,
        vram->Depth, flags | BMF_DISPLAYABLE | BMF_CHECKVALUE, (struct BitMap *)t);
}

/*
 * Under ActiViewCprSemaphore: the display bitmap that shows vp, or NULL.
 * There is no display start to move, so the display's planes go to the
 * bitmap shown: vp's bitmap when it is a display's or one in RAM of its
 * mode and size, which trades planes with the one that has them.
 * Another bitmap in RAM of the display's format and depth is copied in
 * from vp's offsets, the planes' bitmap moved to RAM first.
 */
struct BitMap *gfx_ViewPortDisplay(struct ViewPort *vp, struct GfxBase *GfxBase)
{
    struct GfxBase_intern *gb = PrivGBase(GfxBase);
    struct BitMap *bm = vp->RasInfo ? vp->RasInfo->BitMap : NULL;
    struct BitMap *dbm, *s;
    WORD x, y, w, h;

    if (!bm || !IS_GFXBM(bm))
        return NULL;
    if (GFXBM(bm)->gb_Flags & GBF_DISPLAY)
        return bm;
    x = vp->RasInfo->RxOffset;
    y = vp->RasInfo->RyOffset;
    /* No display bitmap: one of graphics' own takes the display */
    if (!gb->vram && GFXBM(bm)->gb_ModeID != INVALID_ID) {
        gb->spare = gfx_Spare(bm, GFXBM(bm)->gb_ModeID, 0, GfxBase);
        if (gb->spare && !(GFXBM(gb->spare)->gb_Flags & GBF_DISPLAY)) {
            FreeBitMap(gb->spare);
            gb->spare = NULL;
        }
    }
    dbm = gb->vram;
    if (!dbm)
        return NULL;
    if ((GFXBM(bm)->gb_Flags & GBF_OWNPLANES) && x == 0 && y == 0 &&
        GFXBM(bm)->gb_ModeID == GFXBM(dbm)->gb_ModeID &&
        bm->BytesPerRow == dbm->BytesPerRow && bm->Rows == dbm->Rows &&
        bm->Depth == dbm->Depth) {
        gfx_Trade(bm, GfxBase);
        /* graphics' own now has bm's old planes */
        if (dbm == gb->spare) {
            FreeBitMap(dbm);
            gb->spare = NULL;
        }
        gfx_LoadColours(vp, GfxBase);
        return bm;
    }
    if (GFXBM(bm)->gb_ModeID == INVALID_ID ?
        GFXBM(bm)->gb_Driver->gd_Format != GFXBM(dbm)->gb_Driver->gd_Format ||
        bm->Depth != dbm->Depth :
        GFXBM(bm)->gb_ModeID != GFXBM(dbm)->gb_ModeID)
        return NULL;
    /* the display around the copy is cleared */
    if (dbm != gb->spare &&
        (s = gfx_Spare(dbm, GFXBM(dbm)->gb_ModeID, BMF_CLEAR, GfxBase))) {
        gfx_Trade(s, GfxBase);
        gb->spare = dbm = s;
    }
    /* graphics' own shows the pixels, not the pens */
    if (dbm == gb->spare)
        GFXBM(dbm)->gb_PixTab = GFXBM(bm)->gb_PixTab;
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
