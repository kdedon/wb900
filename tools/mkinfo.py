#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""mkinfo.py - write a classic Amiga .info icon file.

Usage: mkinfo.py OUT --type=TYPE --art=ART [--select=ART] [--tool=NAME]
                 [--tooltype=TEXT]... [--pos=X,Y] [--stack=BYTES]
                 [--window=X,Y,W,H] [--showall]

TYPE is disk, drawer, tool, project or trashcan.  ART is a text file of
pixel art, one character a pixel: '.' pen 0, '#' pen 1, '+' pen 2 and
'o' pen 3; or builtin:NAME, the art above NAME_data[] in
icon.library's built-in images.  Without --select the icon is
complemented when selected.  --window gives a drawer's or a disk's
window; --showall makes it show all files.
"""

import os
import re
import struct
import sys

PENS = {'.': 0, '#': 1, '+': 2, 'o': 3}
BUILTIN = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                       '..', 'libs', 'icon', 'support_builtin.c')
TYPES = {'disk': 1, 'drawer': 2, 'tool': 3, 'project': 4, 'trashcan': 5}
PRESENT = 0xABADCAFE
NO_POSITION = 0x80000000


def art(spec):
    if spec.startswith('builtin:'):
        src = open(BUILTIN).read()
        m = re.search(r'/\*\n((?: \* [.#+o]+\n)+) \*/\nstatic const UWORD %s_data'
                      % re.escape(spec[8:]), src)
        if not m:
            sys.exit('mkinfo: no built-in art ' + spec[8:])
        rows = [l[3:] for l in m.group(1).splitlines()]
    else:
        rows = [l.rstrip('\n') for l in open(spec) if l.strip()]
    width = max(len(r) for r in rows)
    return [r.ljust(width, '.') for r in rows]


def image(rows):
    """struct Image and its planes"""
    w, h = len(rows[0]), len(rows)
    wpr = (w + 15) // 16
    data = b''
    for plane in (0, 1):
        for r in rows:
            r = r.ljust(wpr * 16, '.')
            for k in range(wpr):
                v = 0
                for b in range(16):
                    if PENS[r[k * 16 + b]] >> plane & 1:
                        v |= 0x8000 >> b
                data += struct.pack('>H', v)
    # LeftEdge, TopEdge, Width, Height, Depth, ImageData, PlanePick,
    # PlaneOnOff, NextImage
    return struct.pack('>hhhhhIBBI', 0, 0, w, h, 2, PRESENT, 3, 0, 0) + data


def string(s):
    b = s.encode('latin-1') + b'\0'
    return struct.pack('>I', len(b)) + b


def main(argv):
    opts = {'tooltype': []}
    out = None
    for a in argv:
        if a.startswith('--'):
            k, _, v = a[2:].partition('=')
            if k == 'tooltype':
                opts[k].append(v)
            else:
                opts[k] = v
        elif out is None:
            out = a
        else:
            sys.exit(__doc__)
    if out is None or 'type' not in opts or 'art' not in opts:
        sys.exit(__doc__)
    typ = TYPES[opts['type']]
    normal = art(opts['art'])
    select = art(opts['select']) if 'select' in opts else None
    w = max(len(normal[0]), len(select[0]) if select else 0)
    h = max(len(normal), len(select) if select else 0)
    drawer = typ in (1, 2, 5)
    x, y = (NO_POSITION, NO_POSITION)
    if 'pos' in opts:
        x, y = (int(v) for v in opts['pos'].split(','))
    # GFLG_GADGIMAGE, and GFLG_GADGHIMAGE or GFLG_GADGHCOMP
    flags = 0x0004 | (0x0002 if select else 0x0000)

    # struct Gadget: NextGadget, LeftEdge, TopEdge, Width, Height,
    # Flags, Activation, GadgetType, GadgetRender, SelectRender,
    # GadgetText, MutualExclude, SpecialInfo, GadgetID, UserData
    gadget = struct.pack('>IhhhhHHHIIIiIHI', 0, 0, 0, w, h, flags, 0x0003,
                         0x0001, PRESENT, PRESENT if select else 0, 0, 0, 0,
                         0, 1 if drawer else 0)
    # struct DiskObject: Magic, Version, Gadget, Type, pad, DefaultTool,
    # ToolTypes, CurrentX, CurrentY, DrawerData, ToolWindow, StackSize
    tool = opts.get('tool')
    tts = opts['tooltype']
    do = struct.pack('>HH', 0xE310, 1) + gadget + struct.pack(
        '>BBIIIIIII', typ, 0, PRESENT if tool else 0, PRESENT if tts else 0,
        x & 0xFFFFFFFF, y & 0xFFFFFFFF, PRESENT if drawer else 0, 0,
        int(opts.get('stack', 0)))
    assert len(do) == 78
    body = do
    if drawer:
        wx, wy, ww, wh = (int(v) for v in
                          opts.get('window', '50,50,400,150').split(','))
        # struct NewWindow, then dd_CurrentX, dd_CurrentY
        body += struct.pack('>hhhhBBIIIIIIIhhHHH', wx, wy, ww, wh, 255, 255,
                            0, 0, 0, 0, 0, 0, 0, 80, 40, 0xFFFF, 0xFFFF,
                            0x0001) + struct.pack('>ii', 0, 0)
    body += image(normal)
    if select:
        body += image(select)
    if tool:
        body += string(tool)
    if tts:
        body += struct.pack('>I', (len(tts) + 1) * 4)
        for t in tts:
            body += string(t)
    if drawer:
        # dd_Flags, dd_ViewModes
        body += struct.pack('>IH', 2 if 'showall' in opts else 0, 1)
    with open(out, 'wb') as f:
        f.write(body)


if __name__ == '__main__':
    main(sys.argv[1:])
