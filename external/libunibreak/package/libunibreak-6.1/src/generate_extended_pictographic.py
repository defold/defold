#!/usr/bin/env python3

import sys
import unicode_data_property


def output_extended_pictographic_data(leading_comment, emoji_data):
    print('/* The content of this file is generated from:')
    print(leading_comment, end='')
    print('*/')
    print('')
    print('#include "emojidef.h"')
    print('')
    print('static const struct ExtendedPictograpic ep_prop[] = {')
    for start, (end, prop) in emoji_data.items():
        if prop == 'Extended_Pictographic':
            print(f"\t{{0x{start:04X}, 0x{end:04X}}},")
    print('};')


def main():
    input_file_path = sys.argv[1] if sys.argv[1:] else 'emoji-data.txt'
    leading_comment, emoji_data = \
        unicode_data_property.load_data(input_file_path)
    output_extended_pictographic_data(leading_comment, emoji_data)


if __name__ == '__main__':
    main()
