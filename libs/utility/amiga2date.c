/*
    Copyright (C) 1995-2001, The AROS Development Team. All rights reserved.

    Desc: Convert the date from machine to human form.
*/
#include "intern.h"

static const ULONG Utility_DayTable[]=
{
    0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335
};

/*****************************************************************************

    NAME */
#include <utility/date.h>
#include <proto/utility.h>

        AROS_LH2(void, Amiga2Date,

/*  SYNOPSIS */
        AROS_LHA(ULONG             , seconds, D0),
        AROS_LHA(struct ClockData *, result, A0),

/*  LOCATION */
        struct Library *, UtilityBase, 20, Utility)

/*  FUNCTION
        Convert the time value given as the number of seconds since the
        1st of January 1978 (00:00:00 1.1.78), to a more useful values,
        which is easier for most people to understand. These values will
        be stored in the ClockData structure whose address is passed as
        an argument.

    INPUTS
        seconds     -   Number of seconds since 1.1.78 00:00:00
        result      -   The ClockData structure to store the information
                        in.

    RESULT
        The ClockData structure will contain the converted time values.

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS
        Some information about some constants I use:

             731 =  365 + 366, the number of days between 1.1.1978 and
                    1.1.1976. Using 1976 makes working out leap years
                    simpler.
            1461 =  The number of days in four years including 1 leap year.
                    (eg 365*3 + 366)
           86400 =  The number of seconds in one day.

        I used these as constants so that they don't have to be computed
        on the fly, or read from variables.

    HISTORY
        29-10-95    digulla automatically created from
                            utility_lib.fd and clib/utility_protos.h
        19-05-96    iaint   Wrote, with a little help from a Perl package.
        11-08-96    iaint   Updated for the new AROS format.
        17-08-96    iaint   Removed calls to unimplemented UDivMod32/UMult32
        24-02-97    iaint   Reimplemented, actually works now :)

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    LONG days;
    LONG leap, year, month;
    days = seconds / 86400;
    result->wday = days % 7;

    result->sec = seconds % 60;
    seconds /= 60;
    result->min = seconds % 60;
    seconds /= 60;
    result->hour = seconds % 24;

    /* stegerg: based on dos.library/datetostr */
    
    leap = 1;
    
    if(days<92L*365+30*366)
    {
        /*
            1976 was a leap year so use it as a base to divide the days
            into 4-year blocks (each beginning with a leap year).
        */
        days+=366+365;
        year=4*(days/(366+3*365))+1976;
        days%=(366+3*365);

        /* Now divide the 4-year blocks into single years. */
        if (days>=366)
        {
            leap=0;
            days--;
            year+=days/365;
            days%=365;
        }
    }
    else
    {
        /*
            The rule for calendar calculations are:
            1. Every year even divisible by 4 is a leap year.
            2. As an exception from rule 1 every year even divisible by
               100 is not.
            3. Every year even divisible by 400 is a leap year as an
               exception from rule 2.
            So 1996, 2000 and 2004 are leap years - 1900 and 1999 are not.

            Use 2000 as a base to devide the days into 400 year blocks,
            those into 100 year blocks and so on...
        */
        days-=17*365+5*366;
        year=400*(days/(97L*366+303L*365))+2000;
        days%=(97L*366+303L*365);

        if(days>=366)
        {
            leap=0;
            days--;
            year+=100*(days/(24L*366+76*365));
            days%=(24L*366+76*365);

            if(days>=365)
            {
                leap=1;
                days++;
                year+=4*(days/(366+3*365));
                days%=(366+3*365);

                if(days>=366)
                {
                    leap=0;
                    days--;
                    year+=days/365;
                    days%=365;
                }
            }
        }
    }
    /*
         The starting-day table assumes a leap year - so add one day if
         the date is after february 28th and the year is no leap year.
    */
    if(!leap&&days>=31+28)
        days++;

    for(month=11;;month--)
    {
        if(days>=Utility_DayTable[month])
        {
            days-=Utility_DayTable[month];
            break;
        }
    }

    /* Remember that 0 means 1.1.1978. */
    days++;
    month++;
    
    result->month = month;
    result->mday = days;
    result->year = year;

    AROS_LIBFUNC_EXIT

} /* Amiga2Date */
