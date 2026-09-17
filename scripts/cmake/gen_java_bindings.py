#!/usr/bin/env python3

import argparse
import glob
import os
import shutil
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


def find_first_executable(candidates):
    for candidate in candidates:
        if not candidate:
            continue
        resolved = shutil.which(candidate)
        if resolved:
            return resolved
        if os.path.isfile(candidate):
            return os.path.abspath(candidate)
    return None


def find_windows_clang():
    candidates = [
        "clang++",
        "clang-cl",
        r"C:\Program Files\LLVM\bin\clang++.exe",
        r"C:\Program Files\LLVM\bin\clang-cl.exe",
        r"C:\Program Files (x86)\LLVM\bin\clang++.exe",
        r"C:\Program Files (x86)\LLVM\bin\clang-cl.exe",
    ]

    program_files = [
        os.environ.get("ProgramFiles"),
        os.environ.get("ProgramFiles(x86)"),
    ]
    for root in program_files:
        if not root:
            continue
        candidates.extend(glob.glob(os.path.join(root, "Microsoft Visual Studio", "*", "*", "VC", "Tools", "Llvm", "x64", "bin", "clang++.exe")))
        candidates.extend(glob.glob(os.path.join(root, "Microsoft Visual Studio", "*", "*", "VC", "Tools", "Llvm", "x64", "bin", "clang-cl.exe")))

    return find_first_executable(candidates)


def ensure_clang_environment():
    if os.environ.get("CLANGPP"):
        return

    clangpp = None
    if os.name == "nt":
        clangpp = find_windows_clang()
    else:
        clangpp = find_first_executable(["clang++"])

    if not clangpp:
        return

    os.environ["CLANGPP"] = clangpp

    if not os.environ.get("CLANG"):
        clang_dir = os.path.dirname(clangpp)
        clang_name = "clang.exe" if os.name == "nt" else "clang"
        clang = find_first_executable([os.path.join(clang_dir, clang_name), "clang"])
        if clang:
            os.environ["CLANG"] = clang


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
    defold_home = os.environ.get("DEFOLD_HOME")
    if defold_home:
        append_unique(sys.path, os.path.join(defold_home, "build_tools"))

    ensure_clang_environment()

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
