/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.
*/

#include <exec/types.h>
#include <workbench/icon.h>
#include <utility/tagitem.h>
#include <proto/icon.h>

#include "icon_intern.h"

/*
 * Most tags of IconControlA() only copy a value between ti_Data and one
 * field of the library base or of the icon. Those are described by this
 * table; the tags that need more than that are handled in the function.
 */
#define ICF_NATIVE      (1 << 0)    /* field of the NativeIcon; tag is skipped without one */
#define ICF_SET         (1 << 1)    /* store ti_Data in the field, else return the field */
#define ICF_WORD        (1 << 2)    /* BOOL field, returned as a LONG */
#define ICF_PTR         (1 << 3)    /* pointer field. Neither: LONG or ULONG field */
#define ICF_ERRORCODE   (1 << 7)    /* set the error code to 0 */
#define ICF_ELSEZERO    (1 << 8)    /* without a NativeIcon: return 0 */

struct IconControlEntry
{
    UWORD ice_Tag;                  /* tag - ICONA_BASE */
    UWORD ice_Offset;               /* of the field in IconBase or NativeIcon */
    UWORD ice_Flags;
};

#define ICT(tag)        ((tag) - ICONA_BASE)

/* A field of the library base */
#define ICG_LONG(tag, field, flags) \
    { ICT(tag), offsetof(struct IconBase, field), (flags) }
#define ICG_BOOL(tag, field, flags) \
    { ICT(tag), offsetof(struct IconBase, field), (flags) | ICF_WORD }
#define ICG_PTR(tag, field, flags) \
    { ICT(tag), offsetof(struct IconBase, field), (flags) | ICF_PTR }

/* A field of the NativeIcon */
#define ICN_BOOL(tag, field, flags) \
    { ICT(tag), offsetof(struct NativeIcon, field), (flags) | ICF_NATIVE | ICF_WORD }
#define ICN_PTR(tag, field, flags) \
    { ICT(tag), offsetof(struct NativeIcon, field), (flags) | ICF_NATIVE | ICF_PTR }

static const struct IconControlEntry IconControlTable[] =
{
    /* Global tags ---------------------------------------------------------*/
    ICG_PTR (ICONCTRLA_GetGlobalScreen,             ib_Screen,              0),
    /* NOTE: Setting the precision intentionally does *not* set the error code */
    ICG_LONG(ICONCTRLA_SetGlobalPrecision,          ib_Precision,           ICF_SET),
    ICG_LONG(ICONCTRLA_GetGlobalPrecision,          ib_Precision,           0),
    ICG_BOOL(ICONCTRLA_SetGlobalFrameless,          ib_Frameless,           ICF_SET | ICF_ERRORCODE),
    ICG_BOOL(ICONCTRLA_GetGlobalFrameless,          ib_Frameless,           0),
    ICG_PTR (ICONCTRLA_SetGlobalIdentifyHook,       ib_IdentifyHook,        ICF_SET | ICF_ERRORCODE),
    ICG_PTR (ICONCTRLA_GetGlobalIdentifyHook,       ib_IdentifyHook,        0),
    ICG_LONG(ICONCTRLA_SetGlobalMaxNameLength,      ib_MaxNameLength,       ICF_SET | ICF_ERRORCODE),
    ICG_LONG(ICONCTRLA_GetGlobalMaxNameLength,      ib_MaxNameLength,       0),

    /* Local tags ----------------------------------------------------------*/
    /* NOTE: None of the local tags that set a value modifies the error code */
    ICN_BOOL(ICONCTRLA_SetFrameless,                ni_Frameless,           ICF_SET),
    ICN_BOOL(ICONCTRLA_GetFrameless,                ni_Frameless,           0),
    ICN_PTR (ICONCTRLA_GetScreen,                   ni_Screen,              ICF_ELSEZERO),
    ICN_BOOL(ICONGETA_IsDefaultIcon,                ni_IsDefault,           ICF_ELSEZERO),

    { 0, 0, 0 }
};

/*****************************************************************************

    NAME */

    AROS_LH2(ULONG, IconControlA,

/*  SYNOPSIS */
        AROS_LHA(struct DiskObject *, icon, A0),
        AROS_LHA(struct TagItem *,    tags, A1),

/*  LOCATION */
        struct IconBase *, IconBase, 26, Icon)

/*  FUNCTION
        Set and get icon and icon.library options.
        
    INPUTS
        icon - icon to be queried

    RESULT
        Number of processed tags.

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT
    
    struct TagItem       *tstate       = tags;
    struct TagItem       *tag          = NULL;
    ULONG                 processed    = 0;
    LONG                 *errorCode    = NULL;
    struct TagItem      **errorTagItem = NULL;
    struct NativeIcon    *nativeicon = NULL;

    if (icon)
    {
        nativeicon = GetNativeIcon(icon, LB(IconBase));
    }

#   define STORE(pointer, value)   (pointer != NULL ? *pointer = (value) : (value))
#   define SET_ERRORCODE(value)    do { if (errorCode) STORE(errorCode, (value)); else SetIoErr(value); } while (0)
#   define SET_ERRORTAGITEM(value) STORE(errorTagItem, (value))

    /* The following tags need to be setup early ---------------------------*/
    errorCode = (LONG *) GetTagData(ICONA_ErrorCode, 0, tags);
    errorTagItem = (struct TagItem **) GetTagData(ICONA_ErrorTagItem, 0, tags);
    SET_ERRORTAGITEM(NULL);

    /* Parse taglist -------------------------------------------------------*/
    while ((tag = NextTagItem(&tstate)) != NULL)
    {
        const struct IconControlEntry *ice;
        ULONG t = tag->ti_Tag - ICONA_BASE;

        if (tag->ti_Tag == OBP_Precision)
            t = ICT(ICONCTRLA_SetGlobalPrecision);

        /* Tags that copy a value from or to a field -----------------------*/
        for (ice = IconControlTable; ice->ice_Tag != 0; ice++)
        {
            if (ice->ice_Tag == t)
                break;
        }

        if (ice->ice_Tag != 0)
        {
            ULONG flags = ice->ice_Flags;
            UBYTE *object = (flags & ICF_NATIVE) ? (UBYTE *)nativeicon : (UBYTE *)LB(IconBase);

            if (object != NULL)
            {
                APTR field = object + ice->ice_Offset;

                if (flags & ICF_SET)
                {
                    if (flags & ICF_PTR)
                        *(IPTR *)field = tag->ti_Data;
                    else if (flags & ICF_WORD)
                        *(BOOL *)field = tag->ti_Data;
                    else
                        *(LONG *)field = tag->ti_Data;

                    if (flags & ICF_ERRORCODE)
                        SET_ERRORCODE(0);
                }
                else if (flags & ICF_PTR)
                {
                    STORE((IPTR *)tag->ti_Data, *(IPTR *)field);
                }
                else
                {
                    STORE((LONG *)tag->ti_Data, (flags & ICF_WORD) ? *(BOOL *)field : *(LONG *)field);
                }
                processed++;
            }
            else if (flags & ICF_ELSEZERO)
            {
                if (flags & ICF_PTR)
                    STORE((IPTR *)tag->ti_Data, 0);
                else
                    STORE((LONG *)tag->ti_Data, 0);
                processed++;
            }
            continue;
        }

        switch (t)
        {
            /* Global tags -------------------------------------------------*/
            case ICT(ICONCTRLA_SetGlobalScreen):
                if (LB(IconBase)->ib_Screen != (struct Screen *) tag->ti_Data)
                {
                    if ((struct Screen *) tag->ti_Data == NULL)
                    {
                        LB(IconBase)->ib_Screen = NULL;
                        /* NOTE: This intentionally does *not* set the error code */
                        processed++;
                    } else if (LB(IconBase)->ib_Screen == NULL)
                    {
                        LB(IconBase)->ib_Screen = (struct Screen *) tag->ti_Data;
                        /* NOTE: This intentionally does *not* set the error code */
                        processed++;
                    }
                }
                break;

            case ICT(ICONCTRLA_SetGlobalEmbossRect):
                CopyMem((struct Rectangle *)tag->ti_Data, &LB(IconBase)->ib_EmbossRectangle, sizeof(struct Rectangle));
                processed++;
                SET_ERRORCODE(0);
                break;

            case ICT(ICONCTRLA_GetGlobalEmbossRect):
                CopyMem(&LB(IconBase)->ib_EmbossRectangle, (struct Rectangle *)tag->ti_Data, sizeof(struct Rectangle));
                processed++;
                break;

            /* Local tags --------------------------------------------------*/
            case ICT(ICONCTRLA_GetWidth):
                if (nativeicon && nativeicon->ni_Screen)
                {
                    STORE((ULONG *)tag->ti_Data, nativeicon->ni_Width);
                    processed++;
                }
                break;

            case ICT(ICONCTRLA_GetHeight):
                if (nativeicon && nativeicon->ni_Screen)
                {
                    STORE((ULONG *)tag->ti_Data, nativeicon->ni_Height);
                    processed++;
                }
                break;

            case ICT(ICONCTRLA_IsPaletteMapped):
            case ICT(ICONCTRLA_IsNewIcon):
                /* Palette mapped icons and NewIcons are not supported */
                STORE((LONG *)tag->ti_Data, 0);
                processed++;
                break;

            case ICT(ICONCTRLA_IsNativeIcon):
                STORE((LONG *)tag->ti_Data, nativeicon ? 1 : 0);
                processed++;
                break;

            case ICT(ICONCTRLA_HasRealImage2):
                STORE((LONG *)tag->ti_Data,
                    icon && icon->do_Gadget.SelectRender &&
                    (icon->do_Gadget.Flags & GFLG_GADGHIGHBITS) == GFLG_GADGHIMAGE);
                processed++;
                break;

            default:
                SET_ERRORTAGITEM(tag);
                break;
        }
    }

    return processed;
    
    AROS_LIBFUNC_EXIT
    
#   undef STORE
#   undef SET_ERRORCODE
#   undef SET_ERRORTAGITEM
} /* IconControlA() */
