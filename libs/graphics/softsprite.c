/* SPDX-License-Identifier: MIT */
/*
 * The pointer: a software sprite drawn into the ViewPort's bitmap, with
 * what it covers saved in RAM.  The task that moves it (input.device's)
 * and every drawing into that bitmap take gfx_SpriteLock, and drawing
 * hides the sprite while it covers the area drawn.
 *
 * Sprite pixels are 2 bits (Amiga sprite pens 1-3, 0 transparent) and go
 * to the bitmap as pens 17-19, through its pixel table.
 */
#include <exec/memory.h>
#include <graphics/sprite.h>
#include <graphics/view.h>
#include <proto/exec.h>

#include "graphics_intern.h"
#include "gfxfuncsupport.h"
#include "intregions.h"

#define SPRITEMAXW	32

static void sprite_rect(struct SoftSprite *ss, struct Rectangle *r,
                        struct GfxBase *GfxBase)
{
    struct BitMap *img = ss->ss_Sprite->es_BitMap;

    r->MinX = ss->ss_X;
    r->MinY = ss->ss_Y;
    r->MaxX = ss->ss_X + GetBitMapAttr(img, BMA_WIDTH) - 1;
    r->MaxY = ss->ss_Y + img->Rows - 1;
}

/* The part of the sprite inside its bitmap, or FALSE */
static BOOL sprite_clip(struct SoftSprite *ss, struct Rectangle *r,
                        struct GfxBase *GfxBase)
{
    struct Rectangle bm;

    sprite_rect(ss, r, GfxBase);
    GetRPClipRectangleForBitMap(ss->ss_BitMap, &bm, GfxBase);
    if (r->MinX < bm.MinX) r->MinX = bm.MinX;
    if (r->MinY < bm.MinY) r->MinY = bm.MinY;
    if (r->MaxX > bm.MaxX) r->MaxX = bm.MaxX;
    if (r->MaxY > bm.MaxY) r->MaxY = bm.MaxY;
    return r->MinX <= r->MaxX && r->MinY <= r->MaxY;
}

/* Put the sprite in, saving what it covers */
static void sprite_show(struct SoftSprite *ss, struct GfxBase *GfxBase)
{
    struct BitMap *bm = ss->ss_BitMap;
    struct GfxDriver *gd = GFX_DRIVER(bm);
    struct BitMap *img = ss->ss_Sprite->es_BitMap;
    UWORD mod = img->BytesPerRow, h = img->Rows;
    struct GfxPens p;
    struct Rectangle r, sr;
    WORD dx, dy, k;

    if (!sprite_clip(ss, &r, GfxBase))
        return;
    dx = r.MinX - ss->ss_X;
    dy = r.MinY - ss->ss_Y;
    sr.MinX = dx;
    sr.MinY = dy;
    sr.MaxX = r.MaxX - ss->ss_X;
    sr.MaxY = r.MaxY - ss->ss_Y;
    gd->gd_Copy(bm, r.MinX, r.MinY, ss->ss_Save, &sr, 0xC0, 0xFF);
    p.gp_DrMd = JAM1;
    p.gp_Mask = 0xFF;
    p.gp_Ptrn = NULL;
    for (k = 1; k < 4; k++) {
        p.gp_APen = BM_PIXEL(bm, 16 + k);
        gd->gd_Template(bm, &r, ss->ss_Tmpl + (ULONG)((k - 1) * h + dy) * mod,
            dx, mod, &p);
    }
    ss->ss_Shown = TRUE;
}

/* Take the sprite out, putting back what it covered */
static void sprite_hide(struct SoftSprite *ss, struct GfxBase *GfxBase)
{
    struct BitMap *bm = ss->ss_BitMap;
    struct Rectangle r;

    ss->ss_Shown = FALSE;
    if (!sprite_clip(ss, &r, GfxBase))
        return;
    GFX_DRIVER(bm)->gd_Copy(ss->ss_Save, r.MinX - ss->ss_X,
        r.MinY - ss->ss_Y, bm, &r, 0xC0, 0xFF);
}

/*
 * The templates of sprite pens 1 to 3 of img, one after another, rows of
 * img's BytesPerRow
 */
static UBYTE *sprite_tmpl(struct BitMap *img, ULONG *size)
{
    UWORD bpr = img->BytesPerRow, h = img->Rows, y, i;
    UBYTE *t, *a, *b, *c, *p0, *p1;

    *size = (ULONG)bpr * h * 3;
    t = AllocMem(*size, MEMF_ANY);
    if (!t)
        return NULL;
    a = t;
    b = t + bpr * h;
    c = b + bpr * h;
    p0 = img->Planes[0];
    p1 = img->Planes[1];
    for (y = 0; y < h; y++)
        for (i = 0; i < bpr; i++, p0++, p1++) {
            *a++ = *p0 & ~*p1;
            *b++ = ~*p0 & *p1;
            *c++ = *p0 & *p1;
        }
    return t;
}

/*
 * Around drawing into bm: lock the sprite, and hide it if it covers r.
 * Returns whether to call gfx_SpriteAfter().
 */
BOOL gfx_SpriteBefore(struct BitMap *bm, struct Rectangle *r, struct GfxBase *GfxBase)
{
    struct SoftSprite *ss = &PrivGBase(GfxBase)->sprite;
    struct Rectangle sr;

    if (bm != ss->ss_BitMap)
        return FALSE;
    ObtainSemaphore(&ss->ss_Lock);
    if (ss->ss_Shown) {
        sprite_rect(ss, &sr, GfxBase);
        if (_AndRectRect(&sr, r, &sr))
            sprite_hide(ss, GfxBase);
    }
    return TRUE;
}

void gfx_SpriteAfter(struct GfxBase *GfxBase)
{
    struct SoftSprite *ss = &PrivGBase(GfxBase)->sprite;

    if (!ss->ss_Shown && ss->ss_Sprite)
        sprite_show(ss, GfxBase);
    ReleaseSemaphore(&ss->ss_Lock);
}

/*****************************************************************************

    NAME */

        AROS_LH2(struct ExtSprite *, AllocSpriteDataA,

/*  SYNOPSIS */
        AROS_LHA(struct BitMap  *, bitmap, A2),
        AROS_LHA(struct TagItem *, tagList, A1),

/*  LOCATION */
        struct GfxBase *, GfxBase, 170, Graphics)

/*  FUNCTION
        Make a sprite from an image of up to 2 planes. SPRITEA_Width
        (default 16, at most 32) is its width; with SPRITEA_OldDataFormat
        bitmap is old sprite data instead: the position and control
        words, then a plane 0 and a plane 1 word for each row, ended by
        a pair of 0 words or as many rows as SPRITEA_OutputHeight says.

    RESULT
        The sprite, or NULL.

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct ExtSprite *es;
    struct BitMap *img;
    ULONG width = GetTagData(SPRITEA_Width, 16, tagList);
    BOOL old = GetTagData(SPRITEA_OldDataFormat, FALSE, tagList);
    UWORD *data = (UWORD *)bitmap;
    UWORD height, y;

    if (width == 0 || width > SPRITEMAXW)
        return NULL;
    if (old) {
        /* after the position and control words; up to the 0, 0 pair
           unless SPRITEA_OutputHeight gives the height */
        width = 16;
        data += 2;
        height = GetTagData(SPRITEA_OutputHeight, 0, tagList);
        if (height == 0)
            for (; data[height * 2] || data[height * 2 + 1]; height++)
                ;
    } else
        height = bitmap->Rows;
    if (height == 0)
        return NULL;

    es = AllocMem(sizeof(struct ExtSprite), MEMF_ANY | MEMF_CLEAR);
    if (!es)
        return NULL;
    img = AllocBitMap(width, height, 2, BMF_CLEAR, NULL);
    if (!img) {
        FreeMem(es, sizeof(struct ExtSprite));
        return NULL;
    }
    if (old) {
        for (y = 0; y < height; y++) {
            ((UWORD *)img->Planes[0])[y * (img->BytesPerRow / 2)] = data[y * 2];
            ((UWORD *)img->Planes[1])[y * (img->BytesPerRow / 2)] = data[y * 2 + 1];
        }
    } else
        BltBitMap(bitmap, 0, 0, img, 0, 0, width, height, 0xC0, 0x03, NULL);

    es->es_BitMap = img;
    es->es_wordwidth = (width + 15) >> 4;
    es->es_SimpleSprite.height = height;
    return es;

    AROS_LIBFUNC_EXIT
} /* AllocSpriteDataA */

/*****************************************************************************

    NAME */

        AROS_LH1(void, FreeSpriteData,

/*  SYNOPSIS */
        AROS_LHA(struct ExtSprite *, extsp, A2),

/*  LOCATION */
        struct GfxBase *, GfxBase, 172, Graphics)

/*  FUNCTION
        Free a sprite from AllocSpriteDataA(). It must not be shown.

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    if (!extsp)
        return;
    FreeBitMap(extsp->es_BitMap);
    FreeMem(extsp, sizeof(struct ExtSprite));

    AROS_LIBFUNC_EXIT
} /* FreeSpriteData */

/*****************************************************************************

    NAME */

        AROS_LH4(LONG, ChangeExtSpriteA,

/*  SYNOPSIS */
        AROS_LHA(struct ViewPort  *, vp, A0),
        AROS_LHA(struct ExtSprite *, oldsprite, A1),
        AROS_LHA(struct ExtSprite *, newsprite, A2),
        AROS_LHA(struct TagItem   *, tags, A3),

/*  LOCATION */
        struct GfxBase *, GfxBase, 171, Graphics)

/*  FUNCTION
        Show newsprite on vp's bitmap in place of oldsprite, at the
        old one's position. A NULL newsprite takes the pointer away.

    RESULT
        TRUE, or FALSE without memory for what it covers and its
        templates.

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct SoftSprite *ss = &PrivGBase(GfxBase)->sprite;
    struct BitMap *bm = NULL, *save = NULL, *img;
    UBYTE *tmpl = NULL;
    ULONG size = 0;

    if (newsprite) {
        bm = vp->RasInfo->BitMap;
        img = newsprite->es_BitMap;
        save = AllocBitMap(GetBitMapAttr(img, BMA_WIDTH), img->Rows,
            GetBitMapAttr(bm, BMA_DEPTH), 0, bm);
        tmpl = sprite_tmpl(img, &size);
        if (!save || !tmpl) {
            FreeBitMap(save);
            if (tmpl)
                FreeMem(tmpl, size);
            return FALSE;
        }
    }

    ObtainSemaphore(&ss->ss_Lock);
    if (ss->ss_Shown)
        sprite_hide(ss, GfxBase);
    if (ss->ss_Save) {
        FreeBitMap(ss->ss_Save);
        FreeMem(ss->ss_Tmpl, ss->ss_TmplSize);
    }
    ss->ss_Save = save;
    ss->ss_Tmpl = tmpl;
    ss->ss_TmplSize = size;
    ss->ss_Sprite = newsprite;
    ss->ss_BitMap = bm;
    if (newsprite) {
        if (oldsprite) {
            ss->ss_X = oldsprite->es_SimpleSprite.x;
            ss->ss_Y = oldsprite->es_SimpleSprite.y;
        }
        newsprite->es_SimpleSprite.x = ss->ss_X;
        newsprite->es_SimpleSprite.y = ss->ss_Y;
        sprite_show(ss, GfxBase);
    }
    ReleaseSemaphore(&ss->ss_Lock);
    return TRUE;

    AROS_LIBFUNC_EXIT
} /* ChangeExtSpriteA */

/*****************************************************************************

    NAME */

        AROS_LH4(void, MoveSprite,

/*  SYNOPSIS */
        AROS_LHA(struct ViewPort *, vp, A0),
        AROS_LHA(struct SimpleSprite *, sprite, A1),
        AROS_LHA(WORD, x, D0),
        AROS_LHA(WORD, y, D1),

/*  LOCATION */
        struct GfxBase *, GfxBase, 71, Graphics)

/*  FUNCTION
        Move the sprite of an ExtSprite shown with ChangeExtSpriteA() to
        (x, y) of the ViewPort's bitmap.

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct SoftSprite *ss = &PrivGBase(GfxBase)->sprite;

    sprite->x = x;
    sprite->y = y;
    ObtainSemaphore(&ss->ss_Lock);
    if (ss->ss_Sprite && &ss->ss_Sprite->es_SimpleSprite == sprite &&
            (x != ss->ss_X || y != ss->ss_Y)) {
        if (ss->ss_Shown)
            sprite_hide(ss, GfxBase);
        ss->ss_X = x;
        ss->ss_Y = y;
        sprite_show(ss, GfxBase);
    }
    ReleaseSemaphore(&ss->ss_Lock);

    AROS_LIBFUNC_EXIT
} /* MoveSprite */
