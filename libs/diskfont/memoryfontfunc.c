/*
    Copyright (C) 1995-2001, The AROS Development Team. All rights reserved.

    Desc: Hook for getting font descriptions from memory font list in mem
*/

#include "diskfont_intern.h"

/****************************************************************************************/

/* Userdata needed by the MemoryFontHook */
struct MFData
{
    /* Pointer to the current font in the memory font list */
    struct TextFont *CurrentFont;
    struct TTextAttr currattr;
};

/****************************************************************************************/

/*******************/
/* MF_IteratorInit */
/*******************/

/****************************************************************************************/

APTR MF_IteratorInit(struct DiskfontBase *DiskfontBase)
{
    struct MFData *mfdata;

    mfdata = AllocVec(sizeof(struct MFData), MEMF_ANY|MEMF_CLEAR);
    if (mfdata == NULL)
        return NULL;

    /* Held until MF_IteratorFree, so no font leaves the list */
    ObtainSemaphore(DiskfontBase->gfxfontsem);

    /* Get the first font */
    mfdata->CurrentFont = (struct TextFont*)GfxBase->TextFonts.lh_Head;

    return (APTR)mfdata;
}

/****************************************************************************************/

/**********************/
/* MF_IteratorGetNext */
/**********************/

/****************************************************************************************/

struct TTextAttr *MF_IteratorGetNext(APTR iterator, struct DiskfontBase *DiskfontBase)
{
    struct MFData *mfdata = (struct MFData *)iterator;
    struct TextFont *currfont;
    
    if (mfdata==NULL || mfdata->CurrentFont->tf_Message.mn_Node.ln_Succ==NULL)
        return NULL;
    
    currfont = mfdata->CurrentFont;
    
    mfdata->currattr.tta_Tags = NULL;
    mfdata->currattr.tta_Name = currfont->tf_Message.mn_Node.ln_Name;
    mfdata->currattr.tta_YSize = currfont->tf_YSize;
    mfdata->currattr.tta_Style = currfont->tf_Style;
    mfdata->currattr.tta_Flags = currfont->tf_Flags;

    /* Go to next font */
    mfdata->CurrentFont = (struct TextFont *)currfont->tf_Message.mn_Node.ln_Succ;
    
    return &mfdata->currattr;
}

/****************************************************************************************/

/*******************/
/* MF_IteratorFree */
/*******************/

/****************************************************************************************/

VOID MF_IteratorFree(APTR iterator, struct DiskfontBase *DiskfontBase)
{
    struct MFData *mfdata = (struct MFData *)iterator;

    /* Init only takes the semaphore when it returns non-NULL. */
    if (mfdata == NULL)
        return;

    FreeVec(mfdata);
    ReleaseSemaphore(DiskfontBase->gfxfontsem);
}

/****************************************************************************************/
