/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: Hook for handling fonts in FONTS:
*/

/****************************************************************************************/

#include "diskfont_intern.h"

/****************************************************************************************/

struct DirEntry;

struct FileEntry
{
    struct MinNode          Node;
    struct DirEntry        *DirEntry;
    STRPTR                  FileName;
    struct DateStamp        FileChanged;
    ULONG                   Numentries;
    struct TTextAttr       *Attrs;
};

struct DirEntry
{
    struct MinNode         Node;
    BPTR                   DirLock;
    struct DateStamp       DirChanged;
    struct MinList         FileList;
};

struct DF_FontsData /*DiskFontData */
{
    struct DirEntry       *CurrentDirEntry;
    struct FileEntry      *CurrentFileEntry, *PrevFileEntry;
    UWORD                  AttrsIndex;
    struct FileEntry      *RememberFileEntry;
    UWORD                  RememberIndex;
};

struct DF_FileData
{
    struct FontDescrHeader *FDH;
    STRPTR                  FilePart;
    STRPTR                  OrigName;
    struct TTextAttr       *LastAttr;
    UWORD                   AttrsIndex;
    UWORD                   RememberIndex;
};

typedef enum {DF_FONTSDATA, DF_FILEDATA} DF_DataType;

struct DF_Data
{
    DF_DataType       Type;
    struct TTextAttr *ReqAttr;
    union
    {
        struct DF_FontsData FontsData;
        struct DF_FileData  FileData;
    } u;
};

    
/****************************************************************************************/
#define REMOVE(n)	Remove((struct Node *)(n))
#define ADDTAIL(l, n)	AddTail((struct List *)(l), (struct Node *)(n))
#define NEWLIST(l)	NewList((struct List *)(l))

/* A list's first node and a node's next, or NULL */
static APTR GetHead(struct MinList *l)
{
    return l->mlh_Head->mln_Succ ? l->mlh_Head : NULL;
}

static APTR GetSucc(APTR n)
{
    struct MinNode *s = ((struct MinNode *)n)->mln_Succ;

    return s->mln_Succ ? s : NULL;
}

/****************************************************************************************/

/*****************/
/* ReadFileEntry */
/*****************/

/****************************************************************************************/

STATIC struct FileEntry *ReadFileEntry(struct FileInfoBlock *fib, struct DiskfontBase *DiskfontBase)
{
    struct FontDescrHeader *fdh;
    struct FileEntry       *retval = NULL;
    ULONG                   size, strsize;
    int                     i;
    STRPTR                  filename;
    
    fdh = ReadFontDescr(fib->fib_FileName, DiskfontBase);
    if (fdh == NULL)
        return NULL;
    
    strsize = (strlen(fib->fib_FileName) + 1 + (AROS_LONGALIGN - 1)) & ~(AROS_LONGALIGN - 1);
    size = sizeof(struct FileEntry) + strsize + fdh->NumEntries * sizeof(struct TTextAttr);
    
    retval = AllocVec(size, MEMF_ANY|MEMF_CLEAR);
    if (retval == NULL)
    {
        FreeFontDescr(fdh, DiskfontBase);
        return NULL;
    }

    filename = (STRPTR)(retval + 1);
    strcpy(filename, fib->fib_FileName);
    retval->FileName = filename;
    retval->FileChanged = fib->fib_Date;
    
    retval->Numentries = fdh->NumEntries;
    retval->Attrs = (struct TTextAttr *)(filename + strsize);
    CopyMem(fdh->TAttrArray, retval->Attrs, fdh->NumEntries * sizeof(struct TTextAttr));

    for (i = 0; i < fdh->NumEntries; i++)
        retval->Attrs[i].tta_Name = retval->FileName;
    
    FreeFontDescr(fdh, DiskfontBase);
    
    return retval;
}
    
/****************************************************************************************/

/*****************/
/* FreeFileEntry */
/*****************/

/****************************************************************************************/

STATIC VOID FreeFileEntry(struct FileEntry *feptr, struct DiskfontBase *DiskfontBase)
{
    FreeVec(feptr);
}

/****************************************************************************************/

/****************/
/* FreeFileList */
/****************/

/****************************************************************************************/

STATIC VOID FreeFileList(struct MinList *filelist, struct DiskfontBase *DiskfontBase)
{
    struct FileEntry *feptr, *nextfeptr;

    ForeachNodeSafe(filelist, feptr, nextfeptr)
    {
        REMOVE(feptr);
        FreeFileEntry(feptr, DiskfontBase);
    }
}

/****************************************************************************************/

/****************/
/* GetFileList  */
/****************/

/****************************************************************************************/

/* Build the list of .font files in the current directory, which fib examined */
STATIC BOOL GetFileList(struct DirEntry *direntry, struct FileInfoBlock *fib, struct DiskfontBase *DiskfontBase)
{
    struct MinList       newlist;
    struct FileEntry    *fe, *nextfe;
    BOOL                 retval = TRUE;
    
    NEWLIST(&newlist);
    
    while (retval && ExNext(direntry->DirLock, fib))
    {
        ULONG namelen = strlen(fib->fib_FileName);

        if (fib->fib_DirEntryType >= 0 || namelen < 5 ||
            Stricmp(fib->fib_FileName+namelen-5, ".font") != 0)
            continue;
            
        /* Is a FileEntry for this file already in the list */
        for (fe = (struct FileEntry *)GetHead(&direntry->FileList);
             fe != NULL;
             fe = (struct FileEntry *)GetSucc(fe))
        {
            if (strcmp(fib->fib_FileName, fe->FileName) == 0)
                break;
        }
            
        if (fe == NULL)
        {
            fe = ReadFileEntry(fib, DiskfontBase);
            if (fe != NULL)
            {
                fe->DirEntry = direntry;
                ADDTAIL(&newlist, fe);
            }
        }
        else
        {
            REMOVE(fe);
            
            if (CompareDates(&fib->fib_Date, &fe->FileChanged) == 0)
            {
                ADDTAIL(&newlist, fe);
            }
            else
            {
                /* Date changed rereading information */
                FreeFileEntry(fe, DiskfontBase);
                
                fe = ReadFileEntry(fib, DiskfontBase);
                if (fe == NULL)
                {
                    retval = FALSE;
                }
                else
                {
                    fe->DirEntry = direntry;
                    ADDTAIL(&newlist, fe);
                }
            }
        }
    }
    if (retval && IoErr() != ERROR_NO_MORE_ENTRIES)
        retval = FALSE;
    
    if (!retval)
    {
        FreeFileList(&newlist, DiskfontBase);
    }
    else
    {
        /* Remove FileEntries that are still in memory but not anymore
         * on disk */
        FreeFileList(&direntry->FileList, DiskfontBase);
        
        ForeachNodeSafe(&newlist, fe, nextfe)
        {
            REMOVE(fe);
            ADDTAIL(&direntry->FileList, fe);
        }
    }
    
    return retval;
}

/****************************************************************************************/

/****************/
/* FreeDirEntry */
/****************/

/****************************************************************************************/

STATIC VOID FreeDirEntry(struct DirEntry *direntry, struct DiskfontBase *DiskfontBase)
{
    if (direntry!=NULL)
    {
        FreeFileList(&direntry->FileList, DiskfontBase);
        UnLock(direntry->DirLock);
        FreeVec(direntry);
    }
}

/****************************************************************************************/

/****************/
/* ReadDirEntry */
/****************/

/****************************************************************************************/

/*
 * The fonts of dirlock's directory, from direntry (which has its own
 * lock) while the directory's date is the same; NULL and dirlock and
 * direntry freed on failure.
 */
STATIC struct DirEntry *ReadDirEntry(BPTR dirlock, struct DirEntry *direntry, struct DiskfontBase *DiskfontBase)
{
    struct FileInfoBlock *fib;
    BPTR olddir;
    BOOL ok;

    if (direntry != NULL)
    {
        UnLock(dirlock);
        dirlock = direntry->DirLock;
    }

    fib = AllocDosObject(DOS_FIB, NULL);
    ok = fib != NULL && Examine(dirlock, fib);

    if (ok && direntry != NULL &&
        CompareDates(&direntry->DirChanged, &fib->fib_Date) == 0)
    {
        FreeDosObject(DOS_FIB, fib);
        return direntry;
    }

    if (ok && direntry == NULL)
    {
        direntry = AllocVec(sizeof(struct DirEntry), MEMF_ANY | MEMF_CLEAR);
        if (direntry != NULL)
        {
            direntry->DirLock  = dirlock;
            NEWLIST(&direntry->FileList);
        }
        ok = direntry != NULL;
    }

    if (ok)
    {
        direntry->DirChanged = fib->fib_Date;
        olddir = CurrentDir(dirlock);
        ok = GetFileList(direntry, fib, DiskfontBase);
        CurrentDir(olddir);
    }

    if (fib != NULL)
        FreeDosObject(DOS_FIB, fib);

    if (!ok)
    {
        if (direntry != NULL)
            FreeDirEntry(direntry, DiskfontBase);
        else
            UnLock(dirlock);
        return NULL;
    }
    
    return direntry;
}

/****************************************************************************************/

/****************/
/* FreeResouces */
/****************/

/****************************************************************************************/

STATIC VOID FreeResources(struct DF_Data *df_data, struct DiskfontBase *DiskfontBase)
{
    ReleaseSemaphore(&DiskfontBase->fontssemaphore);

    if (df_data->Type == DF_FILEDATA && df_data->u.FileData.FDH != NULL)
        FreeFontDescr(df_data->u.FileData.FDH, DiskfontBase);
    
    FreeMem(df_data, sizeof (struct DF_Data));
}

/****************************************************************************************/

/****************************/
/* CleanUpFontsDirEntryList */
/****************************/

/****************************************************************************************/

VOID CleanUpFontsDirEntryList(struct DiskfontBase *DiskfontBase)
{
    struct DirEntry *direntry, *direntry2;
    
    ForeachNodeSafe(&DiskfontBase->fontsdirentrylist, direntry, direntry2)
        FreeDirEntry(direntry, DiskfontBase);
}

/****************************************************************************************/

/******************/
/* AllocResources */
/******************/

/****************************************************************************************/

STATIC struct DF_Data *AllocResources(struct TTextAttr *reqattr, struct DiskfontBase *DiskfontBase)
{
    struct DF_Data *df_data;
    
    ObtainSemaphore(&DiskfontBase->fontssemaphore);
    
    /* Allocate user data */
    if ((df_data = AllocMem( sizeof (struct DF_Data), MEMF_ANY | MEMF_CLEAR)))
    {
        df_data->ReqAttr = reqattr;
        
        if (reqattr==NULL || FilePart(reqattr->tta_Name)==reqattr->tta_Name)
        {
            struct DevProc *dp = NULL;
            struct MinList newdirlist;
            struct DirEntry *direntry, *direntry2;

            df_data->Type = DF_FONTSDATA;
            
            NEWLIST(&newdirlist);
            while((dp = GetDeviceProc(FONTSDIR, dp))!=NULL)
            {
                BPTR lock;
            
                lock = DupLock(dp->dvp_Lock);
                if (lock==NULL)
                    continue;

                /* See if direntry is already in memory */
                for (direntry = (struct DirEntry *)GetHead(&DiskfontBase->fontsdirentrylist);
                     direntry != NULL;
                     direntry = (struct DirEntry *)GetSucc(direntry))
                {
                    if (SameLock(direntry->DirLock, lock) == LOCK_SAME)
                    {
                        REMOVE(direntry);
                        break;
                    }
                }

                /* Read or update fonts information in this directory */
                direntry = ReadDirEntry(lock, direntry, DiskfontBase);
                if (direntry!=NULL)
                    ADDTAIL(&newdirlist, direntry);
            }

            /* Clean up directory lists that are in memory but not in the
             * FONTS: assign anymore */
            ForeachNodeSafe(&DiskfontBase->fontsdirentrylist, direntry, direntry2)
            {
                REMOVE(direntry);
                FreeDirEntry(direntry, DiskfontBase);
            }

            ForeachNodeSafe(&newdirlist, direntry, direntry2)
            {
                REMOVE(direntry);
                ADDTAIL(&DiskfontBase->fontsdirentrylist, direntry);
            }

            FreeDeviceProc(dp);
        }
        else
        {
            df_data->Type = DF_FILEDATA;
            df_data->u.FileData.FDH = ReadFontDescr(reqattr->tta_Name, DiskfontBase);
        }
    }
    else
        ReleaseSemaphore(&DiskfontBase->fontssemaphore);
    
    return df_data;
}

/****************************************************************************************/

/*******************/
/* DF_IteratorInit */
/*******************/

/****************************************************************************************/

APTR DF_IteratorInit(struct TTextAttr *reqattr, struct DiskfontBase *DiskfontBase)
{
    struct DF_Data *df_data;

    df_data = AllocResources(reqattr, DiskfontBase);
    if (df_data == NULL)
        return NULL;

    switch (df_data->Type)
    {
    case DF_FONTSDATA:
        df_data->u.FontsData.CurrentDirEntry = (struct DirEntry *)GetHead(&DiskfontBase->fontsdirentrylist);
        df_data->u.FontsData.CurrentFileEntry = NULL;
        if (df_data->u.FontsData.CurrentDirEntry != NULL)
            df_data->u.FontsData.CurrentFileEntry = (struct FileEntry *)GetHead(&df_data->u.FontsData.CurrentDirEntry->FileList);

        /* If DirEntry was empty search for one that is not empty */
        while (df_data->u.FontsData.CurrentDirEntry != NULL && df_data->u.FontsData.CurrentFileEntry == NULL)
        {
            df_data->u.FontsData.CurrentDirEntry = (struct DirEntry *)GetSucc(df_data->u.FontsData.CurrentDirEntry);
            if (df_data->u.FontsData.CurrentDirEntry != NULL)
                df_data->u.FontsData.CurrentFileEntry = (struct FileEntry *)GetHead(&df_data->u.FontsData.CurrentDirEntry->FileList);
        }
        df_data->u.FontsData.AttrsIndex = 0;
        
        break;

    case DF_FILEDATA:
        df_data->u.FileData.AttrsIndex = 0;
        df_data->u.FileData.FilePart = FilePart(df_data->ReqAttr->tta_Name);
        df_data->u.FileData.LastAttr = NULL;
        break;
    }
    
    return df_data;
}

/****************************************************************************************/

/**********************/
/* DF_IteratorGetNext */
/**********************/

/****************************************************************************************/

struct TTextAttr *DF_IteratorGetNext(APTR iterator, struct DiskfontBase *DiskfontBase)
{
    struct TTextAttr *retval = NULL;
    struct DF_Data *df_data = (struct DF_Data *)iterator;

    if (df_data==NULL)
        return NULL;
    
    switch (df_data->Type)
    {
    case DF_FONTSDATA:
        if (df_data->u.FontsData.CurrentDirEntry==NULL)
            return NULL;
            
        retval = df_data->u.FontsData.CurrentFileEntry->Attrs + df_data->u.FontsData.AttrsIndex;
       
        /* Let the iterator point to the next attr */
        if (df_data->u.FontsData.AttrsIndex == df_data->u.FontsData.CurrentFileEntry->Numentries-1)
        {
            df_data->u.FontsData.PrevFileEntry = df_data->u.FontsData.CurrentFileEntry;
            df_data->u.FontsData.CurrentFileEntry = (struct FileEntry *)GetSucc(df_data->u.FontsData.CurrentFileEntry);
            df_data->u.FontsData.AttrsIndex = 0;
            while (df_data->u.FontsData.CurrentDirEntry != NULL && df_data->u.FontsData.CurrentFileEntry == NULL)
            {
                df_data->u.FontsData.CurrentDirEntry = (struct DirEntry *)GetSucc(df_data->u.FontsData.CurrentDirEntry);
                if (df_data->u.FontsData.CurrentDirEntry != NULL)
                    df_data->u.FontsData.CurrentFileEntry = (struct FileEntry *)GetHead(&df_data->u.FontsData.CurrentDirEntry->FileList);
            }
        }
        else
            df_data->u.FontsData.AttrsIndex++;
        break;
        
    case DF_FILEDATA:
        if (df_data->u.FileData.LastAttr != NULL)
        {
            df_data->u.FileData.LastAttr->tta_Name = df_data->u.FileData.OrigName;
            df_data->u.FileData.LastAttr = NULL;
        }

        if (df_data->u.FileData.FDH==NULL
            || df_data->u.FileData.AttrsIndex >= df_data->u.FileData.FDH->NumEntries)
            return NULL;

        /* Get the TextAttr the iterator is pointing to */
        retval = &df_data->u.FileData.FDH->TAttrArray[df_data->u.FileData.AttrsIndex];
        df_data->u.FileData.OrigName = retval->tta_Name;
        retval->tta_Name = df_data->u.FileData.FilePart;
        df_data->u.FileData.LastAttr = retval;
        
        /* Let the iterator point to the next element.
         */
        df_data->u.FileData.AttrsIndex++;

        break;
    }

    return retval;
}

/****************************************************************************************/

/***********************/
/* DF_IteratorRemember */
/***********************/

/****************************************************************************************/

VOID DF_IteratorRemember(APTR iterator, struct DiskfontBase *DiskfontBase)
{
    struct DF_Data *df_data = (struct DF_Data *)iterator;

    switch (df_data->Type)
    {
    case DF_FONTSDATA:
        if (df_data->u.FontsData.AttrsIndex > 0)
        {
            df_data->u.FontsData.RememberIndex = df_data->u.FontsData.AttrsIndex-1;
            df_data->u.FontsData.RememberFileEntry = df_data->u.FontsData.CurrentFileEntry;
        }
        else
        {
            df_data->u.FontsData.RememberFileEntry = df_data->u.FontsData.PrevFileEntry;
            df_data->u.FontsData.RememberIndex = df_data->u.FontsData.RememberFileEntry->Numentries-1;
        }
        break;

    case DF_FILEDATA:
        df_data->u.FileData.RememberIndex = df_data->u.FileData.AttrsIndex-1;
        break;
    }
}

/****************************************************************************************/

/***************************/
/* DF_IteratorRememberOpen */
/***************************/

/****************************************************************************************/

struct TextFont *DF_IteratorRememberOpen(APTR iterator, struct DiskfontBase *DiskfontBase)
{
    struct DF_Data *df_data = (struct DF_Data *)iterator;
    struct FontDescrHeader *fdh = NULL;
    struct TTextAttr *RememberAttr = NULL;
    struct TextFont *tf = NULL;
    BPTR olddir = 0, lock, dirlock;

    /* Set current dir and get the Remember TextAttr */
    switch (df_data->Type)
    {
    case DF_FONTSDATA:
        {
            struct FileEntry *rementry = df_data->u.FontsData.RememberFileEntry;
            
            olddir = CurrentDir(rementry->DirEntry->DirLock);
            fdh = ReadFontDescr(rementry->FileName, DiskfontBase);
            if (fdh == NULL)
            {
                CurrentDir(olddir);
                return NULL;
            }
            RememberAttr = &fdh->TAttrArray[df_data->u.FontsData.RememberIndex];
        }
        break;
        
    case DF_FILEDATA:
        if (df_data->u.FileData.LastAttr != NULL)
        {
            df_data->u.FileData.LastAttr->tta_Name = df_data->u.FileData.OrigName;
            df_data->u.FileData.LastAttr = NULL;
        }
        
        lock = Lock(df_data->ReqAttr->tta_Name, ACCESS_READ);
        if (lock == NULL)
            return NULL;
        
        dirlock = ParentDir(lock);
        UnLock(lock);
        if (dirlock == NULL)
            return NULL;
        olddir = CurrentDir(dirlock);
        
        fdh = df_data->u.FileData.FDH;
        RememberAttr = &fdh->TAttrArray[df_data->u.FileData.RememberIndex];
        break;
    }

    tf = ReadDiskFont(RememberAttr,
                      FilePart(df_data->ReqAttr->tta_Name),
                      DiskfontBase);

    dirlock = CurrentDir(olddir);
    switch (df_data->Type)
    {
    case DF_FONTSDATA:
        FreeFontDescr(fdh, DiskfontBase);
        break;
        
    case DF_FILEDATA:
        UnLock(dirlock);
        break;
    }

    if (tf != NULL)
    {
        /* PPaint's personal.font/8 has not set FPF_DISKFONT,
         (FPF_ROMFONT neither), but AmigaOS diskfont.library
         still shows FPF_DISKFONT set when opening this font */
                   
        tf->tf_Flags &= ~FPF_ROMFONT;
        tf->tf_Flags |= FPF_DISKFONT;

        DF_AddFont(tf, DiskfontBase);
    }

    return tf;
}

/****************************************************************************************/

/*******************/
/* DF_IteratorFree */
/*******************/

/****************************************************************************************/

VOID DF_IteratorFree(APTR iterator, struct DiskfontBase *DiskfontBase)
{
    struct DF_Data *df_data = (struct DF_Data *)iterator;

    FreeResources(df_data, DiskfontBase);
}

/****************************************************************************************/

/**************/
/* DF_AddFont */
/**************/

/****************************************************************************************/

/* tf, loaded or made, in graphics' list and opened once; freed by CleanMem */
VOID DF_AddFont(struct TextFont *tf, struct DiskfontBase *DiskfontBase)
{
    struct DiskFontHeader *dfh;

    dfh = (struct DiskFontHeader *)((UBYTE *)tf -
        (ULONG)&((struct DiskFontHeader *)0)->dfh_TF);

    /* Held across both, so no other task frees the font before it is
       counted */
    ObtainSemaphore(DiskfontBase->gfxfontsem);

    AddFont(tf);
    tf->tf_Accessors++;
    AddTail(&DiskfontBase->diskfontlist, &dfh->dfh_DF);

    ReleaseSemaphore(DiskfontBase->gfxfontsem);
}
