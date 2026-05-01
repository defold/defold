#!/usr/bin/env python3

import sys
import unicode_data_property


def output_east_asian_width_data(leading_comment,
                                 east_asian_width_properties,
                                 skip=''):
    print('/* The content of this file is generated from:')
    print(leading_comment, end='')
    print('*/')
    print('')
    print('#include "eastasianwidthdef.h"')
    print('')
    print('static const struct EastAsianWidthProperties eaw_prop[] = {')
    for start, (end, prop) in east_asian_width_properties.items():
        if prop == skip:
            continue
        print(f"    {{0x{start:04X}, 0x{end:04X}, EAW_{prop}}},")
    print('};')


def main():
    input_file_path = sys.argv[1] if sys.argv[1:] else \
        'EastAsianWidth.txt'
    leading_comment, east_asian_width_properties = \
        unicode_data_property.load_data(input_file_path)
    output_east_asian_width_data(
        leading_comment, east_asian_width_properties, skip='N')


if __name__ == '__main__':
    main()
