#!/usr/bin/env python3


def parse_data_line(line):
    # Skip comments and empty lines
    if line.startswith('#') or not line.strip():
        return None

    # Parse the valid line to extract the range and the property
    range_part, prop = line.split(';')
    prop = prop.split('#')[0].strip()  # Clean up the property string

    # Handle single code points and ranges
    if '..' in range_part:
        start_str, end_str = range_part.split('..')
        start = int(start_str, 16)
        end = int(end_str, 16)
    else:
        start = end = int(range_part.strip(), 16)

    return start, end, prop


def load_data(file_path):
    result = {}

    with open(file_path, 'r', encoding='utf-8') as input_file:
        leading_comment = input_file.readline()
        leading_comment += input_file.readline()
        last_start = -1
        last_end = -1
        last_prop = ''
        for line in input_file:
            parsed_data = parse_data_line(line)
            if parsed_data:
                start, end, prop = parsed_data
                if last_end + 1 == start and last_prop == prop:
                    result[last_start] = (end, prop)
                    last_end = end
                else:
                    result[start] = (end, prop)
                    last_start, last_end, last_prop = start, end, prop

    # Sort by the start points
    result = {
        k: v for k, v in
        sorted(result.items(), key=lambda item: item[0])
    }
    return leading_comment, result
