/*
    Copyright (C) 1995-2001, The AROS Development Team. All rights reserved.

    Desc: Functions for readin .font files
*/

/****************************************************************************************/

#include "diskfont_intern.h"

/****************************************************************************************/

/******************/
/* ReadFontDescr  */
/******************/

/****************************************************************************************/

/* A .font file's sizes; a tagged entry's tags are left out */
struct FontDescrHeader *ReadFontDescr(CONST_STRPTR filename, struct DiskfontBase *DiskfontBase)
{
    struct FontDescrHeader   *fdh = 0;
    struct TTextAttr         *tattr;
    struct FontContentsHeader fch;
    struct FontContents       fc;
    BPTR                      fh;
    UWORD                     numentries;

    if (!(fh = Open((STRPTR)filename, MODE_OLDFILE)))
        goto failure;

    if (!(fdh = AllocMem(sizeof (struct FontDescrHeader), MEMF_ANY | MEMF_CLEAR)) )
        goto failure;

    /* The file id and the number of (T)FontContents, which follow */
    if (Read(fh, &fch, sizeof(fch)) != sizeof(fch))
        goto failure;
    
    if ((fch.fch_FileID != FCH_ID) && (fch.fch_FileID != TFCH_ID))
        goto failure;
    
    numentries = fch.fch_NumEntries;
    if (numentries == 0) goto failure;
    
    /* Allocate mem for array of TTextAttrs */
    fdh->TAttrArray = AllocVec(numentries * sizeof(struct TTextAttr), MEMF_ANY|MEMF_CLEAR);
    if (!fdh->TAttrArray) goto failure;
    
    tattr = fdh->TAttrArray;
        
    while (numentries--)
    {
        if (Read(fh, &fc, sizeof(fc)) != sizeof(fc))
            goto failure;

        /* A tagged entry's name ends before its tags */
        fc.fc_FileName[fch.fch_FileID == TFCH_ID ? MAXFONTPATH - 2 : MAXFONTPATH - 1] = 0;

        /* We set tattr->tta_Name to the real font name, for
           example FONTS:helvetica/15 */
           
        if (!(tattr->tta_Name = AllocVec(strlen(fc.fc_FileName) + 1, MEMF_ANY)))
            goto failure;
                
        strcpy(tattr->tta_Name, fc.fc_FileName);
        fdh->NumEntries++;

        tattr->tta_YSize = fc.fc_YSize;
        tattr->tta_Style = fc.fc_Style & ~FSF_TAGGED;
        tattr->tta_Flags = fc.fc_Flags;
        tattr->tta_Tags = NULL;

        tattr ++;
        
    } /* while (numentries--) */
    
    Close(fh);
      
    return fdh;

failure:
    /* Free all allocated memory */
    
    if (fh) Close(fh);
    
    if (fdh)
        FreeFontDescr(fdh, DFB(DiskfontBase) );
        
    return NULL;

}

/****************************************************************************************/

/******************/
/* FreeFontDescr  */
/******************/

/****************************************************************************************/

VOID FreeFontDescr(struct FontDescrHeader *fdh, struct DiskfontBase *DiskfontBase)
{
    struct TTextAttr *tattr;

    UWORD numentries;
    
    if (fdh)
    {
        tattr       = fdh->TAttrArray;
        numentries  = fdh->NumEntries;
    
        if (tattr)
        {
            /* Go through the whole array freeing the names */
               
            while(numentries--)
            {
                FreeVec(tattr->tta_Name);
                tattr ++;
            }
            
            FreeVec(fdh->TAttrArray);
        }
    
        FreeMem(fdh, sizeof (struct FontDescrHeader));
    }
}

/****************************************************************************************/
