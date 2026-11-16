#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""mkfont.py - write a classic Amiga bitmap font from text sources.

Usage: mkfont.py OUTDIR SRC...

Each SRC is one size of a font.  All must name the same font; mkfont
writes its contents file OUTDIR/NAME.font and a font hunk file
OUTDIR/NAME/YSIZE for each SRC.

A source holds keyword lines, then the glyphs:

    name fixed          the font's name, without .font
    ysize 8             rows of every glyph
    baseline 6          row of the baseline, from 0 at the top
    boldsmear 1         (optional, default 1)
    fixed 8             every glyph this wide; else proportional
    glyph 0x41 A        character code, or "default"; the rest is a note
    ..##....            ysize rows, '#' set; a row's length is the
                        glyph's width and its advance

'#' starts a comment outside glyph rows.  A code between the lowest and
highest with no glyph gets the default glyph, which every font needs.
"""

import os
import struct
import sys

HUNK_HEADER, HUNK_CODE, HUNK_RELOC32, HUNK_END = 0x3F3, 0x3E9, 0x3EC, 0x3F2
NT_FONT = 12
DFH_ID, FCH_ID = 0x0F80, 0x0F00
MAXFONTNAME, MAXFONTPATH = 32, 256
FPF_DISKFONT, FPF_PROPORTIONAL, FPF_DESIGNED = 0x02, 0x20, 0x40
# The hunk's first long, a Z8000 RET and NOP, runs if the file is run
STUB = struct.pack('>HH', 0x9E08, 0x8D07)


def fail(src, n, what):
    sys.exit('mkfont: %s:%d: %s' % (src, n, what))


def parse(src):
    f = {'boldsmear': 1, 'fixed': None, 'glyphs': {}}
    code = rows = None
    for n, line in enumerate(open(src), 1):
        line = line.rstrip('\n')
        if rows is not None:
            if len(line) == 0 or line.strip('.#') != '':
                fail(src, n, 'bad glyph row')
            rows.append(line)
            if len(rows) == f['ysize']:
                f['glyphs'][code] = rows
                rows = None
            continue
        words = line.split('#', 1)[0].split()
        if not words:
            continue
        key = words[0]
        if key == 'name':
            f['name'] = words[1]
        elif key in ('ysize', 'baseline', 'boldsmear', 'fixed'):
            f[key] = int(words[1], 0)
        elif key == 'glyph':
            if 'ysize' not in f:
                fail(src, n, 'glyph before ysize')
            code = 'default' if words[1] == 'default' else int(words[1], 0)
            if code in f['glyphs']:
                fail(src, n, 'glyph given twice')
            rows = []
        else:
            fail(src, n, 'unknown keyword ' + key)
    if rows is not None:
        fail(src, n, 'short glyph')
    for k in ('name', 'ysize', 'baseline'):
        if k not in f:
            sys.exit('mkfont: %s: no %s' % (src, k))
    if 'default' not in f['glyphs']:
        sys.exit('mkfont: %s: no default glyph' % src)
    for code, rows in f['glyphs'].items():
        if len(set(map(len, rows))) != 1 or \
           (f['fixed'] and len(rows[0]) != f['fixed']):
            sys.exit('mkfont: %s: glyph %s: rows of unequal width' %
                     (src, code))
    return f


def fonthunk(f):
    """The font's hunk: the stub, a DiskFontHeader and its tables"""
    g = f['glyphs']
    lo = min(c for c in g if c != 'default')
    hi = max(c for c in g if c != 'default')
    order = [g.get(c, g['default']) for c in range(lo, hi + 1)] + [g['default']]
    prop = f['fixed'] is None
    if prop:
        xsize = round(sum(len(r[0]) for r in order) / len(order))
    else:
        xsize = f['fixed']

    # Glyphs side by side in one bitmap; a fixed font's from byte edges
    bit, locs = 0, []
    for rows in order:
        w = len(rows[0])
        locs.append((bit, w))
        bit += 8 * ((w + 7) // 8) if not prop else w
    modulo = (bit + 15) // 16 * 2
    data = bytearray(modulo * f['ysize'])
    for (pos, w), rows in zip(locs, order):
        for y, r in enumerate(rows):
            for x, c in enumerate(r):
                if c == '#':
                    b = pos + x
                    data[y * modulo + b // 8] |= 0x80 >> (b & 7)

    # Offsets in the hunk
    dfh = len(STUB)
    name = dfh + 22
    tf = name + MAXFONTNAME
    chardata = tf + 52
    charloc = chardata + len(data)
    charspace = charloc + 4 * len(order)
    end = charspace + (2 * len(order) if prop else 0)

    fname = (f['name'] + '.font').encode()
    if len(fname) >= MAXFONTNAME:
        sys.exit('mkfont: name too long')
    flags = FPF_DISKFONT | FPF_DESIGNED | (FPF_PROPORTIONAL if prop else 0)
    h = bytearray(STUB)
    # dfh_DF, dfh_FileID, dfh_Revision, dfh_Segment, dfh_Name
    h += struct.pack('>IIBbIHHI', 0, 0, NT_FONT, 0, name, DFH_ID, 1, 0)
    h += fname.ljust(MAXFONTNAME, b'\0')
    # tf_Message, then the TextFont's fields
    h += struct.pack('>IIBbIIH', 0, 0, NT_FONT, 0, name, 0, end - dfh)
    h += struct.pack('>HBBHHHHBBIHII', f['ysize'], 0, flags, xsize,
                     f['baseline'], f['boldsmear'], 0, lo, hi, chardata,
                     modulo, charloc, charspace if prop else 0)
    h += struct.pack('>I', 0)                       # tf_CharKern
    h += data
    for pos, w in locs:
        h += struct.pack('>HH', pos, w)
    if prop:
        for rows in order:
            h += struct.pack('>h', len(rows[0]))
    h += b'\0' * (-len(h) % 4)
    relocs = [dfh + 10, tf + 10, tf + 34, tf + 40]
    if prop:
        relocs.append(tf + 44)

    out = struct.pack('>IIIIII', HUNK_HEADER, 0, 1, 0, 0, len(h) // 4)
    out += struct.pack('>II', HUNK_CODE, len(h) // 4) + bytes(h)
    out += struct.pack('>III', HUNK_RELOC32, len(relocs), 0)
    out += b''.join(struct.pack('>I', r) for r in relocs)
    out += struct.pack('>II', 0, HUNK_END)
    return out, flags


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    outdir = sys.argv[1]
    fonts = [parse(s) for s in sys.argv[2:]]
    name = fonts[0]['name']
    if any(f['name'] != name for f in fonts):
        sys.exit('mkfont: sources name different fonts')
    os.makedirs(os.path.join(outdir, name), exist_ok=True)
    contents = struct.pack('>HH', FCH_ID, len(fonts))
    for f in sorted(fonts, key=lambda f: f['ysize']):
        hunk, flags = fonthunk(f)
        path = '%s/%d' % (name, f['ysize'])
        with open(os.path.join(outdir, path), 'wb') as o:
            o.write(hunk)
        contents += path.encode().ljust(MAXFONTPATH, b'\0')
        contents += struct.pack('>HBB', f['ysize'], 0, flags)
    with open(os.path.join(outdir, name + '.font'), 'wb') as o:
        o.write(contents)


if __name__ == '__main__':
    main()
