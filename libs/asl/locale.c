/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: asl's English strings
*/

/*******************************************************************************************/

#include <libraries/gadtools.h>

#include "asl_intern.h"
#include "strings.h"

/* By message number.  A menu item is its shortcut key, a NUL and its label. */
static const char *const strings[] =
{
    "Select a file",
    "Ok",
    "Volumes",
    "Parent",
    "Cancel",
    "Pattern",
    "Drawer",
    "File",
    "Drawer",
    "Assign",
    "Control",
    "L\0Last Name",
    "N\0Next Name",
    "R\0Restore",
    "P\0Parent",
    "V\0Volumes",
    "U\0Update",
    "D\0Delete...",
    "M\0Make Drawer...",
    "E\0Rename...",
    " \0Select Match...",
    "O\0Ok",
    "Q\0Cancel",
    "File List",
    "1\0Sort by Name",
    "2\0Sort by Date",
    "3\0Sort by Size",
    "<\0Ascending",
    ">\0Descending",
    " \0Drawers First",
    " \0Drawers Mixed",
    " \0Drawers Last",
    "Delete",
    "Delete \"%s\"?",
    "Delete|Cancel",
    "Make Drawer",
    "Unnamed",
    "Make",
    "Cancel",
    "Rename",
    "Rename",
    "Cancel",
    "Select Match",
    "Select",
    "Cancel",
    "Drop",
    "Only icons from this drawer can be dropped:\n%s",
    "Ok",

    "Select a font",
    "Ok",
    "Cancel",
    "Style",
    "Bold",
    "Italic",
    "Underlined",
    "Text",
    "Field",
    "Text/Field",
    "Mode",
    "Text",
    "Text+Field",
    "Complement",
    "Control",
    "L\0Last Font",
    "N\0Next Font",
    "R\0Restore",
    "U\0Rescan Disk",
    "O\0Ok",
    "Q\0Cancel",

    "Select a screen mode",
    "Ok",
    "Cancel",
    "Overscan",
    "Text Size",
    "Graphics Size",
    "Extreme Size",
    "Maximum Size",
    "Width",
    "Height",
    "Colors",
    "AutoScroll",
    "Off",
    "On",
    "Properties",
    "Not Workbench compatible",
    "Not genlockable",
    "Not draggable",
    "HAM",
    "Extra-Halfbrite",
    "Interlaced",
    "ECS only",
    "Workbench compatible",
    "Genlockable",
    "Draggable",
    "Dual-playfield priority",
    "Refresh rate",
    "Control",
    "L\0Last Mode",
    "N\0Next Mode",
    "?\0Properties",
    "R\0Restore",
    "O\0Ok",
    "Q\0Cancel"
};

/*******************************************************************************************/

STRPTR GetString(LONG id)
{
    return (STRPTR)strings[id];
}

/*******************************************************************************************/

void LocalizeMenus(struct NewMenu *nm)
{
    struct NewMenu *actnm = nm;

    for(actnm = nm; actnm->nm_Type != NM_END; actnm++)
    {
        if (actnm->nm_Label != NM_BARLABEL)
        {
            IPTR  id   = (IPTR)actnm->nm_Label;
            STRPTR str = GetString(id);

            switch(actnm->nm_Type)
            {
                case NM_TITLE:
                    actnm->nm_Label = str;
                    break;

                case NM_ITEM:
                case NM_SUB:
                    actnm->nm_Label = str + 2;
                    if (str[0] != ' ') actnm->nm_CommKey = str;
                    break;
            }

        } /* if (actnm->nm_Label != NM_BARLABEL) */

    } /* for(actnm = nm; nm->nm_Type != NM_END; nm++) */

}

/*******************************************************************************************/
