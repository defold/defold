#!/usr/bin/env python3
# Copyright 2020-2026 The Defold Foundation
# Copyright 2014-2020 King
# Copyright 2009-2014 Ragnar Svensson, Christian Murray
# Licensed under the Defold License version 1.0 (the "License"); you may not use
# this file except in compliance with the License.

"""Merge extender configuration fragments.

The public extender inputs are always present. Vendor/private fragments are
optional and, when present, are appended after the public input to match the
legacy Waf merge task.
"""

import argparse
import os


def merge(output, required_inputs, optional_inputs):
    os.makedirs(os.path.dirname(output), exist_ok=True)
    inputs = list(required_inputs)
    inputs.extend(path for path in optional_inputs if os.path.exists(path))

    with open(output, 'wb') as out_f:
        for path in inputs:
            with open(path, 'rb') as in_f:
                out_f.write(in_f.read())


def main():
    parser = argparse.ArgumentParser(description='Merge extender configuration fragments.')
    parser.add_argument('--output', required=True, help='Output file to write.')
    parser.add_argument('--input', action='append', default=[], required=True, help='Required input file. May be repeated.')
    parser.add_argument('--optional-input', action='append', default=[], help='Optional input file. May be repeated.')
    args = parser.parse_args()

    merge(args.output, args.input, args.optional_input)


if __name__ == '__main__':
    main()
