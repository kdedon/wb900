/*
    Copyright (C) 2003-2026, The AROS Development Team. All rights reserved.

    Builtin icon images to fall back upon if on-disk icons cannot be opened.
*/

#include <workbench/workbench.h>
#include <workbench/icon.h>
#include <proto/icon.h>

#include "icon_intern.h"
#include "support_builtin.h"

#define ICON_DEPTH  (2)

/*
 * Two planes: '.' is pen 0 (background), '#' pen 1 (outline), '+' pen 2
 * (light) and 'o' pen 3 (accent).  A one-plane screen shows plane 0, so
 * '#' and 'o' are black there and '.' and '+' white.
 */

/*
 * ######################..
 * #oooo#++++++++++#ooo#.#.
 * #oooo#++++++###+#ooo#..#
 * #oooo#++++++###+#ooo#..#
 * #oooo#++++++###+#ooo#..#
 * #oooo#++++++###+#ooo#..#
 * #oooo#++++++++++#ooo#..#
 * #oooo############ooo#..#
 * #ooooooooooooooooooo#..#
 * #ooooooooooooooooooo#..#
 * #oo++++++++++++++++o#..#
 * #oo+##############+o#..#
 * #oo++++++++++++++++o#..#
 * #oo+##############+o#..#
 * #oo++++++++++++++++o#..#
 * #oo+##########+++++o#..#
 * #oo++++++++++++++++o#..#
 * #oo++++++++++++++++o#..#
 * #ooooooooooooooooooo#..#
 * ######################.#
 * .#.....................#
 * .#######################
 */
static const UWORD disk_data[] =
{
    0xffff, 0xfc00, 0xfc00, 0xfa00, 0xfc0e, 0xf900, 0xfc0e, 0xf900,
    0xfc0e, 0xf900, 0xfc0e, 0xf900, 0xfc00, 0xf900, 0xffff, 0xf900,
    0xffff, 0xf900, 0xffff, 0xf900, 0xe000, 0x1900, 0xefff, 0xd900,
    0xe000, 0x1900, 0xefff, 0xd900, 0xe000, 0x1900, 0xeffc, 0x1900,
    0xe000, 0x1900, 0xe000, 0x1900, 0xffff, 0xf900, 0xffff, 0xfd00,
    0x4000, 0x0100, 0x7fff, 0xff00, 0x0000, 0x0000, 0x7bff, 0x7000,
    0x7bf1, 0x7000, 0x7bf1, 0x7000, 0x7bf1, 0x7000, 0x7bf1, 0x7000,
    0x7bff, 0x7000, 0x7800, 0x7000, 0x7fff, 0xf000, 0x7fff, 0xf000,
    0x7fff, 0xf000, 0x7000, 0x3000, 0x7fff, 0xf000, 0x7000, 0x3000,
    0x7fff, 0xf000, 0x7003, 0xf000, 0x7fff, 0xf000, 0x7fff, 0xf000,
    0x7fff, 0xf000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};

/*
 * ................................
 * ...##########################...
 * ...#++++++++++++++++++++++++#...
 * ...#+######################+#...
 * ...#+#++++++++++++++++++++#+#...
 * ...#+#++++++++++++++++++++#+#...
 * ...#+#++++++++##++++++++++#+#...
 * ...#+#+++++++####+++++++++#+#...
 * ...#+#++++++##oo##++++++++#+#...
 * ...#+#++++++######++++++++#+#...
 * ...#+#++++++++++++++++++++#+#...
 * ...#+######################+#...
 * ...#++++++++++++++++++++++++#...
 * ...##########################...
 * ...#oooooooooooooooooooooooo#...
 * ...##########################...
 */
static const UWORD drawer_data[] =
{
    0x0000, 0x0000, 0x1fff, 0xfff8, 0x1000, 0x0008, 0x17ff, 0xffe8,
    0x1400, 0x0028, 0x1400, 0x0028, 0x1403, 0x0028, 0x1407, 0x8028,
    0x140f, 0xc028, 0x140f, 0xc028, 0x1400, 0x0028, 0x17ff, 0xffe8,
    0x1000, 0x0008, 0x1fff, 0xfff8, 0x1fff, 0xfff8, 0x1fff, 0xfff8,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0fff, 0xfff0, 0x0800, 0x0010,
    0x0bff, 0xffd0, 0x0bff, 0xffd0, 0x0bfc, 0xffd0, 0x0bf8, 0x7fd0,
    0x0bf3, 0x3fd0, 0x0bf0, 0x3fd0, 0x0bff, 0xffd0, 0x0800, 0x0010,
    0x0fff, 0xfff0, 0x0000, 0x0000, 0x0fff, 0xfff0, 0x0000, 0x0000
};

/*
 * ................................
 * ...##########################...
 * ...#++++++++++++++++++++++++#...
 * ...#+######################+#...
 * ...#+#....................#+#...
 * ..##########################....
 * ..#++++++++++++++++++++++++#....
 * ..#+######################+#....
 * ..#+#+++++++++##+++++++++#+#....
 * ..#+#++++++++####++++++++#+#....
 * ..#+#+++++++##oo##+++++++#+#....
 * ..#+#+++++++######+++++++#+#....
 * ..#+######################+#....
 * ..#++++++++++++++++++++++++#....
 * ..##########################....
 * ..#oooooooooooooooooooooooo#....
 * ..##########################....
 */
static const UWORD drawer_open_data[] =
{
    0x0000, 0x0000, 0x1fff, 0xfff8, 0x1000, 0x0008, 0x17ff, 0xffe8,
    0x1400, 0x0028, 0x3fff, 0xfff0, 0x2000, 0x0010, 0x2fff, 0xffd0,
    0x2803, 0x0050, 0x2807, 0x8050, 0x280f, 0xc050, 0x280f, 0xc050,
    0x2fff, 0xffd0, 0x2000, 0x0010, 0x3fff, 0xfff0, 0x3fff, 0xfff0,
    0x3fff, 0xfff0, 0x0000, 0x0000, 0x0000, 0x0000, 0x0fff, 0xfff0,
    0x0800, 0x0010, 0x0800, 0x0010, 0x0000, 0x0000, 0x1fff, 0xffe0,
    0x1000, 0x0020, 0x17fc, 0xffa0, 0x17f8, 0x7fa0, 0x17f3, 0x3fa0,
    0x17f0, 0x3fa0, 0x1000, 0x0020, 0x1fff, 0xffe0, 0x0000, 0x0000,
    0x1fff, 0xffe0, 0x0000, 0x0000
};

/*
 * ...................#....
 * ..................#+#...
 * .................#+++#..
 * ................#+++#...
 * ...............#+++#....
 * ..............#+++#.....
 * .......#####.#+++#......
 * ......#++++##+++#.......
 * .....#++++++#++#........
 * .....#+++++++##.........
 * ......#+++++++#.........
 * .......##+++++#.........
 * ........##ooo#..........
 * .......#oooo#...........
 * ......#oooo#............
 * .....#oooo#.............
 * ....#oooo#..............
 * ...#oooo#...............
 * ..#oooo#................
 * ..#ooo#.................
 * ...###..................
 */
static const UWORD tool_data[] =
{
    0x0000, 0x1000, 0x0000, 0x2800, 0x0000, 0x4400, 0x0000, 0x8800,
    0x0001, 0x1000, 0x0002, 0x2000, 0x01f4, 0x4000, 0x0218, 0x8000,
    0x0409, 0x0000, 0x0406, 0x0000, 0x0202, 0x0000, 0x0182, 0x0000,
    0x00fc, 0x0000, 0x01f8, 0x0000, 0x03f0, 0x0000, 0x07e0, 0x0000,
    0x0fc0, 0x0000, 0x1f80, 0x0000, 0x3f00, 0x0000, 0x3e00, 0x0000,
    0x1c00, 0x0000, 0x0000, 0x0000, 0x0000, 0x1000, 0x0000, 0x3800,
    0x0000, 0x7000, 0x0000, 0xe000, 0x0001, 0xc000, 0x0003, 0x8000,
    0x01e7, 0x0000, 0x03f6, 0x0000, 0x03f8, 0x0000, 0x01fc, 0x0000,
    0x007c, 0x0000, 0x0038, 0x0000, 0x00f0, 0x0000, 0x01e0, 0x0000,
    0x03c0, 0x0000, 0x0780, 0x0000, 0x0f00, 0x0000, 0x1e00, 0x0000,
    0x1c00, 0x0000, 0x0000, 0x0000
};

/*
 * ################........
 * #++++++++++++++##.......
 * #++++++++++++++#+#......
 * #+o+oo+ooo+o+++#++#.....
 * #++++++++++++++#####....
 * #+oo+o+oooo+oo++++o#....
 * #++++++++++++++++++#....
 * #+ooo+o+ooo+o+oo+++#....
 * #++++++++++++++++++#....
 * #+o+ooo+oo+o+ooo+++#....
 * #++++++++++++++++++#....
 * #+oo+oooo+o+oo+o+++#....
 * #++++++++++++++++++#....
 * #+ooo+oo+o+ooo+++++#....
 * #++++++++++++++++++#....
 * #++++++++++++++++++#....
 * ####################....
 */
static const UWORD project_data[] =
{
    0xffff, 0x0000, 0x8001, 0x8000, 0x8001, 0x4000, 0xadd1, 0x2000,
    0x8001, 0xf000, 0xb5ec, 0x3000, 0x8000, 0x1000, 0xbaeb, 0x1000,
    0x8000, 0x1000, 0xaed7, 0x1000, 0x8000, 0x1000, 0xb7ad, 0x1000,
    0x8000, 0x1000, 0xbb5c, 0x1000, 0x8000, 0x1000, 0x8000, 0x1000,
    0xffff, 0xf000, 0x0000, 0x0000, 0x7ffe, 0x0000, 0x7ffe, 0x8000,
    0x7ffe, 0xc000, 0x7ffe, 0x0000, 0x7fff, 0xe000, 0x7fff, 0xe000,
    0x7fff, 0xe000, 0x7fff, 0xe000, 0x7fff, 0xe000, 0x7fff, 0xe000,
    0x7fff, 0xe000, 0x7fff, 0xe000, 0x7fff, 0xe000, 0x7fff, 0xe000,
    0x7fff, 0xe000, 0x0000, 0x0000
};

/*
 * ......######..........
 * .....#++++++#.........
 * .####################.
 * .#++++++++++++++++++#.
 * .####################.
 * ..#+o+o+o+o+o+o+o+#...
 * ..#+o+o+o+o+o+o+o+#...
 * ..#+o+o+o+o+o+o+o+#...
 * ..#+o+o+o+o+o+o+o+#...
 * ..#+o+o+o+o+o+o+o+#...
 * ..#+o+o+o+o+o+o+o+#...
 * ..#+o+o+o+o+o+o+o+#...
 * ..#+o+o+o+o+o+o+o+#...
 * ..#+o+o+o+o+o+o+o+#...
 * ..#+o+o+o+o+o+o+o+#...
 * ...################...
 */
static const UWORD trash_data[] =
{
    0x03f0, 0x0000, 0x0408, 0x0000, 0x7fff, 0xf800, 0x4000, 0x0800,
    0x7fff, 0xf800, 0x2aaa, 0xa000, 0x2aaa, 0xa000, 0x2aaa, 0xa000,
    0x2aaa, 0xa000, 0x2aaa, 0xa000, 0x2aaa, 0xa000, 0x2aaa, 0xa000,
    0x2aaa, 0xa000, 0x2aaa, 0xa000, 0x2aaa, 0xa000, 0x1fff, 0xe000,
    0x0000, 0x0000, 0x03f0, 0x0000, 0x0000, 0x0000, 0x3fff, 0xf000,
    0x0000, 0x0000, 0x1fff, 0xc000, 0x1fff, 0xc000, 0x1fff, 0xc000,
    0x1fff, 0xc000, 0x1fff, 0xc000, 0x1fff, 0xc000, 0x1fff, 0xc000,
    0x1fff, 0xc000, 0x1fff, 0xc000, 0x1fff, 0xc000, 0x0000, 0x0000
};

/*
 * .............######...
 * ..........###++++++#..
 * .......###++++++####..
 * ....###+++++++####....
 * .###++++++++####......
 * .#+++++++####.........
 * .#######..............
 * ..#+o+o+o+o+o+o+o+#...
 * ..#+o+o+o+o+o+o+o+#...
 * ..#+o+o+o+o+o+o+o+#...
 * ..#+o+o+o+o+o+o+o+#...
 * ..#+o+o+o+o+o+o+o+#...
 * ..#+o+o+o+o+o+o+o+#...
 * ..#+o+o+o+o+o+o+o+#...
 * ..#+o+o+o+o+o+o+o+#...
 * ..#+o+o+o+o+o+o+o+#...
 * ...################...
 */
static const UWORD trash_open_data[] =
{
    0x0007, 0xe000, 0x0038, 0x1000, 0x01c0, 0xf000, 0x0e03, 0xc000,
    0x700f, 0x0000, 0x4078, 0x0000, 0x7f00, 0x0000, 0x2aaa, 0xa000,
    0x2aaa, 0xa000, 0x2aaa, 0xa000, 0x2aaa, 0xa000, 0x2aaa, 0xa000,
    0x2aaa, 0xa000, 0x2aaa, 0xa000, 0x2aaa, 0xa000, 0x2aaa, 0xa000,
    0x1fff, 0xe000, 0x0000, 0x0000, 0x0007, 0xe000, 0x003f, 0x0000,
    0x01fc, 0x0000, 0x0ff0, 0x0000, 0x3f80, 0x0000, 0x0000, 0x0000,
    0x1fff, 0xc000, 0x1fff, 0xc000, 0x1fff, 0xc000, 0x1fff, 0xc000,
    0x1fff, 0xc000, 0x1fff, 0xc000, 0x1fff, 0xc000, 0x1fff, 0xc000,
    0x1fff, 0xc000, 0x0000, 0x0000
};

/* The images of WBDISK to WBGARBAGE; no selected image: complemented */
static const struct
{
    const UWORD *normal, *selected;
    UWORD        width, height, selheight;
} builtin[] =
{
    { disk_data,    NULL,               24, 22, 0 },
    { drawer_data,  drawer_open_data,   32, 16, 17 },
    { tool_data,    NULL,               24, 21, 0 },
    { project_data, NULL,               24, 17, 0 },
    { trash_data,   trash_open_data,    22, 16, 17 }
};

BOOL __GetBuiltinImage_WB(struct Image *img, LONG type, BOOL selected, struct IconBase *IconBase)
{
    const UWORD *data;
    UWORD height;

    if (type < WBDISK || type > WBGARBAGE)
        return FALSE;

    data = builtin[type - WBDISK].normal;
    height = builtin[type - WBDISK].height;
    if (selected && builtin[type - WBDISK].selected)
    {
        data = builtin[type - WBDISK].selected;
        height = builtin[type - WBDISK].selheight;
    }

    img->LeftEdge   = 0;
    img->TopEdge    = 0;
    img->Depth      = ICON_DEPTH;
    img->Width      = builtin[type - WBDISK].width;
    img->Height     = height;
    img->PlanePick  = (1 << ICON_DEPTH) - 1;
    img->PlaneOnOff = 0;
    img->ImageData  = (UWORD *)data;
    img->NextImage  = NULL;

    return TRUE;
}

struct DiskObject *__GetBuiltinIcon_WB(LONG type, struct IconBase *IconBase)
{
    struct DiskObject  temp;
    struct Image       img1;
    struct Image       img2;
    struct DiskObject *dobj;
    struct TagItem     tags[3];

    /* Only WBDISK, WBDRAWER, WBTOOL, WBPROJECT and WBGARBAGE are supported */
    if (type < WBDISK || type > WBGARBAGE) return NULL;

    memset(&temp, 0, sizeof(temp));
    temp.do_Magic                = WB_DISKMAGIC;
    temp.do_Version              = WB_DISKVERSION;
    temp.do_Type                 = type;
    temp.do_CurrentX             = NO_ICON_POSITION;
    temp.do_CurrentY             = NO_ICON_POSITION;

    GetBuiltinImage(&img1, type, FALSE);
    temp.do_Gadget.GadgetRender  = &img1;
    temp.do_Gadget.Width         = img1.Width;
    temp.do_Gadget.Height        = img1.Height;
    temp.do_Gadget.Flags         = GFLG_GADGIMAGE | GFLG_GADGHCOMP;
    if (builtin[type - WBDISK].selected)
    {
        GetBuiltinImage(&img2, type, TRUE);
        temp.do_Gadget.SelectRender = &img2;
        temp.do_Gadget.Flags     = GFLG_GADGIMAGE | GFLG_GADGHIMAGE;
        if (img2.Height > temp.do_Gadget.Height)
            temp.do_Gadget.Height = img2.Height;
    }
    temp.do_DefaultTool          = NULL;

    tags[0].ti_Tag = ICONDUPA_DuplicateImages;      tags[0].ti_Data = TRUE;
    tags[1].ti_Tag = ICONDUPA_DuplicateImageData;   tags[1].ti_Data = TRUE;
    tags[2].ti_Tag = TAG_DONE;
    dobj = DupDiskObjectA(&temp, tags);

    return dobj;
}
