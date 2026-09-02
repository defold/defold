#!/usr/bin/env python3

import argparse
import os
import shutil
import sys
import zipfile


def encode_varint(value):
    encoded = bytearray()
    while value > 0x7f:
        encoded.append((value & 0x7f) | 0x80)
        value >>= 7
    encoded.append(value)
    return encoded


def append_length_delimited_field(message, field_number, value):
    value = value.encode("utf-8")
    message.extend(encode_varint((field_number << 3) | 2))
    message.extend(encode_varint(len(value)))
    message.extend(value)


def generate_many_valid_resources(args):
    os.makedirs(args.output_dir, exist_ok=True)

    container = bytearray()
    append_length_delimited_field(container, 1, "Many Valid References")
    for i in range(args.count):
        resource_path = "/many_valid_ref_%04d.foo" % i
        append_length_delimited_field(container, 2, resource_path)

    with open(os.path.join(args.output_dir, "many_valid_refs.cont"), "wb") as out_file:
        out_file.write(container)

    # TestResource.ResourceFoo { x: 123 }
    resource_foo = bytes([0x08, 0x7b])
    for i in range(args.count):
        with open(os.path.join(args.output_dir, "many_valid_ref_%04d.foo" % i), "wb") as out_file:
            out_file.write(resource_foo)


def generate_deep_resource_chain(args):
    os.makedirs(args.output_dir, exist_ok=True)

    container_count = args.count - 1
    for i in range(container_count):
        container = bytearray()
        append_length_delimited_field(container, 1, "Deep Resource Chain")
        if i + 1 < container_count:
            child_path = "/deep_chain_%04d.cont" % (i + 1)
        else:
            child_path = "/deep_chain_leaf.foo"
        append_length_delimited_field(container, 2, child_path)

        with open(os.path.join(args.output_dir, "deep_chain_%04d.cont" % i), "wb") as out_file:
            out_file.write(container)

    # TestResource.ResourceFoo { x: 123 }
    with open(os.path.join(args.output_dir, "deep_chain_leaf.foo"), "wb") as out_file:
        out_file.write(bytes([0x08, 0x7b]))


def add_to_zip_file(args):
    extra_entries = {}
    cwd = os.path.abspath(args.cwd)
    for path in args.inputs:
        if os.path.isdir(path):
            continue
        extra_entries[os.path.relpath(os.path.abspath(path), cwd)] = path

    output_dir = os.path.dirname(args.output)
    if output_dir:
        os.makedirs(output_dir, exist_ok=True)

    tmp_zip = args.output + ".tmp"
    with zipfile.ZipFile(args.base_zip, "r") as src_zip:
        with zipfile.ZipFile(tmp_zip, "w") as dst_zip:
            written = set()
            for info in src_zip.infolist():
                if info.filename in extra_entries or info.filename in written:
                    continue
                dst_zip.writestr(info, src_zip.read(info.filename))
                written.add(info.filename)

            for dst, src in sorted(extra_entries.items()):
                dst_zip.write(src, dst)

    os.replace(tmp_zip, args.output)


def main():
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)

    add_zip = subparsers.add_parser("add-to-zip")
    add_zip.add_argument("--base-zip", required=True)
    add_zip.add_argument("--output", required=True)
    add_zip.add_argument("--cwd", required=True)
    add_zip.add_argument("inputs", nargs="*")
    add_zip.set_defaults(func=add_to_zip_file)

    generate_many_valid = subparsers.add_parser("generate-many-valid-resources")
    generate_many_valid.add_argument("--output-dir", required=True)
    generate_many_valid.add_argument("--count", required=True, type=int)
    generate_many_valid.set_defaults(func=generate_many_valid_resources)

    generate_deep_chain = subparsers.add_parser("generate-deep-resource-chain")
    generate_deep_chain.add_argument("--output-dir", required=True)
    generate_deep_chain.add_argument("--count", required=True, type=int)
    generate_deep_chain.set_defaults(func=generate_deep_resource_chain)

    args = parser.parse_args()
    try:
        args.func(args)
    except Exception as exc:
        print(exc, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
