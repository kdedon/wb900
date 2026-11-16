/*
    Copyright (C) 1995-2025, The AROS Development Team. All rights reserved.
*/

/****************************************************************************************/

#include "diskfont_intern.h"

/****************************************************************************************/

STATIC VOID CopyContents(struct List *list, APTR mem, struct DiskfontBase *DiskfontBase);
STATIC VOID FreeBuffers(struct List *list, struct DiskfontBase *DiskfontBase);

/****************************************************************************************/

struct contentsBuffer
{
    struct Node         node;
    struct FontContents fc;
};

/****************************************************************************************/

/*****************************************************************************

    NAME */
#include <proto/diskfont.h>

        AROS_LH2(struct FontContentsHeader *, NewFontContents,

/*  SYNOPSIS */
        AROS_LHA(BPTR  , fontsLock, A0),
        AROS_LHA(STRPTR, fontName, A1),

/*  LOCATION */
        struct Library *, DiskfontBase, 7, Diskfont)

/*  FUNCTION

    Create an array of FontContents entries describing the fonts related
    with 'fontName' -- this is those in the directory with the same name
    as 'fontName' without the ".font" suffix.

    INPUTS

    fontsLock  --  A lock on the FONTS: directory or another directory
                   containing the font file and associated directory
                   exists.
    fontName   --  The font name (with the ".font" suffix).

    RESULT

    Pointer to a struct FontContentsHeader describing the font or NULL
    if something went wrong.

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO

    DisposeFontContents()

    INTERNALS

    HISTORY

    5.8.1999  SDuvan  partial implementation

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    BPTR   oldDir;
    BPTR   lock;
    STRPTR suffix;
    char   name[MAXFONTNAME];
    struct List contentsList;

    struct FileInfoBlock      *fib;
    struct FontContentsHeader *ret = NULL;
    struct FontContentsHeader fch = { FCH_ID , 0 };

    if (strlen(fontName) >= sizeof(name))
        return NULL;

    NewList(&contentsList);
    oldDir = CurrentDir(fontsLock);

    strcpy((char *)&name, fontName);
    suffix = strrchr(name, '.');

    if ((suffix == NULL) ||
        (strcmp(suffix, ".font") != 0))
    {
        CurrentDir(oldDir);
        return NULL;
    }

    /* Get the correct directory name */
    *suffix = 0;

    lock = Lock(name, SHARED_LOCK); /* Lock font directory */
    if (lock != NULL) {
        fib = AllocDosObject(DOS_FIB, NULL);
        if (fib != NULL) {
            CurrentDir(lock);

            if(Examine(lock, fib) == DOSTRUE)
            {

                /* Loop through the files in this font's directory */
                while (ExNext(lock, fib))
                {
                    BPTR  fontSeg;
                    struct DiskFontHeader *dfh = NULL;
                    struct contentsBuffer *cNode;
                    
                    /* Skip directories */
                    if(fib->fib_DirEntryType >= 0)
                        continue;

                    fontSeg = LoadSeg(fib->fib_FileName);

                    if(fontSeg == NULL)
                        continue;

                    dfh = FontHeader(fontSeg);
                    
                    cNode = NULL;
                    if (dfh != NULL &&
                        strlen(name) + 1 + strlen(fib->fib_FileName) < MAXFONTPATH)
                        cNode = AllocVec(sizeof(struct contentsBuffer),
                                         MEMF_PUBLIC | MEMF_CLEAR);
                    
                    if(cNode == NULL)
                    {
                        UnLoadSeg(fontSeg);
                        FreeBuffers(&contentsList, DFB(DiskfontBase));
                        UnLock(lock);
                        FreeDosObject(DOS_FIB, fib);
                        CurrentDir(oldDir);
                        return NULL;
                    }
                    
                    AddTail(&contentsList, (struct Node *)cNode);
                    
                    strcpy(cNode->fc.fc_FileName, name);
                    strcat(cNode->fc.fc_FileName, "/");
                    strcat(cNode->fc.fc_FileName, fib->fib_FileName);

                    cNode->fc.fc_YSize = dfh->dfh_TF.tf_YSize;
                    cNode->fc.fc_Style = dfh->dfh_TF.tf_Style & ~FSF_TAGGED;
                    cNode->fc.fc_Flags = dfh->dfh_TF.tf_Flags;

                    cNode->fc.fc_Flags &= ~FPF_REMOVED;
                    cNode->fc.fc_Flags &= ~FPF_ROMFONT;
                    cNode->fc.fc_Flags |=  FPF_DISKFONT;
                    
                    fch.fch_NumEntries++;
                    
                    UnLoadSeg(fontSeg);
                } /* while(there are files left in the directory) */
                
                if (IoErr() != ERROR_NO_MORE_ENTRIES)
                        fch.fch_NumEntries = 0;
            } /* if(we could examine the font's directory) */

            FreeDosObject(DOS_FIB, fib);
        }
        UnLock(lock);
    }
    if (fch.fch_NumEntries > 0) {
        ret = (struct FontContentsHeader *)AllocVec(sizeof(struct FontContentsHeader) + fch.fch_NumEntries * sizeof(struct FontContents), MEMF_PUBLIC | MEMF_CLEAR);
        if(ret != NULL)
        {
            ret->fch_NumEntries = fch.fch_NumEntries;
            ret->fch_FileID = fch.fch_FileID;

            CopyContents(&contentsList, ret + 1, DFB(DiskfontBase));
        }
    }
    FreeBuffers(&contentsList, DFB(DiskfontBase));
    CurrentDir(oldDir);

    return ret;
    
    AROS_LIBFUNC_EXIT
    
} /* NewFontContents */

/****************************************************************************************/

STATIC VOID FreeBuffers(struct List *list, struct DiskfontBase *DiskfontBase)
{
    struct Node *node, *temp;

    ForeachNodeSafe(list, node, temp)
    {
        Remove(node);
        FreeVec(node);
    }
}

/****************************************************************************************/

STATIC VOID CopyContents(struct List *list, APTR mem, struct DiskfontBase *DiskfontBase)
{
    struct contentsBuffer *temp, *next;
    struct FontContents *fc = mem;

    ForeachNodeSafe(list, temp, next)
    {
        CopyMem(&temp->fc, fc, sizeof(struct FontContents));
        fc++;
    }
}

/****************************************************************************************/
