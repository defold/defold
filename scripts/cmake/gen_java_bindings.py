#!/usr/bin/env python3

import argparse
import os
import sys


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

    gen_java.generate(
        header_path=os.path.abspath(args.header),
        namespace=args.namespace,
        package_name=args.package,
        includes=[os.path.abspath(path) for path in args.include],
        java_outdir=os.path.abspath(args.java_outdir),
        jni_outdir=os.path.abspath(args.jni_outdir))


if __name__ == "__main__":
    main()
