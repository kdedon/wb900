/*
    Copyright (C) 1995-2014, The AROS Development Team. All rights reserved.
*/

#include <proto/arossupport.h>

struct TagItem *LibFindTagItem(Tag tagValue, const struct TagItem *tagList)
{
    register const struct TagItem *t = tagList;
    register ULONG v;

    /* LibNextTagItem's walk, inline: the control tags are 0 to 3 */
    while (t)
    {
        v = t->ti_Tag;
        if (v > TAG_SKIP)
        {
            if (v == (ULONG)tagValue)
                return (struct TagItem *)t;
            t++;
        }
        else if (v == TAG_DONE)
            return NULL;
        else if (v == TAG_MORE)
            t = (const struct TagItem *)t->ti_Data;
        else if (v == TAG_SKIP)
            t += t->ti_Data + 1;
        else
            t++;
    }

    return NULL;
}
