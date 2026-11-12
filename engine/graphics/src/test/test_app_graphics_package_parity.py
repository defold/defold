#!/usr/bin/env python3
"""Regenerate the raster parity SPIR-V header: script <path-to-glslang>."""
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parent
header = ('// Generated from parity.vert and parity.frag using glslang -V.\n'
          '// Regenerate: python test_app_graphics_package_parity.py <path-to-glslang>\n'
          '#pragma once\n')
with tempfile.TemporaryDirectory() as temporary:
    for stage in ('vert', 'frag'):
        output = Path(temporary) / (stage + '.spv')
        subprocess.run([sys.argv[1], '-V', str(root / ('parity.' + stage)), '-o', str(output)], check=True)
        data = output.read_bytes()
        header += 'alignas(4) static const unsigned char parity_' + stage + '[] = {\n'
        for offset in range(0, len(data), 16):
            header += '    ' + ','.join('0x%02x' % value for value in data[offset:offset+16]) + ',\n'
        header += '};\n'
(root / 'test_app_graphics_parity.h').write_text(header, encoding='utf-8', newline='\n')
