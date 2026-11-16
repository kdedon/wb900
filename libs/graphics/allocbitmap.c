/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: Create a new BitMap
*/

#include <exec/memory.h>
#include <utility/tagitem.h>
#include <graphics/modeid.h>
#include <proto/exec.h>

#include "graphics_intern.h"
#include "gfxfuncsupport.h"

/* A bitmap of a display mode has its own pens' pixels, after the struct */
static void ownpixtab(struct GfxBitMap *gbm)
{
    UWORD i;

    gbm->gb_PixTab = (UBYTE *)(gbm + 1);
    for(i = 0; i < 256; i++)
        gbm->gb_PixTab[i] = i & ((1 << gbm->gb_BitMap.Depth) - 1);
}

/*****************************************************************************

    NAME */
#include <graphics/gfx.h>
#include <proto/graphics.h>

        AROS_LH5(struct BitMap *, AllocBitMap,

/*  SYNOPSIS */
        AROS_LHA(ULONG          , sizex, D0),
        AROS_LHA(ULONG          , sizey, D1),
        AROS_LHA(ULONG          , depth, D2),
        AROS_LHA(ULONG          , flags, D3),
        AROS_LHA(struct BitMap *, friend_bitmap, A0),

/*  LOCATION */
        struct GfxBase *, GfxBase, 153, Graphics)

/*  FUNCTION
        Allocates and initializes a bitmap structure. Allocates and
        initializes bitplane data, and sets the bitmap's planes to point to
        it.

    INPUTS
        sizex, sizey - The width and height in pixels

        depth - The depth of the bitmap. A depth of 1 will allocate a
            bitmap for two colors, a depth of 24 will allocate a bitmap for
            16 million colors. Pixels with AT LEAST this many bits will be
            allocated.

        flags - One of these flags:

            BMF_CLEAR: Fill the bitmap with color 0.

            BMF_DISPLAYABLE: to specify that this bitmap data should
                be allocated in such a manner that it can be displayed.
                Displayable data has more severe alignment restrictions
                than non-displayable data in some systems.
                Note that it may be not enough to specify only this flag
                to make the bitmap really displayable. See "INTERNALS"
                section below.

            BMF_INTERLEAVED: tells graphics that you would like your
                bitmap to be allocated with one large chunk of display
                memory for all bitplanes. This minimizes color flashing on
                deep displays. If there is not enough contiguous RAM for an
                interleaved bitmap, graphics.library will fall back to a
                non-interleaved one.

            BMF_MINPLANES: causes graphics to only allocate enough
                space in the bitmap structure for "depth" plane pointers.
                This is for system use and should not be used by
                applications use as it is inefficient, and may waste
                memory.

            BMF_RTGTAGS,
            BMF_RTGCHECK,
            BMF_FRIENDSTAG: Setting these flags to 1's while BMF_SPECIALFMT
                and BMF_INVALID are set to 0 means that friend_bitmap
                points to a taglist instead of BitMap structure.

        friend_bitmap - pointer to another bitmap, or NULL. If this pointer
            is passed, then the bitmap data will be allocated in
            the most efficient form for blitting to friend_bitmap.

            This pointer can also point to a TagList, if specified by flags.
            In this case it may contain the following tags:

              - BMATags_Friend (struct BitMap *)
                    An actual pointer to friend bitmap. Defaults to NULL.

              - BMATags_Depth (ULONG)
                    Depth of the bitmap to create. Defaults to depth argument
                    of AllocBitMap().

              - BMATags_Clear (BOOL)
                    Tells if the newly created bitmap should be explicitly
                    cleared. Defaults to the value of BMF_CLEAR flag in
                    AllocBitMap() arguments.

              - BMATags_NoMemory (BOOL)
                    Tells AllocBitMap() not to allocate actual bitmap storage. Only
                    header is allocated and set up. Default value is FALSE.

              - BMATags_DisplayID (ULONG)
                    A bitmap for this display mode: the display's own when
                    it is free and sizex and sizey fit the mode, else one in
                    RAM of the mode's format, of the mode's size when they
                    fit, so that it can take the display's planes.
              - BMATags_GfxFormat (ULONG)
                    The format (GDF_, <graphics/gfxdriver.h>) when there is no
                    friend. Default GDF_PLANAR.

    RESULT
        A pointer to the new bitmap.

    NOTES
        When allocating using a friend_bitmap bitmap, it is not safe to assume
        anything about the structure of the bitmap data if that friend_bitmap
        BitMap might not be a standard Amiga bitmap (for instance, if the
        workbench is running on a non-Amiga display device, its
        Screen->RastPort->BitMap won't be in standard Amiga format. The
        only safe operations to perform on a non-standard BitMap are:

            blitting it to another bitmap, which must be either a
                standard Amiga bitmap, or a friend_bitmap of this bitmap.

            blitting from this bitmap to a friend_bitmap bitmap or to a
                standard Amiga bitmap.

            attaching it to a rastport and making rendering calls.

        Good arguments to pass for the friend_bitmap are your window's
        RPort->BitMap, and your screen's RastPort->BitMap. Do NOT pass
        &(screenptr->BitMap)!

        BitMaps not allocated with BMF_DISPLAYABLE may not be used as
        Intuition Custom BitMaps or as RasInfo->BitMaps.  They may be
        blitted to a BMF_DISPLAYABLE BitMap, using one of the BltBitMap()
        family of functions.

        When allocating a displayable bitmap, make sure that its size is
        within limits allowed by the display driver. Use GetDisplayInfoData()
        with DTAG_DIMS in order to obtain the needed information.

    EXAMPLE

    BUGS

    SEE ALSO
        FreeBitMap()

    INTERNALS
        A bitmap is planar (up to 8 planes) unless friend_bitmap is chunky
        and depth fits its pixels. A plane over 64 KB (at most 128 KB) is
        one block over two segments; only a bitmap of one plane, or a
        chunky one, may have one. A display's own bitmap comes from its
        driver. BMF_INTERLEAVED is ignored.

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct GfxBitMap *gbm;
    struct BitMap    *nbm;
    struct GfxDriver *drv = &gfx_Planar;
    ULONG             clear = flags & BMF_CLEAR;
    BOOL              alloc = TRUE;
    ULONG             size;
    UWORD             bpr, plane, planes;
    ULONG             format = GDF_PLANAR;
    ULONG             modeid = INVALID_ID;
    BOOL              fits;

    if(BITMAPFLAGS_ARE_EXTENDED(flags)) {
        struct TagItem *tag, *tstate = (struct TagItem *)friend_bitmap;

        friend_bitmap = NULL;

        while((tag = NextTagItem(&tstate))) {
            switch(tag->ti_Tag) {
            case BMATags_Friend:
                friend_bitmap = (struct BitMap *)tag->ti_Data;
                break;

            case BMATags_Depth:
                depth = tag->ti_Data;
                break;

            case BMATags_Clear:
                clear = tag->ti_Data;
                break;

            case BMATags_NoMemory:
                alloc = !tag->ti_Data;
                break;

            case BMATags_GfxFormat:
                format = tag->ti_Data;
                break;

            case BMATags_DisplayID:
                modeid = tag->ti_Data;
                break;
            }
        }
    }

    if(modeid != INVALID_ID) {
        const struct GfxMode *m = gfx_FindMode(modeid, GfxBase);
        struct GfxDriver *gd = gfx_Display(GfxBase);

        if(!m)
            return NULL;
        fits = sizex <= m->gm_Width && sizey <= m->gm_Height;
        gbm = fits ? AllocVec(sizeof(struct GfxBitMap) + 256, MEMF_ANY | MEMF_CLEAR) : NULL;
        ObtainSemaphore(GfxBase->ActiViewCprSemaphore);
        nbm = gbm && gd->gd_AllocDisplay(gd, m, &gbm->gb_BitMap) ? &gbm->gb_BitMap : NULL;
        if(nbm)
            PrivGBase(GfxBase)->vram = nbm;
        ReleaseSemaphore(GfxBase->ActiViewCprSemaphore);
        if(nbm) {
            ownpixtab(gbm);
            if(clear) {
                struct GfxPens pens;
                struct Rectangle r;

                pens.gp_APen = pens.gp_BPen = 0;
                pens.gp_DrMd = JAM2;
                pens.gp_Mask = 0xFF;
                pens.gp_Ptrn = NULL;
                pens.gp_PtrnMask = 0;
                r.MinX = r.MinY = 0;
                r.MaxX = m->gm_Width - 1;
                r.MaxY = m->gm_Height - 1;
                gd->gd_Fill(nbm, &r, &pens);
            }
            return nbm;
        }
        FreeVec(gbm);
        if(fits) {
            sizex = m->gm_Width;
            sizey = m->gm_Height;
        }
        format = m->gm_Format;
        depth = m->gm_Depth;
        friend_bitmap = NULL;
    }

    if(friend_bitmap && IS_GFXBM(friend_bitmap))
        format = GFXBM(friend_bitmap)->gb_Driver->gd_Format;
    if(format == GDF_CHUNKY4 && depth <= 4)
        drv = &gfx_Chunky4;

    if(depth < 1 || depth > 8 || sizex == 0 || sizey == 0)
        return NULL;

    if(drv == &gfx_Chunky4) {
        bpr = ((sizex + 15) >> 4) << 3;
        depth = 4;
        planes = 1;
    } else {
        bpr = ((sizex + 15) >> 4) << 1;
        planes = depth;
    }
    size = (ULONG)bpr * sizey;
    if(size > 0x20000L || (planes > 1 && size > 0x10000L))
        return NULL;

    gbm = AllocVec(sizeof(struct GfxBitMap) + (modeid != INVALID_ID ? 256 : 0),
                   MEMF_ANY | MEMF_CLEAR);
    if(!gbm)
        return NULL;
    nbm = &gbm->gb_BitMap;

    nbm->BytesPerRow = bpr;
    nbm->Rows        = sizey;
    nbm->Flags       = (flags & BMF_DISPLAYABLE) | (drv == &gfx_Planar ? BMF_STANDARD : 0);
    nbm->Depth       = depth;
    nbm->pad         = GFXBM_MAGIC;
    gbm->gb_Driver   = drv;
    gbm->gb_ModeID   = modeid;
    gbm->gb_PlaneSize = size;
    if(modeid != INVALID_ID)
        ownpixtab(gbm);
    /* Friends of the same format share the pens' pixels, of its depth also the mode */
    if(friend_bitmap && IS_GFXBM(friend_bitmap) &&
       GFXBM(friend_bitmap)->gb_Driver->gd_Format == drv->gd_Format) {
        gbm->gb_PixTab = GFXBM(friend_bitmap)->gb_PixTab;
        if(modeid == INVALID_ID && depth == friend_bitmap->Depth)
            gbm->gb_ModeID = GFXBM(friend_bitmap)->gb_ModeID;
    }

    if(alloc) {
        gbm->gb_Flags = GBF_OWNPLANES;
        for(plane = 0; plane < planes; plane++) {
            nbm->Planes[plane] = AllocMem(size, MEMF_ANY | (clear ? MEMF_CLEAR : 0));
            if(!nbm->Planes[plane]) {
                while(plane-- > 0)
                    FreeMem(nbm->Planes[plane], size);
                FreeVec(gbm);
                return NULL;
            }
        }
        /* A block over 64 KB starts a segment and goes on into the next */
        if(size > 0x10000L)
            gbm->gb_Half1 = PtrStep(nbm->Planes[0], 0x10000L);
    }

    return nbm;

    AROS_LIBFUNC_EXIT
} /* AllocBitMap */
