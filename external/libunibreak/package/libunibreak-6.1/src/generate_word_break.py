#!/usr/bin/env python3

import sys
import unicode_data_property


def output_word_break_data(leading_comment, word_break_properties):
    print('/* The content of this file is generated from:')
    print(leading_comment, end='')
    print('*/')
    print('')
    print('#include "wordbreakdef.h"')
    print('')
    print('static const struct WordBreakProperties wb_prop_default[] = {')
    for start, (end, prop) in word_break_properties.items():
        print(f"\t{{0x{start:04X}, 0x{end:04X}, WBP_{prop}}},")
    print('};')


def main():
    input_file_path = sys.argv[1] if sys.argv[1:] else \
        'WordBreakProperty.txt'
    leading_comment, word_break_properties = \
        unicode_data_property.load_data(input_file_path)
    output_word_break_data(leading_comment, word_break_properties)


if __name__ == '__main__':
    main()
