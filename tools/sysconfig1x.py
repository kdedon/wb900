#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""sysconfig1x.py OUT: write DEVS:system-configuration for the Workbench
1.x style.

The file is intuition's struct Preferences (232 bytes, big-endian, in
the layout the Z8001 compiler gives <intuition/preferences.h>), holding
intuition's defaults except for the Workbench 1.0 colours and the
OLD_LOOK flag in EnableCLI.  intuition.library passes it to SetPrefs()
when it is first opened.
"""
import struct
import sys

SIZE = 232
MOUSE_ACCEL = 1 << 15
OLD_LOOK = 1 << 13

# The Workbench 1.0 colours: blue, white, black, orange
COLORS = (0x05A, 0xFFF, 0x002, 0xF80)

# intuition's default pointer: a white arrow with a black rim all round
POINTER = (
    0x0000, 0x000c,
    0x0000, 0x8000, 0x0000, 0xc000, 0x4000, 0xe000, 0x6000, 0xf000,
    0x7000, 0xf800, 0x7800, 0xfc00, 0x7c00, 0xfe00, 0x7e00, 0xff00,
    0x7f00, 0xff80, 0x7f80, 0xffc0, 0x7c00, 0xffc0, 0x6c00, 0xfe00,
    0x4600, 0xef00, 0x0600, 0xcf00, 0x0300, 0x0780, 0x0000, 0x0300,
    0x0000, 0x0000,
)


def prefs():
    p = bytearray(SIZE)
    # FontHeight, PrinterPort, BaudRate (BAUD_9600)
    struct.pack_into('>bBH', p, 0, 8, 0, 5)
    # KeyRptSpeed, KeyRptDelay, DoubleClick
    struct.pack_into('>6L', p, 4, 0, 40000, 0, 500000, 0, 500000)
    struct.pack_into('>36H', p, 28, *POINTER)
    # XOffset, YOffset, color17-19, PointerTicks
    struct.pack_into('>bb4H', p, 100, 0, 0, 0x0e44, 0x0000, 0x0eec, 2)
    struct.pack_into('>4H', p, 110, *COLORS)
    struct.pack_into('>H', p, 124, MOUSE_ACCEL | OLD_LOOK)
    p[128:135] = b'generic'
    # PrintLeftMargin, PrintRightMargin, PrintImage, PrintAspect,
    # PrintShade, PrintThreshold, PaperSize, PaperLength
    struct.pack_into('>8H', p, 164, 5, 75, 0, 0, 1, 2, 32, 66)
    # wb_Width, wb_Height, wb_Depth
    struct.pack_into('>HHB', p, 226, 1024, 800, 1)
    return bytes(p)


if __name__ == '__main__':
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    with open(sys.argv[1], 'wb') as f:
        f.write(prefs())
