#!/usr/bin/env python3

import argparse
import os
import shutil
import sys
import zipfile


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

    args = parser.parse_args()
    try:
        args.func(args)
    except Exception as exc:
        print(exc, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
