/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: Graphics function BestModeIDA()
*/

#include <graphics/modeid.h>
#include <graphics/displayinfo.h>
#include <proto/graphics.h>
#include <proto/utility.h>

#include "graphics_intern.h"
#include "gfxfuncsupport.h"

#define AROS_MONITOR_ID_MASK  0xFFFF0000 /* Internal monitor ID mask */

/*
 * There are lots of parameters and it's totally inoptimal to pass them
 * as separate arguments.
 * Instead we store our search state as a structure.
 */
struct MatchData {
    /* Arguments */
    ULONG  dipf_musthave;
    ULONG  dipf_mustnothave;
    UBYTE  redbits;
    UBYTE  greenbits;
    UBYTE  bluebits;
    UBYTE  depth;
    ULONG  monitorid;
    UWORD  nominal_width;
    UWORD  nominal_height;
    UWORD  desired_width;
    UWORD  desired_height;
    /* Results */
    ULONG  found_id;
    UWORD  found_depth;
    UWORD  found_width;
    UWORD  found_height;
};

static void BestModeID(struct MatchData *args, ULONG modemask,
                       struct GfxBase *GfxBase)
{
    DisplayInfoHandle dinfo;
    struct DisplayInfo disp;
    struct DimensionInfo dims;
    ULONG modeid;

    for(modeid = NextDisplayInfo(INVALID_ID); modeid != INVALID_ID; modeid = NextDisplayInfo(modeid)) {
        UWORD gm_width, gm_height;

        if(args->monitorid != INVALID_ID && (args->monitorid & modemask) != (modeid & modemask))
            continue;

        dinfo = FindDisplayInfo(modeid);
        if(GetDisplayInfoData(dinfo, (UBYTE *)&disp, sizeof(disp), DTAG_DISP, modeid) < offsetof(struct DisplayInfo, pad2))
            continue;

        /* Filter out not available modes */
        if(disp.NotAvailable)
            continue;

        /* Filter out modes which do not meet out special needs */
        if(disp.PropertyFlags & args->dipf_mustnothave)
            continue;
        if((disp.PropertyFlags & args->dipf_musthave) != args->dipf_musthave)
            continue;

        if(GetDisplayInfoData(dinfo, (UBYTE *)&dims, sizeof(dims), DTAG_DIMS, modeid) < offsetof(struct DimensionInfo,
                MaxOScan))
            continue;
        gm_width  = dims.Nominal.MaxX - dims.Nominal.MinX + 1;
        gm_height = dims.Nominal.MaxY - dims.Nominal.MinY + 1;

        /* FIXME: Take aspect ratio into account (nominal_width : nominal_height) */

        /* Check if mode is not worse than requested */
        if(disp.RedBits   >= args->redbits
                && disp.GreenBits >= args->greenbits
                && disp.BlueBits  >= args->bluebits
                && dims.MaxDepth  >= args->depth
                && gm_width       >= args->desired_width
                && gm_height      >= args->desired_height) {
            /* Check if this mode matches closer than the one we already found */
            if((dims.MaxDepth <= args->found_depth) &&
                    (gm_width <= args->found_width) && (gm_height <= args->found_height)) {
                /* Remember the new mode only if something changed. */
                if((dims.MaxDepth < args->found_depth) ||
                        (gm_width < args->found_width) || (gm_height < args->found_height)) {
                    args->found_id     = modeid;
                    args->found_depth  = dims.MaxDepth;
                    args->found_width  = gm_width;
                    args->found_height = gm_height;
                }
            }
        }
    } /* for (each mode) */
}

/*****************************************************************************

    NAME */
#include <proto/graphics.h>

        AROS_LH1(ULONG, BestModeIDA,

/*  SYNOPSIS */
        AROS_LHA(struct TagItem *, TagItems, A0),

/*  LOCATION */
        struct GfxBase *, GfxBase, 175, Graphics)

/*  FUNCTION

    INPUTS
        TagItems - pointer to an array of TagItems

    TAGS
        BIDTAG_ViewPort (struct ViewPort *) - Viewport for which a mode is searched. Default: NULL
        BIDTAG_MonitorID (ULONG)            - Returned ID must use this monitor
        BIDTAG_SourceID (ULONG)             - Use this ModeID instead of a ViewPort.
                                              DIPFMustHave mask is made up of the
                                              ((DisplayInfo->PropertyFlags of this ID & SPECIAL_FLAGS) |
                                              DIPFMustHave flags).
                                              Default:
                                              if BIDTAG_ViewPort was passed: VPModeID(vp), else the
                                              DIPFMustHave and DIPFMustNotHave are unchanged.
        BIDTAG_Depth (UBYTE)                - Minimal depth. Default:
                                              if BIDTAG_ViewPort is passed: vp->RasInfo->BitMap->Depth,
                                              else 1.
        BIDTAG_NominalWidth (UWORD),
        BIDTAG_NominalHeight (UWORD)        - Aspect ratio. Default:
                                              if BIDTAG_SourceID: SourceID NominalDimensionInfo
                                              if BIDTAG_ViewPort: vp->DWidth and vp->DHeight
                                              or the display's first mode.
        BIDTAG_DesiredWidth (UWORD)         - Width. Default: DIBTAG_NominalWidth.
        BIDTAG_DesiredHeight (UWORD)        - Height. Default: BIDTAG_NominalHeight.
        BIDTAG_RedBits (UBYTE),
        BIDTAG_GreenBits (UBYTE),
        BIDTAG_BlueBits (UBYTE)             - Bits per gun the mode must support. Default: 1,
                                              as the displays have fixed palettes of
                                              1 (mono) or 2 (RGBI) bits a gun
        BIDTAG_DIPFMustHave (ULONG)         - DIPF flags the resulting mode must have
        BIDTAG_DIPFMustNotHave (ULONG)      - DIPF flags the resulting mode must not have

    RESULT
        ID - ID of the best mode to use, or INVALID_ID if a match
             could not be found

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO
        graphics/modeid.h, graphics/displayinfo.h

    INTERNALS

    HISTORY

******************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct TagItem *tag, *tstate = TagItems;
    DisplayInfoHandle dinfo;
    struct DisplayInfo disp;
    struct DimensionInfo dims;
    struct ViewPort *vp = NULL;
    ULONG  sourceid = INVALID_ID;
    struct MatchData args;

    args.dipf_musthave    = 0;
    args.dipf_mustnothave = SPECIAL_FLAGS;
    args.redbits = args.greenbits = args.bluebits = 1;
    args.depth            = 1;
    args.monitorid        = INVALID_ID;
    args.nominal_width    = args.nominal_height = 0;
    args.desired_width    = args.desired_height = 0;
    args.found_id         = INVALID_ID;
    args.found_depth      = -1;
    args.found_width      = args.found_height = -1;

    /* Get defaults which can be overriden */
    while((tag = NextTagItem(&tstate))) {
        switch(tag->ti_Tag) {
        case BIDTAG_DIPFMustHave:
            args.dipf_musthave = tag->ti_Data;
            break;

        case BIDTAG_DIPFMustNotHave:
            args.dipf_mustnothave = tag->ti_Data;
            break;

        case BIDTAG_MonitorID:
            args.monitorid = tag->ti_Data;
            break;

        case BIDTAG_RedBits:
            args.redbits = tag->ti_Data;
            break;

        case BIDTAG_GreenBits:
            args.greenbits = tag->ti_Data;
            break;

        case BIDTAG_BlueBits:
            args.bluebits = tag->ti_Data;
            break;

        case BIDTAG_ViewPort:
            /* If we got ViewPort, obtain some more defaults from it */
            vp = (struct ViewPort *)tag->ti_Data;
            args.nominal_width  = vp->DWidth;
            args.nominal_height = vp->DHeight;
            args.depth          = GetBitMapAttr(vp->RasInfo->BitMap, BMA_DEPTH);
            sourceid            = GetVPModeID(vp);
            break;
        }
    }

    /* Then process SourceID, it overrides ViewPort size and mode */
    sourceid = GetTagData(BIDTAG_SourceID, sourceid, TagItems);
    if(sourceid != INVALID_ID) {
        /* Patch musthave flags */
        if(GetDisplayInfoData(NULL, (UBYTE *)&disp, sizeof(disp), DTAG_DISP, sourceid) >= offsetof(struct DisplayInfo,
                Resolution))
            args.dipf_musthave |= (disp.PropertyFlags & SPECIAL_FLAGS);

        if(!vp) {
            /* Override nominal size from source ID only if there was no ViewPort specified */
            dinfo = FindDisplayInfo(sourceid);
            if(GetDisplayInfoData(dinfo, (UBYTE *)&dims, sizeof(dims), DTAG_DIMS, sourceid) >= offsetof(struct DimensionInfo,
                    MaxOScan)) {
                args.nominal_width  = dims.Nominal.MaxX - dims.Nominal.MinX + 1;
                args.nominal_height = dims.Nominal.MaxY - dims.Nominal.MinY + 1;
            }
        }
    }

    /* The display's nominal size is that of its first mode */
    if(!args.nominal_width && GetDisplayInfoData(NULL, (UBYTE *)&dims, sizeof(dims), DTAG_DIMS,
            NextDisplayInfo(INVALID_ID)) >= offsetof(struct DimensionInfo, MaxOScan)) {
        args.nominal_width  = dims.Nominal.MaxX - dims.Nominal.MinX + 1;
        args.nominal_height = dims.Nominal.MaxY - dims.Nominal.MinY + 1;
    }

    /* Get high-priority parameters */
    args.nominal_width  = GetTagData(BIDTAG_NominalWidth, args.nominal_width, TagItems);
    args.nominal_height = GetTagData(BIDTAG_NominalHeight, args.nominal_height, TagItems);
    args.desired_width  = GetTagData(BIDTAG_DesiredWidth, args.nominal_width, TagItems);
    args.desired_height = GetTagData(BIDTAG_DesiredHeight, args.nominal_height, TagItems);
    args.depth          = GetTagData(BIDTAG_Depth, args.depth, TagItems);

    /* Exclude flags in MustHave from MustNotHave (CHECKME: if this correct?) */
    args.dipf_mustnothave &= ~args.dipf_musthave;

    /* First try to find exact match */
    BestModeID(&args, ~0L, GfxBase);

    /* Still not found, AROS_MONITOR_ID_MASK.
     * Mask out bit 12 in monitorid because the user may (and will) pass in IDs defined in include/graphics/modeid.h
     * (like PAL_MONITOR_ID, VGA_MONITOR_ID, etc) which have bit 12 set)
     */
    if(args.found_id == INVALID_ID && args.monitorid != INVALID_ID)
        BestModeID(&args, AROS_MONITOR_ID_MASK, GfxBase);

    return args.found_id;

    AROS_LIBFUNC_EXIT
} /* BestModeIDA */
