# Copyright 2020-2026 The Defold Foundation
# Licensed under the Defold License version 1.0

"""Check 16 KB PT_LOAD and GNU_RELRO alignment in linked Android ELF files."""

import argparse
import subprocess
import sys


PAGE_SIZE = 16384


def check_program_headers(output):
    errors = []
    load_count = 0
    for line in output.splitlines():
        fields = line.split()
        if not fields or fields[0] not in ('LOAD', 'GNU_RELRO'):
            continue
        try:
            if len(fields) < 8:
                raise ValueError('incomplete program header')
            offset, address, memory_size, alignment = (
                int(fields[index], 16) for index in (1, 2, 5, -1))
        except ValueError:
            errors.append('Cannot parse program header: ' + line.strip())
            continue

        if fields[0] == 'LOAD':
            load_count += 1
            if alignment < PAGE_SIZE or alignment & (alignment - 1):
                errors.append('PT_LOAD alignment 0x%x must be a power of two >= 0x4000: %s'
                              % (alignment, line.strip()))
            if (address - offset) % PAGE_SIZE:
                errors.append('PT_LOAD virtual address and file offset differ modulo 0x4000: '
                              + line.strip())
        else:
            end = address + memory_size
            if end % PAGE_SIZE:
                errors.append('GNU_RELRO end 0x%x (VirtAddr + MemSiz) is not 16 KB aligned: %s'
                              % (end, line.strip()))

    if not load_count:
        errors.append('No PT_LOAD program headers found')
    return errors


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--readelf', required=True, help='Path to the NDK llvm-readelf')
    parser.add_argument('files', nargs='+', help='64-bit Android shared libraries or executables')
    args = parser.parse_args(argv)
    failed = False
    for path in args.files:
        try:
            result = subprocess.run([args.readelf, '--program-headers', '--wide', path],
                                    check=True, capture_output=True, text=True)
            errors = check_program_headers(result.stdout)
        except subprocess.CalledProcessError as error:
            errors = ['readelf failed: ' + error.stderr.strip()]
        except OSError as error:
            errors = [str(error)]
        if errors:
            failed = True
            for error in errors:
                print('%s: %s' % (path, error), file=sys.stderr)
        else:
            print('%s: 16 KB ELF alignment OK' % path)
    if failed:
        print('Rebuild with -Wl,-z,max-page-size=16384 -Wl,-z,common-page-size=16384.',
              file=sys.stderr)
    return int(failed)


if __name__ == '__main__':
    sys.exit(main())
