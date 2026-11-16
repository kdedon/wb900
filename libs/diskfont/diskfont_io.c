/*
    Copyright (C) 1995-2020, The AROS Development Team. All rights reserved.

    Functions for reading disk font files.
*/

/****************************************************************************************/

#include "diskfont_intern.h"

/****************************************************************************************/

/**************/
/* FontHeader */
/**************/

/****************************************************************************************/

/*
 * The DiskFontHeader of a loaded font file, or NULL.  The file's first
 * hunk holds its first long, then the header, whose pointers the loader
 * relocated: the font is used where it was loaded.
 */
struct DiskFontHeader *FontHeader(BPTR seglist)
{
    struct DiskFontHeader *dfh;

    /* The block's size, before the link */
    if (((ULONG *)seglist)[-1] < sizeof(ULONG) + sizeof(BPTR) + sizeof(ULONG) +
        sizeof(struct DiskFontHeader))
        return NULL;

    /* Go past the link and the dummy code */
    dfh = (struct DiskFontHeader *)(UB(BADDR(seglist)) + sizeof(BPTR) + sizeof(ULONG));

    /* Colour fonts are not supported */
    if (dfh->dfh_FileID != DFH_ID || (dfh->dfh_TF.tf_Style & FSF_COLORFONT) ||
        dfh->dfh_TF.tf_CharData == NULL || dfh->dfh_TF.tf_CharLoc == NULL)
        return NULL;

    return dfh;
}

/****************************************************************************************/

/****************/
/* ReadDiskFont */
/****************/

/****************************************************************************************/

struct TextFont *ReadDiskFont(
        struct TTextAttr *reqattr,
        CONST_STRPTR realfontname,
        struct DiskfontBase *DiskfontBase)
{
    struct DiskFontHeader *dfh;
    struct TextFont       *tf;
    CONST_STRPTR           filepart;
    BPTR                   seglist;
    ULONG                  i;

    if ((seglist = LoadSeg(reqattr->tta_Name)) == NULL)
        return NULL;

    if ((dfh = FontHeader(seglist)) == NULL)
    {
        UnLoadSeg(seglist);
        return NULL;
    }

    tf = &dfh->dfh_TF;
    dfh->dfh_Segment = seglist;
    tf->tf_Message.mn_Node.ln_Type = NT_FONT;
    tf->tf_Message.mn_ReplyPort = NULL;
    tf->tf_Style &= ~FSF_TAGGED;

    /* ----------------------- */
    /* Add fontname */
    filepart = FilePart((STRPTR)realfontname);
    i = strlen(filepart) + 1;
    if (i >= sizeof(dfh->dfh_Name)) i = sizeof(dfh->dfh_Name) - 1;

    CopyMem((STRPTR) filepart, dfh->dfh_Name, i);
    dfh->dfh_Name[i] = 0;

    tf->tf_Message.mn_Node.ln_Name = dfh->dfh_Name;
    dfh->dfh_DF.ln_Name = dfh->dfh_Name;

    return tf;
}

/****************************************************************************************/
