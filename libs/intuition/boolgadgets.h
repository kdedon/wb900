#ifndef _BOOLGADGETS_H_
#define _BOOLGADGETS_H_

/*
    Copyright (C) 1995-2003, The AROS Development Team. All rights reserved.
    Copyright (C) 2001-2003, The MorphOS Development Team. All Rights Reserved.
*/

#ifndef INTUITION_INTUITION_H
#   include <intuition/intuition.h>
#endif
#ifndef INTUITION_INTUITIONBASE_H
#   include <intuition/intuitionbase.h>
#endif

void RefreshBoolGadget (struct Gadget * gadget, struct Window * window,
                        struct Requester * requester, struct IntuitionBase * IntuitionBase);

void RefreshBoolGadgetState(struct Gadget * gadget, struct Window * window,
                            struct Requester *requester, struct IntuitionBase *IntuitionBase);

#endif /* _BOOLGADGETS_H_ */
