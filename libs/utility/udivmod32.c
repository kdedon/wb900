/*
    Copyright (C) 1995-2001, The AROS Development Team. All rights reserved.

    Desc: UDivMod32 - divide two 32 bit numbers.
*/
#include "intern.h"

/*****************************************************************************

    NAME */
#include <proto/utility.h>

        AROS_LH2(ULONG, UDivMod32,

/*  SYNOPSIS */
        AROS_LHA(ULONG, dividend, D0),
        AROS_LHA(ULONG, divisor, D1),

/*  LOCATION */
        struct Library *, UtilityBase, 26, Utility)

/*  FUNCTION
        Perform the 32 bit unsigned division and modulus of dividend by
        divisor, that is dividend / divisor. Will return both the
        quotient and the remainder.

    INPUTS
        dividend        -   The number to divide into (numerator).
        divisor         -   The number to divide by (denominator).

    RESULT
        For m68k assembly programmers,
            D0: quotient
            D1: remainder

        For HLL programmers,
            the quotient

    NOTES
        The utility.library math functions are unlike all other utility
        functions in that they don't require the library base to be
        loaded in register A6, and they also save the values of the
        address registers A0/A1.

        This function is mainly to support assembly programers, and is
        probably of limited use to higher-level language programmers.

    EXAMPLE

    BUGS
        It is impossible for C programmers to obtain the value of
        remainder.

    SEE ALSO
        SDivMod32(), SMult32(), SMult64(), UMult32(), UMult64()

    INTERNALS
        May actually be handled by code that is in config/$(KERNEL)
        This is the case for m68k-native.

    HISTORY
        29-10-95    digulla automatically created from
                            utility_lib.fd and clib/utility_protos.h

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    return dividend / divisor;

    AROS_LIBFUNC_EXIT

} /* UDivMod32 */
