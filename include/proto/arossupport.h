/*
 * The tag list helpers of AROS's arossupport link library that
 * utility.library is built on.
 */
#ifndef PROTO_AROSSUPPORT_H
#define PROTO_AROSSUPPORT_H

#include <aros/types.h>
#include <utility/tagitem.h>

struct TagItem *LibFindTagItem(Tag tagValue, const struct TagItem *tagList);
IPTR LibGetTagData(Tag tagValue, IPTR defaultVal, const struct TagItem *tagList);
struct TagItem *LibNextTagItem(struct TagItem **tagListPtr);

#endif
