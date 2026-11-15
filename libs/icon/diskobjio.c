/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Read and write an icon's .info file.

    The file holds the big-endian 68000 layout of the structures, which is
    also the C900's: words and longs are big-endian and 2-byte aligned and
    pointers are 32 bits.  So each structure is read and written whole; a
    pointer in the file only tells whether its data follows.
*/

#include "icon_intern.h"

/****************************************************************************************/

/* The structures as they are in the file */
#define DISKOBJECT_FILESIZE  78
#define IMAGE_FILESIZE       20
#define NEWDRAWERDATA_SIZE   6      /* dd_Flags and dd_ViewModes */

/* Compile only when the C structures have the file's layout */
typedef char check_diskobject[sizeof(struct DiskObject) == DISKOBJECT_FILESIZE ? 1 : -1];
typedef char check_image[sizeof(struct Image) == IMAGE_FILESIZE ? 1 : -1];
typedef char check_drawerdata[OLDDRAWERDATAFILESIZE == 56 ? 1 : -1];

/* A pointer written to the file to say that its data follows */
#define PRESENT ((APTR)0xABADCAFEL)

/****************************************************************************************/

static BOOL ReadBytes(BPTR file, APTR buf, ULONG size, struct IconBase *IconBase)
{
    return (ULONG)FRead(file, buf, 1, size) == size;
}

static BOOL WriteBytes(BPTR file, CONST_APTR buf, ULONG size, struct IconBase *IconBase)
{
    return (ULONG)FWrite(file, (APTR)buf, 1, size) == size;
}

/****************************************************************************************/

static struct Image * ReadImage (struct DiskObject *icon, BPTR file, struct IconBase *IconBase)
{
    struct Image * image;
    ULONG          size;

    image = AllocMemIcon(icon, sizeof(struct Image),
        MEMF_PUBLIC | MEMF_CLEAR, IconBase);
    if (!image)
        return NULL;

    if (!ReadBytes(file, image, IMAGE_FILESIZE, IconBase))
        return NULL;

    /* Size of imagedata in bytes */
    size = ((image->Width + 15) >> 4) * image->Height * image->Depth * 2;

    image->NextImage = NULL;
    image->ImageData = NULL;
    if (size)
    {
        if (!(image->ImageData = AllocMemIcon(icon, size,
            MEMF_PUBLIC, IconBase)))
            return NULL;

        if (!ReadBytes(file, image->ImageData, size, IconBase))
            return NULL;
    }

    return image;
} /* ReadImage */

/****************************************************************************************/

static BOOL WriteImage (BPTR file, struct Image * image, struct IconBase *IconBase)
{
    struct Image disk;
    ULONG size;

    disk = *image;
    disk.ImageData = image->ImageData ? PRESENT : NULL;
    disk.NextImage = NULL;
    if (!WriteBytes(file, &disk, IMAGE_FILESIZE, IconBase))
        return FALSE;

    /* Size in bytes */
    size = ((image->Width + 15) >> 4) * image->Height * image->Depth * 2;

    return image->ImageData == NULL || WriteBytes(file, image->ImageData, size, IconBase);
} /* WriteImage */

/****************************************************************************************/

static STRPTR ReadIconString (struct DiskObject *icon, BPTR file, struct IconBase *IconBase)
{
    ULONG  len;
    STRPTR str;

    if (!ReadBytes(file, &len, 4, IconBase) || len == 0 || len > 0x10000L)
        return NULL;

    str = AllocMemIcon(icon, len, MEMF_PUBLIC, IconBase);

    if (!str)
        return NULL;

    if (!ReadBytes(file, str, len, IconBase))
        return NULL;
    str[len - 1] = 0;

    return str;
} /* ReadIconString */

/****************************************************************************************/

static BOOL WriteIconString (BPTR file, STRPTR str, struct IconBase *IconBase)
{
    ULONG len;

    len = strlen (str) + 1;

    return WriteBytes(file, &len, 4, IconBase) && WriteBytes(file, str, len, IconBase);
} /* WriteIconString */

/****************************************************************************************/

static BOOL ReadToolTypes (struct DiskObject *icon, BPTR file, struct IconBase *IconBase)
{
    ULONG    t;
    ULONG    count;
    STRPTR * ttarray;

    /* Read size of ToolTypes array (each entry is 4 bytes and the
       last is 0L */
    if (!ReadBytes(file, &count, 4, IconBase))
        return FALSE;

    count = (count >> 2) - 1; /* How many entries */

    if ((LONG)count < 0)
    {
        icon->do_ToolTypes = NULL;
        return TRUE;
    }

    ttarray = AllocMemIcon(icon, (count+1)*sizeof(STRPTR),
        MEMF_PUBLIC, IconBase);
    if (!ttarray) return FALSE;

    for (t=0; t<count; t++)
    {
        ttarray[t] = ReadIconString (icon, file, IconBase);

        if (!ttarray[t])
            return FALSE;
    }

    ttarray[t] = NULL;

    icon->do_ToolTypes = (STRPTR *)ttarray;
    return TRUE;
} /* ReadToolTypes */

/****************************************************************************************/

static BOOL WriteToolTypes (BPTR file, STRPTR *ttarray, struct IconBase *IconBase)
{
    ULONG t;
    ULONG count;
    ULONG size;

    for (count=0; ttarray[count]; count++);

    size = (count+1)*4;

    if (!WriteBytes(file, &size, 4, IconBase))
        return FALSE;

    for (t=0; t<count; t++)
    {
        if (!WriteIconString (file, ttarray[t], IconBase))
            return FALSE;
    }

    return TRUE;
} /* WriteToolTypes */

/****************************************************************************************/

/* The drawer data's second part follows for icons of a disk revision */
static BOOL HasNewDrawerData(struct DiskObject *icon)
{
    IPTR rev = (IPTR)icon->do_Gadget.UserData & WB_DISKREVISIONMASK;

    return icon->do_DrawerData && (rev > 0) && (rev <= WB_DISKREVISION);
}

/****************************************************************************************/

BOOL ReadIconFile(struct NativeIcon *ni, BPTR file, struct IconBase *IconBase)
{
    struct DiskObject *icon = &ni->ni_DiskObject;
    struct Image *image;

    if (!ReadBytes(file, icon, DISKOBJECT_FILESIZE, IconBase))
        return FALSE;

    if (icon->do_Magic != WB_DISKMAGIC)
        return FALSE;

    /* Pointers of the file that point at nothing here */
    icon->do_Gadget.NextGadget = NULL;
    icon->do_Gadget.GadgetText = NULL;
    icon->do_Gadget.SpecialInfo = NULL;

    /* sba: all icons which have do_DrawerData set actually contain
     * also the drawer data */
    if (icon->do_DrawerData)
    {
        struct DrawerData *dd = AllocMemIcon(icon, sizeof(struct DrawerData),
            MEMF_PUBLIC | MEMF_CLEAR, IconBase);

        icon->do_DrawerData = dd;
        if (dd == NULL || !ReadBytes(file, dd, OLDDRAWERDATAFILESIZE, IconBase))
            return FALSE;
        dd->dd_NewWindow.FirstGadget = NULL;
        dd->dd_NewWindow.CheckMark = NULL;
        dd->dd_NewWindow.Title = NULL;
        dd->dd_NewWindow.Screen = NULL;
        dd->dd_NewWindow.BitMap = NULL;
    }

    if (icon->do_Gadget.GadgetRender)
    {
        image = ReadImage (icon, file, IconBase);
        if (!image)
            return FALSE;
        icon->do_Gadget.GadgetRender = image;
    }

    if (icon->do_Gadget.SelectRender)
    {
        image = ReadImage (icon, file, IconBase);
        if (!image)
            return FALSE;
        icon->do_Gadget.SelectRender = image;
    }

    if (icon->do_DefaultTool)
    {
        if (!(icon->do_DefaultTool = ReadIconString (icon, file, IconBase)))
            return FALSE;
    }

    if (icon->do_ToolTypes)
    {
        if (!ReadToolTypes (icon, file, IconBase))
            return FALSE;
    }

    if (icon->do_ToolWindow)
    {
        if (!(icon->do_ToolWindow = ReadIconString (icon, file, IconBase)))
            return FALSE;
    }

    if (HasNewDrawerData(icon))
    {
        if (!ReadBytes(file, &icon->do_DrawerData->dd_Flags, NEWDRAWERDATA_SIZE, IconBase))
            return FALSE;
    }

    return TRUE;
}

/****************************************************************************************/

BOOL WriteIconFile(struct DiskObject *icon, BPTR file, struct IconBase *IconBase)
{
    struct DiskObject disk;

    disk = *icon;
    disk.do_Gadget.NextGadget = NULL;
    disk.do_Gadget.GadgetText = NULL;
    disk.do_Gadget.SpecialInfo = NULL;
    disk.do_Gadget.GadgetRender = icon->do_Gadget.GadgetRender ? PRESENT : NULL;
    disk.do_Gadget.SelectRender = icon->do_Gadget.SelectRender ? PRESENT : NULL;
    disk.do_DefaultTool = icon->do_DefaultTool ? PRESENT : NULL;
    disk.do_ToolTypes = icon->do_ToolTypes ? PRESENT : NULL;
    disk.do_DrawerData = icon->do_DrawerData ? PRESENT : NULL;
    disk.do_ToolWindow = icon->do_ToolWindow ? PRESENT : NULL;

    if (!WriteBytes(file, &disk, DISKOBJECT_FILESIZE, IconBase))
        return FALSE;

    if (icon->do_DrawerData)
    {
        struct DrawerData dd;

        dd = *icon->do_DrawerData;
        dd.dd_NewWindow.FirstGadget = NULL;
        dd.dd_NewWindow.CheckMark = NULL;
        dd.dd_NewWindow.Title = NULL;
        dd.dd_NewWindow.Screen = NULL;
        dd.dd_NewWindow.BitMap = NULL;
        if (!WriteBytes(file, &dd, OLDDRAWERDATAFILESIZE, IconBase))
            return FALSE;
    }

    if (icon->do_Gadget.GadgetRender &&
        !WriteImage (file, icon->do_Gadget.GadgetRender, IconBase))
        return FALSE;

    if (icon->do_Gadget.SelectRender &&
        !WriteImage (file, icon->do_Gadget.SelectRender, IconBase))
        return FALSE;

    if (icon->do_DefaultTool &&
        !WriteIconString (file, icon->do_DefaultTool, IconBase))
        return FALSE;

    if (icon->do_ToolTypes &&
        !WriteToolTypes (file, icon->do_ToolTypes, IconBase))
        return FALSE;

    if (icon->do_ToolWindow &&
        !WriteIconString (file, icon->do_ToolWindow, IconBase))
        return FALSE;

    if (HasNewDrawerData(icon) &&
        !WriteBytes(file, &icon->do_DrawerData->dd_Flags, NEWDRAWERDATA_SIZE, IconBase))
        return FALSE;

    return TRUE;
}
