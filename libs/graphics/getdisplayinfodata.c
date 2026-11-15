/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: Graphics function GetDisplayInfoData()
*/

#include <graphics/displayinfo.h>
#include <graphics/modeid.h>
#include <proto/exec.h>

#include "graphics_intern.h"

/****************************************************************************************/

static const UBYTE size_checks[] = {
    sizeof(struct DisplayInfo),
    sizeof(struct DimensionInfo),
    sizeof(struct MonitorInfo),
    sizeof(struct NameInfo),
};

#define KNOWN_IDS 4

static ULONG check_sizes(ULONG tagID, ULONG size, struct GfxBase *GfxBase);

#define DLONGSZ             (sizeof (ULONG) * 2)
#define DTAG_TO_IDX(dtag)   (((dtag) & 0x7FFFF000) >> 12)

/*
 * Display resolution in ticks: 1280 x 1024 on the screen of a
 * fixed-ratio monitor is 11 x 11
 */
static void CalcScreenResolution(Point *res, const struct GfxMode *m)
{
    if (m->gm_ResX)
    {
        res->x = m->gm_ResX;
        res->y = m->gm_ResY;
        return;
    }
    res->x = 1280L * 11 / m->gm_Width;
    res->y = 1024L * 11 / m->gm_Height;
}

/* The decimal digits of n at s; the end */
static UBYTE *putnum(UBYTE *s, UWORD n)
{
    UBYTE d[5];
    WORD i = 0;

    do {
        d[i++] = '0' + n % 10;
        n /= 10;
    } while(n);
    while(i > 0)
        *s++ = d[--i];
    return s;
}

/****************************************************************************************/

/*****************************************************************************

    NAME */
#include <proto/graphics.h>

        AROS_LH5(ULONG, GetDisplayInfoData,

/*  SYNOPSIS */
        AROS_LHA(DisplayInfoHandle, handle, A0),
        AROS_LHA(UBYTE *, buf, A1),
        AROS_LHA(ULONG, size, D0),
        AROS_LHA(ULONG, tagID, D1),
        AROS_LHA(ULONG, ID, D2),

/*  LOCATION */
        struct GfxBase *, GfxBase, 126, Graphics)

/*  FUNCTION
        Fills buffer with information about displayinfo handle.

    INPUTS
        handle - displayinfo handle
        buf    - pointer to destination buffer
        size   - buffer size in bytes
        tagID  - data chunk type
                 DTAG_DISP (DisplayInfo)
                 DTAG_DIMS (DimensionInfo)
                 DTAG_MNTR (MonitorInfo)
                 DTAG_NAME (NameInfo)
        ID     - displayinfo identifier, optionally used if handle is NULL

    RESULT
        result - if positive, number of bytes actually transferred
                 if zero, no information for ID was available

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO
        FindDisplayInfo(), NextDisplayInfo(), graphics/displayinfo.h

    INTERNALS
        A mode's palette is the display's, fixed: PaletteRange is its
        number of colours.

    HISTORY

******************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct QueryHeader  *qh;
    ULONG               structsize;
    const struct GfxMode *m;
    struct GfxDriver    *gd;

    if(NULL == handle) {
        /* FindDisplayInfo() handles INVALID_ID itself */
        handle = FindDisplayInfo(ID);
    }

    if(NULL == handle)
        return 0;

    /*
     * ID is likely INVALID_ID, but we need its value.
     * Get it from the handle.
     */
    m  = (const struct GfxMode *)handle;
    ID = m->gm_ModeID;
    gd = PrivGBase(GfxBase)->display;

    /* Build the queryheader */
    structsize = check_sizes(tagID, size, GfxBase);
    if(!structsize)
        return 0;
    qh = AllocMem(structsize, MEMF_CLEAR);
    if(!qh)
        return 0;

    /* Fill in the queryheader */
    qh->StructID  = tagID;
    qh->DisplayID = ID;
    qh->SkipID    = TAG_SKIP;

    qh->Length    = (structsize + (DLONGSZ - 1)) / DLONGSZ;

    switch(tagID) {
    case DTAG_DISP: {
        struct DisplayInfo *di = (struct DisplayInfo *)qh;

        di->NotAvailable = gd->gd_Jumper && m != gd->gd_Jumper ?
            DI_AVAIL_NOMONITOR : 0;

        /*
         * We always set DIPF_IS_SPRITES because we always emulate
         * one sprite for mouse pointer. Every mode can be Workbench's.
         */
        di->PropertyFlags = DIPF_IS_SPRITES | DIPF_IS_WB;

        /* RGBI has two levels and I for each gun, mono one */
        di->RedBits = di->GreenBits = di->BlueBits = m->gm_Depth > 1 ? 2 : 1;
        di->PaletteRange = 1 << m->gm_Depth;

        CalcScreenResolution(&di->Resolution, m);

        /* One sprite, drawn by software, for the mouse pointer */
        di->NumStdSprites = 1;

        /* At the moment sprites always have the same resolution as display */
        di->SpriteResolution = di->Resolution;

        break;
    }

    case DTAG_DIMS: {
        struct DimensionInfo *di = (struct DimensionInfo *)qh;

        di->MaxDepth = m->gm_Depth;

        di->MinRasterWidth  = 16;
        di->MinRasterHeight = 1;
        di->MaxRasterWidth  = m->gm_Width;
        di->MaxRasterHeight = m->gm_Height;

        di->Nominal.MinX    = 0;
        di->Nominal.MinY    = 0;
        di->Nominal.MaxX    = m->gm_Width  - 1;
        di->Nominal.MaxY    = m->gm_Height - 1;

        di->TxtOScan        = di->Nominal;
        di->StdOScan        = di->Nominal;
        di->MaxOScan        = di->Nominal;
        di->VideoOScan      = di->Nominal;

        break;
    }

    case DTAG_MNTR: {
        struct MonitorInfo *mi = (struct MonitorInfo *)qh;

        /* No MonitorSpec: the display has one fixed sync */
        CalcScreenResolution(&mi->ViewResolution, m);

        mi->TotalRows         = m->gm_Height;
        mi->MinRow            = 0;
        mi->MouseTicks.x      = 1;
        mi->MouseTicks.y      = 1;
        mi->PreferredModeID   = ID;

        /* One screen is shown at a time */
        mi->Compatibility = MCOMPAT_NOBODY;

        break;
    }

    case DTAG_NAME: {
        struct NameInfo *ni = (struct NameInfo *)qh;
        UBYTE *n = ni->Name;
        const char *c;

        if(m->gm_Name) {
            for(c = m->gm_Name; *c && n < ni->Name + DISPLAYNAMELEN - 1; c++)
                *n++ = *c;
            *n = 0;
            break;
        }
        /* "HR:1024x800 Mono" */
        for(c = gd->gd_Node.ln_Name; *c && n < ni->Name + 8; c++)
            *n++ = *c >= 'a' && *c <= 'z' ? *c - 'a' + 'A' : *c;
        *n++ = ':';
        n = putnum(n, m->gm_Width);
        *n++ = 'x';
        n = putnum(n, m->gm_Height);
        for(c = m->gm_Depth > 1 ? " RGBI" : " Mono"; *c; c++)
            *n++ = *c;
        *n = 0;
        break;
    }

    default:
        break;
    }

    if(size > structsize)
        size = structsize;
    CopyMem(qh, buf, size);
    FreeMem(qh, structsize);
    /* NULL-terminate the name in case if it was trimmed */
    if(tagID == DTAG_NAME && size > 0)
        buf[size - 1] = 0;

    return size;

    AROS_LIBFUNC_EXIT

} /* GetDisplayInfoData */

/****************************************************************************************/

static ULONG check_sizes(ULONG tagID, ULONG size, struct GfxBase *GfxBase)
{
    ULONG idx;

    idx = DTAG_TO_IDX(tagID);

    if(idx >= KNOWN_IDS)
        return 0;

    return size_checks[idx];
}
