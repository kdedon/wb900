#ifndef PREFS_SCREENMODE_H
#define PREFS_SCREENMODE_H

/*
    Copyright (C) 1995-2017, The AROS Development Team. All rights reserved.

    Desc: Screenmode prefs definitions
    Lang: english
*/

#ifndef EXEC_TYPES_H
#   include <exec/types.h>
#endif

#define ID_SCRM (((ULONG)'S'<<24) | ((ULONG)'C'<<16) | ('R'<<8) | 'M')

struct ScreenModePrefs
{
    ULONG smp_Reserved[4];
    ULONG smp_DisplayID;
    UWORD smp_Width;
    UWORD smp_Height;
    UWORD smp_Depth;
    UWORD smp_Control;
};

#define SMB_AUTOSCROLL 0

#define SMF_AUTOSCROLL (1 << SMB_AUTOSCROLL)

#endif /* PREFS_SCREENMODE_H */
