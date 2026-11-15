/*
    Copyright (C) 1995-2013, The AROS Development Team. All rights reserved.

    Desc:
*/

#include <workbench/icon.h>
#include <graphics/gfx.h>
#include "icon_intern.h"

STATIC STRPTR StrDupIcon(struct DiskObject *dobj, STRPTR str, APTR IconBase)
{
    char *newstr;
    if (!str) return NULL;
    newstr = AllocMemIcon(dobj, strlen(str)+1, MEMF_PUBLIC, IconBase);
    if (newstr) strcpy(newstr,str);
    return newstr;
}

STATIC struct Image *ImageDupIcon(struct DiskObject *dobj, struct Image *src, BOOL dupImage, BOOL dupImageData, APTR IconBase)
{
    struct Image *dest;

    if (!src) return NULL;

    if (dupImageData)
        dupImage = TRUE;

    if (!dupImage)
        return src;

    dest = (struct Image*)AllocMemIcon(dobj, sizeof(struct Image),
        MEMF_PUBLIC, IconBase);
    if (dest)
    {
        LONG data_size;

        *dest = *src;
        dest->NextImage = NULL;

        /* It seems planepick must be ignored. See drawer icon in MCC_TheBar-26.7.lha
           which seems to contain a SelectRender image with planepick == 0, but the image
           data is still there. */
        data_size = RASSIZE(src->Width,src->Height) * src->Depth;

        if (!dupImageData)
            return dest;
        
        if ((dest->ImageData = AllocMemIcon(dobj, data_size,
            MEMF_PUBLIC, IconBase)))
        {
            CopyMem(src->ImageData,dest->ImageData,data_size);
            return dest;
        }
    }
    return NULL;
}

/*****************************************************************************

    NAME */
#include <proto/icon.h>

        AROS_LH2(struct DiskObject *, DupDiskObjectA,

/*  SYNOPSIS */
        AROS_LHA(struct DiskObject *, icon, A0),
        AROS_LHA(struct TagItem *, tags, A1),

/*  LOCATION */
        struct IconBase *, IconBase, 25, Icon)

/*  FUNCTION

    INPUTS

    RESULT

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

    HISTORY

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct NativeIcon *mem;
    struct DiskObject *dobj;
    struct NativeIcon *srcnativeicon;

    if (!icon) return NULL;

    dobj = NewDiskObject(icon->do_Type);
    if (dobj == NULL)
        return NULL;

    mem = NATIVEICON(dobj);
    /* We can't cast this, since it could be a hand-build one */
    srcnativeicon = GetNativeIcon(icon, IconBase);

    /* copy the contents */
    *dobj = *icon;

    if (srcnativeicon)
    {
        mem->ni_IsDefault = srcnativeicon->ni_IsDefault;
        mem->ni_Frameless = srcnativeicon->ni_Frameless;
    }

    /* and now the pointers and the rest */
    /* TODO: check for errors here */

    if (GetTagData(ICONDUPA_DuplicateDefaultTool, TRUE, tags) &&
        dobj->do_DefaultTool)
    {
        dobj->do_DefaultTool = StrDupIcon(dobj,icon->do_DefaultTool,IconBase);
        if (!dobj->do_DefaultTool) goto fail;
    }
    
    if (GetTagData(ICONDUPA_DuplicateToolWindow, TRUE, tags) &&
        dobj->do_ToolWindow)
    {
        dobj->do_ToolWindow = StrDupIcon(dobj,icon->do_ToolWindow,IconBase);
        if (!dobj->do_ToolWindow) goto fail;
    }
   
    if (GetTagData(ICONDUPA_DuplicateDrawerData, TRUE, tags) &&
        icon->do_DrawerData)
    {
        LONG size;
        
        dobj->do_DrawerData = AllocMemIcon(dobj, sizeof(struct DrawerData),
            MEMF_PUBLIC, IconBase);
        if (!dobj->do_DrawerData) goto fail;
        
        if (((LONG)icon->do_Gadget.UserData > 0) &&
            ((LONG)icon->do_Gadget.UserData <= WB_DISKREVISION))
        {
            size = sizeof(struct DrawerData);
        }
        else
        {
            size = sizeof(struct OldDrawerData);
        }

        memset(dobj->do_DrawerData, 0, sizeof(struct DrawerData));
        CopyMem(icon->do_DrawerData, dobj->do_DrawerData, size);
    }

    /* Duplicate the image data */
    
    if (icon->do_Gadget.GadgetRender)
    {
        dobj->do_Gadget.GadgetRender = ImageDupIcon(dobj, (struct Image*)icon->do_Gadget.GadgetRender,
                GetTagData(ICONDUPA_DuplicateImages, TRUE, tags),
                GetTagData(ICONDUPA_DuplicateImageData, TRUE, tags),IconBase);
        if (!dobj->do_Gadget.GadgetRender) goto fail;
    }
    
    if (icon->do_Gadget.SelectRender)
    {
        dobj->do_Gadget.SelectRender = ImageDupIcon(dobj, (struct Image*)icon->do_Gadget.SelectRender,
                GetTagData(ICONDUPA_DuplicateImages, TRUE, tags),
                GetTagData(ICONDUPA_DuplicateImageData, TRUE, tags),IconBase);
        if (!dobj->do_Gadget.SelectRender) goto fail;
    }

    /* Duplicate the tool types */
    if (GetTagData(ICONDUPA_DuplicateToolTypes, TRUE, tags) &&
        icon->do_ToolTypes)
    {
        int num_tts;
        /* Get number of tool types */
        for (num_tts = 0;icon->do_ToolTypes[num_tts];num_tts++);

        if ((dobj->do_ToolTypes = (STRPTR *)AllocMemIcon(dobj,
            sizeof(char*) * (num_tts + 1), MEMF_PUBLIC, IconBase)))
        {
            int i;
            for (i=0;i<num_tts;i++)
            {
                dobj->do_ToolTypes[i] = StrDupIcon(dobj,icon->do_ToolTypes[i],IconBase);
                if (!dobj->do_ToolTypes[i]) goto fail;
            }
            dobj->do_ToolTypes[i] = NULL;
        }
        else
        {
            goto fail;
        }
    }

    if (GetTagData(ICONDUPA_ActivateImageData, FALSE, tags))
        LayoutIconA(dobj, LB(IconBase)->ib_Screen, NULL);

    
    return dobj;

fail:
    FreeDiskObject(dobj);
    return NULL;
    
    AROS_LIBFUNC_EXIT
} /* FreeDiskObject */
