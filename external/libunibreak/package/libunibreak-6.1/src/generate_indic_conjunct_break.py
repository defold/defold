#!/usr/bin/env python3

import bisect
import sys

# Leading comment of the input file
leading_comment = ""
# This dictionary will map each start point to its corresponding end point and
# InCB property
indic_conjunct_break_properties = {}
# This list will hold the starting code points of each range for binary search
indic_conjunct_break_start_points = []


def parse_indic_conjunct_break_line(line):
    # Skip comments and empty lines
    if line.startswith('#') or not line.strip() or '; InCB;' not in line:
        return None

    # Parse the valid line to extract the range and the InCB property
    range_part, _, prop = line.split(';')
    prop = prop.split('#')[0].strip()  # Clean up the InCB property string

    # Handle single code points and ranges
    if '..' in range_part:
        start_str, end_str = range_part.split('..')
        start = int(start_str, 16)
        end = int(end_str, 16)
    else:
        start = end = int(range_part.strip(), 16)

    return start, end, prop


def load_indic_conjunct_break_data(file_path):
    global leading_comment
    global indic_conjunct_break_properties
    global indic_conjunct_break_start_points

    with open(file_path, 'r', encoding='utf-8') as input_file:
        leading_comment = input_file.readline()
        leading_comment += input_file.readline()
        for line in input_file:
            parsed_data = parse_indic_conjunct_break_line(line)
            if parsed_data:
                start, end, prop = parsed_data
                indic_conjunct_break_properties[start] = (end, prop)

    # Sort by the start points
    indic_conjunct_break_properties = {
        k: v for k, v in
        sorted(indic_conjunct_break_properties.items(),
               key=lambda item: item[0])
    }
    indic_conjunct_break_start_points = list(
        indic_conjunct_break_properties.keys())


def output_indic_conjunct_break_data():
    print('/* The content of this file is generated from:')
    print(leading_comment, end='')
    print('*/')
    print('')
    print('#include "indicconjunctbreakdef.h"')
    print('')
    print('static const struct IndicConjunctBreakProperties incb_prop[] = {')
    for start, (end, prop) in indic_conjunct_break_properties.items():
        print(f"    {{0x{start:04X}, 0x{end:04X}, InCB_{prop}}},")
    print('};')


def get_indic_conjunct_break(char):
    if not indic_conjunct_break_start_points:
        load_indic_conjunct_break_data('DerivedCoreProperties.txt')
    # Find the code point of the character
    code_point = ord(char)

    # Binary search for the closest start point less than or equal to the
    # code point
    index = bisect.bisect_right(indic_conjunct_break_start_points,
                                code_point) - 1
    if index >= 0:
        start = indic_conjunct_break_start_points[index]
        end, prop = indic_conjunct_break_properties[start]
        if start <= code_point <= end:
            return prop

    # If not found, return None or a default value if you have one
    return None


def main():
    input_file_path = sys.argv[1] if sys.argv[1:] else \
        'DerivedCoreProperties.txt'
    load_indic_conjunct_break_data(input_file_path)
    output_indic_conjunct_break_data()


if __name__ == '__main__':
    main()
