#!/usr/bin/env python3

import argparse
import glob
import os
import sys


def append_unique(paths, path):
    path = os.path.abspath(path)
    if path not in paths:
        paths.append(path)


def get_dmsdk_source_include_dirs():
    defold_home = os.environ.get("DEFOLD_HOME")
    if not defold_home:
        return []

    include_dirs = []
    for dmsdk_dir in sorted(glob.glob(os.path.join(defold_home, "engine", "*", "src", "dmsdk"))):
        append_unique(include_dirs, os.path.dirname(dmsdk_dir))
    return include_dirs


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--gen-java", required=True)
    parser.add_argument("--header", required=True)
    parser.add_argument("--namespace", required=True)
    parser.add_argument("--package", required=True)
    parser.add_argument("--java-outdir", required=True)
    parser.add_argument("--jni-outdir", required=True)
    parser.add_argument("--include", action="append", default=[])
    args = parser.parse_args()

    gen_java_dir = os.path.dirname(os.path.abspath(args.gen_java))
    sys.path.insert(0, gen_java_dir)

    import gen_java

    includes = []
    for path in args.include:
        append_unique(includes, path)
    for path in get_dmsdk_source_include_dirs():
        append_unique(includes, path)

    gen_java.generate(
        header_path=os.path.abspath(args.header),
        namespace=args.namespace,
        package_name=args.package,
        includes=includes,
        java_outdir=os.path.abspath(args.java_outdir),
        jni_outdir=os.path.abspath(args.jni_outdir))


if __name__ == "__main__":
    main()
