/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.
    Copyright (C) 2001-2003, The MorphOS Development Team. All Rights Reserved.
*/

#include "intuition_intern.h"

/*****************************************************************************

    NAME */
#include <proto/intuition.h>

        AROS_LH3(BOOL, DisplayAlert,

/*  SYNOPSIS */
        AROS_LHA(ULONG , alertnumber, D0),
        AROS_LHA(UBYTE*, string, A0),
        AROS_LHA(UWORD , height, D1),

/*  LOCATION */
        struct IntuitionBase *, IntuitionBase, 15, Intuition)

/*  FUNCTION
        Bring up an alert with the given message.

    INPUTS
        alertnumber - Value determining type of alert. For historical reasons,
                 this is the same value as passed to Alert(). However,
                 this functions takes into account only AT_DeadEnd bit.
        string - A pointer to text data. Text data have the following layout:
                 each string is preceded by 3 bytes. The first two of them are
                 the X coordinates of the string in the alert display. This is
                 given as a big-endian value. The third byte is the Y
                 coordinate of the text's baseline. Then a NUL-terminated
                 string follows by itself. After the NUL terminator there's
                 one more byte. If it's not zero, another string starts from
                 the next byte. Zero marks the end of the sequence. The text
                 is always rendered using the topaz/8 font.
        height - The height of alert display in pixels.

    RESULT
        Always FALSE if AT_DeadEnd bit is set in alertnumber. Otherwise the
        function returns TRUE or FALSE depending on what user chooses: the
        left mouse button or Return for TRUE, the right one or Escape for
        FALSE.

        If the alert could not be posted for whatever reason, FALSE is
        returned.

    NOTES
        This function is obsolete and exists only for backwards compatibility
        with AmigaOS(tm). On various modern systems this function has
        different effects. On classic Amiga(tm) this function may not work
        with RTG displays, so it is generally deprecated. Please don't use it
        in new software! Use legitimate intuition requesters if you want to
        present some message to the user.

        The C900 has no alert display: the alert covers the top of the
        frontmost screen, or of a screen of its own when there is none, and
        goes to the serial console too.

    EXAMPLE

    BUGS

    SEE ALSO
        TimedDisplayAlert()

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    return TimedDisplayAlert(alertnumber, string, height, 0);

    AROS_LIBFUNC_EXIT
} /* DisplayAlert */
