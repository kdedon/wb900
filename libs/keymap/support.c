/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc:
*/

/****************************************************************************************/

#include <proto/exec.h>

#include <devices/inputevent.h>
#include "keymap_intern.h"

/****************************************************************************************/

BOOL WriteToBuffer(struct BufInfo *bufinfo, UBYTE *string, LONG numchars)
{
    if (bufinfo->CharsWritten + numchars > bufinfo->BufLength)
        return (FALSE);

    CopyMem(string, bufinfo->Buffer + bufinfo->CharsWritten, numchars);
    bufinfo->CharsWritten += numchars;

    return (TRUE);
}

/****************************************************************************************/

BOOL GetKeyInfo(struct KeyInfo *ki, UWORD code, UWORD qual, struct KeyMap *km)
{
    BOOL valid = TRUE; /* valid code is default */

    if (code & IECODE_UP_PREFIX) /* Key pressed ? */
    {
        valid = FALSE;
    }
    else if (code >= 128) /* Keymaps at the moment support only 128 keys */
    {
        valid = FALSE;
    }
    else
    {
        BYTE capsable;
        BYTE repeatable;

        ki->KCFQual = KC_NOQUAL; /* == 0 */

        code &= ~IECODE_UP_PREFIX;

        /* Convert from IEQUAL_xxx into KCF_xx */
        if (qual & (IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT))
            ki->KCFQual |= KCF_SHIFT;

        if (qual & (IEQUALIFIER_LALT|IEQUALIFIER_RALT))
            ki->KCFQual |= KCF_ALT;

        if (qual & IEQUALIFIER_CONTROL)
            ki->KCFQual |= KCF_CONTROL;

        /* Get the type of the key */
        if (code <= 0x3F)
        {
            /* Get key info from low keymap */
            ki->Key_MapType = km->km_LoKeyMapTypes[code];
            ki->Key_Mapping = km->km_LoKeyMap[code];

            capsable    = GetBitProperty(km->km_LoCapsable,   code);
            repeatable  = GetBitProperty(km->km_LoRepeatable, code);
        }
        else
        {
            code -= 0x40; /* hex 40 is first indexed */
            if (code < 0x38)
            {
                /* Get key info from high keymap */
                ki->Key_MapType = km->km_HiKeyMapTypes[code];
                ki->Key_Mapping = km->km_HiKeyMap[code];
                capsable    = GetBitProperty(km->km_HiCapsable,   code);
                repeatable  = GetBitProperty(km->km_HiRepeatable, code);
            }
            else
            {
                valid = FALSE;
            }
        }

        if (valid)
        {
            if ((qual & IEQUALIFIER_REPEAT) && (!repeatable))
            {
                valid = FALSE; /* Repeating not supported for key, skip keypress */
            }
            else
            {
                if ((qual & IEQUALIFIER_CAPSLOCK) && capsable)
                    ki->KCFQual |= KCF_SHIFT;
            }
        }
    }

    return (valid);
}

/****************************************************************************************/

WORD GetDeadKeyIndex(UWORD code, UWORD qual, struct KeyMap *km)
{
    struct KeyInfo  ki;
    WORD            retval = -1;

    /* Get the key info for the key */
    if ((code < 0x78) && GetKeyInfo(&ki, code, qual, km))
    {
        if (ki.Key_MapType & KCF_DEAD)
        {
            BYTE idx;

            /* Use keymap_str table to get idx to right key descriptor */
            idx = keymapstr_table[ki.Key_MapType & KC_VANILLA][ki.KCFQual];
            if (idx != -1)
            {
                UBYTE *dead_descr = (UBYTE *)ki.Key_Mapping;

                if (dead_descr[idx * 2] == DPF_DEAD)
                {
                    /* Clear first */

                    retval = dead_descr[idx * 2 + 1];

                    retval &= 0x00FF; /* Clear upper byte */
                }
            }
        }
    }

    return (retval);
}

/****************************************************************************************/

