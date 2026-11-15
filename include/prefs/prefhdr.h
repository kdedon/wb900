#ifndef PREFS_PREFHDR_H
#define PREFS_PREFHDR_H

/*
    Copyright (C) 1995-2017, The AROS Development Team. All rights reserved.

    Desc: Prefs file header definitions
    Lang: english
*/

#ifndef EXEC_TYPES_H
#   include <exec/types.h>
#endif

#define ID_FORM (((ULONG)'F'<<24) | ((ULONG)'O'<<16) | ('R'<<8) | 'M')
#define ID_PREF (((ULONG)'P'<<24) | ((ULONG)'R'<<16) | ('E'<<8) | 'F')
#define ID_PRHD (((ULONG)'P'<<24) | ((ULONG)'R'<<16) | ('H'<<8) | 'D')

struct PrefHeader
{
    UBYTE ph_Version;
    UBYTE ph_Type;
    ULONG ph_Flags;
};

#endif /* PREFS_PREFHDR_H */
