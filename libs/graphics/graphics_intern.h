#ifndef GRAPHICS_INTERN_H
#define GRAPHICS_INTERN_H
/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: Internal header file for graphics.library
*/

#include <aros/libcall.h>
#include <exec/execbase.h>
#include <exec/lists.h>
#include <exec/nodes.h>
#include <exec/semaphores.h>
#include <exec/interrupts.h>
#include <exec/memory.h>
#include <graphics/gfxbase.h>
#include <graphics/text.h>
#include <graphics/rastport.h>
#include <graphics/regions.h>
#include <graphics/clip.h>
#include <graphics/layers.h>
#include <graphics/view.h>
#include <graphics/sprite.h>
#include <graphics/gfxdriver.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/utility.h>

#include "gfxformat.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
/* <stddef.h>'s */
#define offsetof(t, m)  ((ULONG)&((t *)0)->m)

/* The glyphs of a font: LoChar to HiChar, then the default glyph */
#define NUMCHARS(tf) 	((tf->tf_HiChar - tf->tf_LoChar) + 2)

/* Rectangles per chunk: a chunk of 6 is 124 bytes, a small block that
 * stays out of the whole free pages big allocations need */
#define SIZERECTBUF		6

struct RegionRectangleExt {
    struct RegionRectangle     RR;
    IPTR                       Counter;
};

struct RegionRectangleExtChunk {
    struct RegionRectangleExt       Rects[SIZERECTBUF];
    struct RegionRectangleExtChunk *FirstChunk;
};

#define RRE(x)     ((struct RegionRectangleExt *)(x))
#define Counter(x) (RRE(x)->Counter)
#define Chunk(x)   ((x) ? ((struct RegionRectangleExtChunk *)&RRE(x)[-Counter(x)]) : NULL)
#define Head(x)    ((x) ? (&((struct RegionRectangleExtChunk *)&RRE(x)[-Counter(x)])->FirstChunk->Rects[0].RR) : NULL)

/* PaletteExtra internals */

typedef WORD PalExtra_RefCnt_Type;
typedef WORD PalExtra_AllocList_Type;

#define PALEXTRA_REFCNT(pe,n) 	    (((PalExtra_RefCnt_Type *)(pe)->pe_RefCnt)[(n)])
#define PALEXTRA_ALLOCLIST(pe,n)    (((PalExtra_AllocList_Type *)(pe)->pe_AllocList)[(n)])

/* Hashtable sizes. Must be powers of two */
#define GFXASSOCIATE_HASHSIZE   8

/* The pointer (softsprite.c) */
struct SoftSprite {
    struct SignalSemaphore  ss_Lock;
    struct BitMap          *ss_BitMap;     /* drawn into, or NULL */
    struct ExtSprite       *ss_Sprite;
    WORD                    ss_X, ss_Y;
    WORD                    ss_W, ss_H;     /* its size */
    BOOL                    ss_Shown;
    struct BitMap          *ss_Save;       /* what it covers, at its place */
    UBYTE                  *ss_Tmpl;       /* templates of pens 1-3, and room to merge */
    ULONG                   ss_TmplSize;
    struct BitMap          *ss_Flush;      /* drawn into, to flush at the end */
    UWORD                   ss_Hold;       /* OwnBlitter() nesting */
};

BOOL gfx_SpriteBefore(struct BitMap *bm, struct Rectangle *r, struct GfxBase *GfxBase);
void gfx_SpriteAfter(struct GfxBase *GfxBase);

/* Internal GFXBase struct */
/* Bytes of the template Text() keeps: 64 characters of an 8 by 8 font */
#define TEXTRAS 512

struct GfxBase_intern {
    struct GfxBase 	 	gfxbase;

    APTR			seglist;
    struct SignalSemaphore	hashtab_sema;	     /* hash_table arbitration semaphore			 */
    struct SignalSemaphore	view_sema;	     /* ActiView arbitration semaphore				 */
    struct SignalSemaphore  	fontsem;

    struct SoftSprite           sprite;

    /* The display bitmap LoadView() last showed, or NULL */
    struct BitMap              *shown;
    /* The bitmap with the display's planes, or NULL */
    struct BitMap              *vram;
    /* graphics' own bitmap for the display's planes (display.c), or NULL */
    struct BitMap              *spare;

    /* The display's driver and its library, once looked for (display.c) */
    struct Library             *drvlib;
    struct GfxDriver           *display;
    BOOL                        probed;

    /* WaitTOF()'s tick server */
    struct Interrupt            tofint;

    /* Regions pool */
    struct SignalSemaphore  	regionsem;

    /* Text()'s template when it fits, and whether a task has it */
    UBYTE                       textras[TEXTRAS];
    BOOL                        textrasbusy;
};

/* Macros */

#define PrivGBase(x)   	    	((struct GfxBase_intern *)x)

/*
 * The layers.library calls graphics makes, through the base layers.library
 * leaves in GfxBase.
 */
#define LayersBase ((struct Library *)GfxBase->gb_LayersBase)
#include <proto/layers.h>

/* utility.library, opened by the library's init */
#define UtilityBase             (GfxBase->UtilBase)

/* Hash index calculation */
extern ULONG CalcHashIndex(IPTR n, UWORD size);

/* display.c */
struct GfxDriver *gfx_Display(struct GfxBase *GfxBase);
const struct GfxMode *gfx_FindMode(ULONG id, struct GfxBase *GfxBase);
int gfx_TOFServer(struct GfxBase *GfxBase, UWORD intnum);
struct BitMap *gfx_ViewPortDisplay(struct ViewPort *vp, struct GfxBase *GfxBase);
BOOL gfx_ViewPortShown(struct ViewPort *vp, struct GfxBase *GfxBase);

/* color_support.c */
ULONG color_distance(struct ColorMap *cm, ULONG r, ULONG g, ULONG b, ULONG index);
BOOL color_equal(struct ColorMap *cm, ULONG r, ULONG g, ULONG b, ULONG index);
VOID color_get(struct ColorMap *cm, ULONG *r, ULONG *g, ULONG *b, ULONG index);

/* The palette of vp's display for colours first to first + count - 1, 8 bits each */
void internal_SetRGB32Colors(struct ViewPort *vp, UBYTE *rgb, ULONG first, ULONG count,
                             struct GfxBase *GfxBase);

/* a function needed by ClipBlit */
void internal_ClipBlit(struct RastPort *srcRP,
                       WORD xSrc,
                       WORD ySrc,
                       struct RastPort *destRP,
                       WORD xDest,
                       WORD yDest,
                       WORD xSize,
                       WORD ySize,
                       UBYTE minterm,
                       struct GfxBase *GfxBase);

void _DisposeRegionRectangleList
(
    struct RegionRectangle *RR,
    struct GfxBase         *GfxBase
);

struct RegionRectangle *_NewRegionRectangle
(
    struct RegionRectangle **LastRectPtr,
    struct GfxBase *GfxBase
);

BOOL _LinkRegionRectangleList
(
    struct RegionRectangle  *src,
    struct RegionRectangle **dstptr,
    struct GfxBase          *GfxBase
);

#define _NewRegionRectangleExtChunk() \
    ((struct RegionRectangleExtChunk *)AllocMem(sizeof(struct RegionRectangleExtChunk), MEMF_ANY))

#define _DisposeRegionRectangleExtChunk(_chunk) \
    FreeMem(_chunk, sizeof(struct RegionRectangleExtChunk))


#endif /* GRAPHICS_INTERN_H */
