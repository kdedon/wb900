/*
    Copyright (C) 1995-2016, The AROS Development Team. All rights reserved.

    Desc: Main fileof diskfont function AvailFonts()
*/


#include "diskfont_intern.h"

/****************************************************************************/

struct BufferInfo
{
    struct AvailFontsHeader *afh;
    LONG space;
    union {
        struct TAvailFonts *taf;
        struct AvailFonts *af;
    } u;
    UBYTE *endptr;
};


STATIC struct BufferInfo *BufferInfoCreate(STRPTR buffer, LONG bufBytes, BOOL tagged, struct DiskfontBase *DiskfontBase);
STATIC VOID BufferInfoAdd(struct BufferInfo *bi, UWORD type, struct TTextAttr *tattr, BOOL tagged, struct DiskfontBase *DiskfontBase);
STATIC VOID BufferInfoFree(struct BufferInfo *bi, struct DiskfontBase *DiskfontBase);

/*****************************************************************************

    NAME */
#include <proto/diskfont.h>

        AROS_LH3(LONG, AvailFonts,

/*  SYNOPSIS */
        AROS_LHA(STRPTR, buffer, A0),
        AROS_LHA(LONG  , bufBytes, D0),
        AROS_LHA(LONG  , flags, D1),

/*  LOCATION */
        struct Library *, DiskfontBase, 6, Diskfont)

/*  FUNCTION
        Fill the supplied buffer with info about the available fonts.
        The buffer will after function execution first contains a
        struct AvailFontsHeader, and then an array of struct AvailFonts
        element (or TAvailFonts elements if AFF_TAGGED is specified in the
        flags parameter). If the buffer is not big enough for the
        descriptions than the additional length needed will be returned.

    INPUTS
        buffer    - pointer to a buffer in which the font descriptions
                    should be placed.
                    
        bufBytes  - size of the supplied buffer.
        
        flags     - flags telling what kind of fonts to load,
                    for example AFF_TAGGED for tagged fonts also,
                    AFF_MEMORY for fonts in memory, AFF_DISK for fonts
                    on disk.

    RESULT
        shortage  - 0 if buffer was big enough or a number telling
                    how much additional place is needed.

    NOTES
        If the routine failes, then the afh_Numentries field
        in the AvailFontsHeader will be 0.

    EXAMPLE

    BUGS

    SEE ALSO
        OpenDiskfont(), <diskfont/diskfont.h>

    INTERNALS

    HISTORY
        27-11-96    digulla automatically created from
                            diskfont_lib.fd and clib/diskfont_protos.h

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    LONG retval = 0;
    APTR iterator;
    struct TTextAttr *attr;
    BOOL tagged = (flags & AFF_TAGGED) ? TRUE : FALSE;
    struct BufferInfo *bi;
    
    bi = BufferInfoCreate(buffer, bufBytes, tagged, DFB(DiskfontBase));
    if (bi == NULL)
        return bufBytes;

    if (flags & AFF_MEMORY)
    {
        iterator = MF_IteratorInit(DFB(DiskfontBase));
        while((attr = MF_IteratorGetNext(iterator, DFB(DiskfontBase)))!=NULL)
        {
            if (!IS_SCALED_FONT(attr) || (flags & AFF_SCALED))
            {
                /* taf_Type only ever seems to contain one of AFF_MEMORY/AFF_DISK/AFF_SCALED,
                 but not a combination of these. */
                UWORD type = IS_SCALED_FONT(attr) ? AFF_SCALED : AFF_MEMORY;

                BufferInfoAdd(bi, type, attr, tagged, DFB(DiskfontBase));
            }
        }
        MF_IteratorFree(iterator, DFB(DiskfontBase));
    }
        
    if (flags & AFF_DISK)
    {
        iterator = DF_IteratorInit(NULL, DFB(DiskfontBase));
        while((attr = DF_IteratorGetNext(iterator, DFB(DiskfontBase)))!=NULL)
        {
            if (!IS_SCALED_FONT(attr) || (flags & AFF_SCALED))
            {
                /* For disk fonts the type is always AFF_DISK ??? */
                BufferInfoAdd(bi, AFF_DISK, attr, tagged, DFB(DiskfontBase));
            }
        }
        DF_IteratorFree(iterator, DFB(DiskfontBase));
    }

    retval = bi->space>=0 ? 0 : -bi->space;
    
    BufferInfoFree(bi, DFB(DiskfontBase));
    
    return retval;

    AROS_LIBFUNC_EXIT
    
} /* AvailFonts */


/*****************************************************************************/

STATIC struct BufferInfo *BufferInfoCreate(STRPTR buffer, LONG bufBytes, BOOL tagged, struct DiskfontBase *DiskfontBase)
{
    struct BufferInfo *retval;
    
    retval = (struct BufferInfo *)AllocMem(sizeof(struct BufferInfo), MEMF_ANY | MEMF_CLEAR);
    if (retval != NULL)
    {
        retval->afh = (struct AvailFontsHeader *)buffer;
        if (buffer != NULL && bufBytes >= sizeof(struct AvailFontsHeader))
        {
            retval->afh->afh_NumEntries = 0;
        }
        retval->space = bufBytes - (LONG)sizeof(struct AvailFontsHeader);
        if (tagged)
            retval->u.taf = (struct TAvailFonts *)(retval->afh+1);
        else
            retval->u.af = (struct AvailFonts *)(retval->afh+1);
        retval->endptr = (UBYTE *)buffer + bufBytes;
    }
    
    return retval;
}


STATIC VOID BufferInfoAdd(struct BufferInfo *bi, UWORD type, struct TTextAttr *tattr, BOOL tagged, struct DiskfontBase *DiskfontBase)
{
    UWORD namesize = strlen(tattr->tta_Name) + 1;

    bi->space -= (tagged ? sizeof(struct TAvailFonts) : sizeof(struct AvailFonts)) + namesize;
    
    if (bi->space >= 0)
    {
        bi->endptr -= namesize;
        strcpy(bi->endptr, tattr->tta_Name);
        
        if (tagged)
        {
            /* No font has tags */
            bi->u.taf->taf_Type = type;
            bi->u.taf->taf_Attr.tta_Name = bi->endptr;
            bi->u.taf->taf_Attr.tta_YSize = tattr->tta_YSize;
            bi->u.taf->taf_Attr.tta_Style = tattr->tta_Style;
            bi->u.taf->taf_Attr.tta_Flags = tattr->tta_Flags;
            bi->u.taf->taf_Attr.tta_Tags = NULL;
            bi->u.taf++;
        }
        else
        {
            bi->u.af->af_Type = type;
            bi->u.af->af_Attr.ta_Name = bi->endptr;
            bi->u.af->af_Attr.ta_YSize = tattr->tta_YSize;
            bi->u.af->af_Attr.ta_Style = tattr->tta_Style;
            bi->u.af->af_Attr.ta_Flags = tattr->tta_Flags;
            
            bi->u.af++;
        }
        
        bi->afh->afh_NumEntries++;
    }
}


STATIC VOID BufferInfoFree(struct BufferInfo *bi, struct DiskfontBase *DiskfontBase)
{
    FreeMem(bi, sizeof(struct BufferInfo));
}

/*****************************************************************************/
