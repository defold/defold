#!/usr/bin/env python
from __future__ import print_function
import sys

ranges = []
last_first = None
last_second = None
last_value = None
for line in sys.stdin:
    first, second = line.rstrip().split('..')
    second, value = second.split(';')
    value = value.lstrip()
    first = int(first, 16)
    second = int(second, 16)
    if last_value is None:
        last_first = first
        last_second = second
        last_value = value
    elif last_second + 1 != first or last_value != value:
        ranges.append((last_first, last_second, last_value))
        last_first = first
        last_second = second
        last_value = value
    else:
        last_second = second

ranges.append((last_first, last_second, last_value))

# print ranges for supplementary planes

BMP = 65536

sp = [r for r in ranges if r[0] >= BMP]

print("""*/

#include "linebreakdef.h"

/** Line breaking properties for supplementary planes */
const struct LineBreakProperties lb_prop_supplementary[] = {""")

for first, second, value in sp:
    if first >= BMP:
        print("\t{{ 0x{:X}, 0x{:X}, LBP_{} }},".format(first, second, value))

print("""\t{ 0xFFFFFFFF, 0xFFFFFFFF, LBP_Undefined }
};

const unsigned int lb_prop_supplementary_len =
    sizeof(lb_prop_supplementary) / sizeof(lb_prop_supplementary[0]);
""")

# print BMP table

print("""/** Line breaking properties for BMP */
const char lb_prop_bmp[] = {""")

bmp = ['XX'] * BMP
for first, second, value in ranges:
    if first >= BMP:
        break
    bmp[first:second + 1] = [value] * (second - first + 1)


def chunks(arr, n):
    for i in range(0, len(arr), n):
        yield i, arr[i:i + n]


for i, chunk in chunks(bmp, 8):
    line = ', '.join(['LBP_{}'.format(value) for value in chunk])
    print('\t', line, sep='', end=',\n')
print("};")
