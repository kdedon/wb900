/*
    Copyright (C) 1995-2007, The AROS Development Team. All rights reserved.

    Desc: Diskfont function OpenDiskFont()
*/

#include "diskfont_intern.h"


/*****************************************************************************

    NAME */
#include <proto/diskfont.h>

        AROS_LH1(struct TextFont *, OpenDiskFont,

/*  SYNOPSIS */
        AROS_LHA(struct TextAttr *, textAttr, A0),

/*  LOCATION */
        struct Library *, DiskfontBase, 5, Diskfont)

/*  FUNCTION
                Tries to open the font specified by textAttr. If the font has allready
                been loaded into memory, it will be opened with OpenFont(). Otherwise
                OpenDiskFont() will try to load it from disk.

    INPUTS
        textAttr - Description of the font to load. Without FPF_DESIGNED
                   in ta_Flags, the nearest size is scaled to ta_YSize when no
                   font has it.

    RESULT
        Pointer to a struct TextFont on success, 0 on failure.

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO
        AvailFonts()

    INTERNALS
        Internally now the WeighTAMatch function is used to find the best
        font match. In OS 3.0 and above this graphics function is made
        private to the library. So in the future maybe this function can
        be replaced by a more flexible internal mechanism.

*****************************************************************************/
{
    AROS_LIBFUNC_INIT
        
    WORD match_weight = 0, new_match_weight;
    APTR iterator;
    BOOL bestinmemory = TRUE;
    struct TTextAttr *ttait;
    struct TextFont *tf = NULL;
    struct DiskFontHeader *dfh;
    STRPTR filepart = FilePart(textAttr->ta_Name), wholename = textAttr->ta_Name;
    ULONG len = strlen(filepart);
    
    if (len>5 && Stricmp(filepart+len-5,".font")==0)
        len -= 5;

    /* Check if font is in memory, ignore path */
    textAttr->ta_Name = filepart;
    
    tf = OpenFont(textAttr);
    if (tf!=NULL)
    {
        struct TextAttr tattr;
        
        tattr.ta_Name = tf->tf_Message.mn_Node.ln_Name;
        tattr.ta_YSize = tf->tf_YSize;
        tattr.ta_Style = tf->tf_Style;
        tattr.ta_Flags = tf->tf_Flags;
        
        match_weight = WeighTAMatch(textAttr, &tattr, NULL);
    }

    textAttr->ta_Name = wholename;
    
    if (match_weight!=MAXFONTMATCHWEIGHT)
    {
        iterator = DF_IteratorInit((struct TTextAttr *)textAttr, DFB(DiskfontBase));
        if (iterator != NULL)
        {
            while ((ttait = DF_IteratorGetNext(iterator, DFB(DiskfontBase)))!=NULL)
            {
                ULONG len2 = strlen(ttait->tta_Name) - 5;

                if ((len == len2)  && (Strnicmp(ttait->tta_Name, filepart, len) == 0))
                {
                    new_match_weight = WeighTAMatch(textAttr,
                                                    (struct TextAttr *)ttait,
                                                    NULL);

                    if (new_match_weight > match_weight)
                    {
                        match_weight = new_match_weight;
                        DF_IteratorRemember(iterator, DFB(DiskfontBase));
                        bestinmemory = FALSE;
                    }
                    
                    if (match_weight==MAXFONTMATCHWEIGHT)
                        break;
                }
            }
        
            /* Best still in memory then open this font, otherwise load from disk */
            if (!bestinmemory)
            {
                if (tf!=NULL)
                    CloseFont(tf);
                tf = DF_IteratorRememberOpen(iterator, DFB(DiskfontBase));
            }
            DF_IteratorFree(iterator, DFB(DiskfontBase));
        }
    }

    /* The nearest size, scaled */
    if (tf && tf->tf_YSize != textAttr->ta_YSize &&
        !(textAttr->ta_Flags & FPF_DESIGNED) &&
        (dfh = Diskfont_9(DiskfontBase, tf, textAttr)) != NULL)
    {
        CloseFont(tf);
        tf = &dfh->dfh_TF;
        DF_AddFont(tf, DFB(DiskfontBase));
    }
        
    return tf;
        
    AROS_LIBFUNC_EXIT
    
} /* OpenDiskFont */
