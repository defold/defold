#!/usr/bin/env python
# Copyright 2020-2026 The Defold Foundation
# Licensed under the Defold License version 1.0

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
import zipfile
from pathlib import Path


ALL_TARGET_FORMATS = ["sdoc", "json", "script_api", "lua"]
SOURCE_EXTENSIONS = {".h", ".hpp", ".cpp", ".cs", ".c", ".doc_h", ".proto", ".apidoc"}


def log(message):
    print("[build_docs] %s" % message, flush=True)


def format_duration(seconds):
    return "%.2f s" % seconds


def timed(name, fn):
    start_time = time.perf_counter()
    log("%s..." % name)
    result = fn()
    log("%s completed in %s" % (name, format_duration(time.perf_counter() - start_time)))
    return result


def run_command(args, cwd=None, env=None, echo=True):
    if echo:
        log("Running: %s" % " ".join(args))
    subprocess.check_call(args, cwd=cwd, env=env)


def build_python_env(pythonpath):
    env = os.environ.copy()
    paths = [path for path in pythonpath if path]
    if env.get("PYTHONPATH"):
        paths.append(env["PYTHONPATH"])
    if paths:
        env["PYTHONPATH"] = os.pathsep.join(paths)
    return env


def add_python_paths(paths):
    for path in reversed([os.path.abspath(path) for path in paths if path]):
        if path not in sys.path:
            sys.path.insert(0, path)


def write_if_changed(path, data, mode="w", encoding="utf-8"):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)

    if "b" in mode:
        old_data = path.read_bytes() if path.exists() else None
    else:
        old_data = path.read_text(encoding=encoding) if path.exists() else None

    if old_data == data:
        return False

    fd, tmp = tempfile.mkstemp(prefix=".%s." % path.name, dir=str(path.parent))
    tmp_path = Path(tmp)
    try:
        if "b" in mode:
            with os.fdopen(fd, mode) as out_file:
                out_file.write(data)
        else:
            with os.fdopen(fd, mode, encoding=encoding) as out_file:
                out_file.write(data)
        os.replace(str(tmp_path), str(path))
    finally:
        if tmp_path.exists():
            tmp_path.unlink()
    return True


def write_stamp(path):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("ok\n", encoding="utf-8")


def write_editor_docs(defold_root, output):
    editor_dir = os.path.join(defold_root, "editor")
    bundle_py = os.path.join(editor_dir, "scripts", "bundle.py")
    temp_dir = tempfile.mkdtemp(prefix="defold-editor-docs.")
    try:
        run_command([sys.executable, bundle_py, "docs", "--docs-dir", temp_dir], cwd=editor_dir)
        source = os.path.join(temp_dir, "editor.apidoc")
        write_if_changed(output, Path(source).read_text(encoding="utf-8"))
    finally:
        shutil.rmtree(temp_dir)


def strip_comment_stars(value):
    lines = value.split("\n")
    result = []
    for line in lines:
        line = line.strip()
        if line.startswith("*"):
            line = line[1:]
            if line.startswith(" "):
                line = line[1:]
        result.append(line)
    return "\n".join(result)


def parse_comment(source):
    stripped = strip_comment_stars(source)
    matches = re.findall(r"^\s*@(\S+) *((?:[^@]|(?<!\n)@)*)", stripped, re.MULTILINE)
    comment = {
        "is_document": False,
        "namespace": None,
        "path": None,
    }
    for tag, value in matches:
        tag = tag.strip()
        value = value.strip()
        if tag == "document":
            comment["is_document"] = True
        else:
            comment[tag] = value
    return comment


def parse_source(source_path, defold_root):
    elements = {}
    resource_path = os.path.abspath(source_path)
    resource_file = os.path.basename(resource_path)
    relative_path = os.path.relpath(resource_path, defold_root)

    with open(resource_path, encoding="utf8") as in_file:
        source = in_file.read()

    default_namespace = None
    for comment_str in re.findall(r"/(\*#.*?)\*/", source, re.DOTALL):
        comment = parse_comment(comment_str)
        namespace = comment.get("namespace")
        if comment["is_document"]:
            comment_path = comment.get("path")
            if not comment_path:
                print("Missing @path in '%s', adding '%s'" % (resource_path, relative_path))
                comment_str = comment_str + ("* @path %s\n" % relative_path)
            else:
                print("Replacing @path in '%s' with '%s'" % (resource_path, relative_path))
                comment_str = comment_str.replace("@path " + comment_path, "@path " + relative_path)

            comment_file = comment.get("file")
            if not comment_file:
                print("Missing @file in '%s', adding '%s'" % (resource_path, resource_file))
                comment_str = comment_str + ("* @file %s\n" % resource_file)
            elif comment_file != resource_file:
                print("Replacing @file in '%s' with '%s'" % (resource_path, resource_file))
                comment_str = comment_str.replace("@file " + comment_file, "@file " + resource_file)

            if not comment.get("language"):
                print("Missing @language in %s, assuming C++" % resource_path)
                comment_str = comment_str + "* @language C++\n"

            if namespace:
                default_namespace = namespace

        if not namespace:
            namespace = default_namespace
            comment["namespace"] = default_namespace

        key = namespace if namespace else resource_path
        elements.setdefault(key, []).append("/" + comment_str + "*/")

    return elements


def extract_source(defold_root, source, output):
    docs = []
    for values in parse_source(source, defold_root).values():
        docs.extend(values)
    write_if_changed(output, "\n".join(docs))


def convert_apidoc(docs_dir, apidoc, output_dir, key, formats, pythonpath):
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    doc_str = Path(apidoc).read_text(encoding="utf-8")
    output_specs = [(target_format, str(output_dir / ("%s_doc.%s" % (key, target_format)))) for target_format in formats]

    if "/*#" not in doc_str:
        for _, output in output_specs:
            write_if_changed(output, "")
        log("Skipped empty API doc: %s" % key)
        return

    add_python_paths([docs_dir] + pythonpath)
    import script_doc

    script_doc.write_formats(doc_str, output_specs)
    log("Converted API doc %s to %d formats" % (key, len(output_specs)))


def read_manifest(path):
    entries = []
    for line in Path(path).read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line:
            continue
        source, target_name = line.split("|", 1)
        entries.append((Path(source), target_name))
    return entries


def sync_outputs(manifest, output_dir, all_formats, stamp):
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    entries = read_manifest(manifest)
    expected_names = {target_name for _, target_name in entries}
    installed_count = 0

    for source, target_name in entries:
        target = output_dir / target_name
        if source.exists() and source.stat().st_size > 0:
            target.parent.mkdir(parents=True, exist_ok=True)
            source_bytes = source.read_bytes()
            if not target.exists() or target.read_bytes() != source_bytes:
                write_if_changed(target, source_bytes, mode="wb")
            installed_count += 1
        elif target.exists():
            target.unlink()

    for target in output_dir.glob("*_doc.*"):
        suffix = target.name.rsplit(".", 1)[-1]
        if suffix in all_formats and target.name not in expected_names:
            target.unlink()

    write_stamp(stamp)
    log("Installed %d non-empty API docs into %s" % (installed_count, output_dir))


def zip_tree(path, outfile, directory):
    outfile = Path(outfile)
    outfile.parent.mkdir(parents=True, exist_ok=True)
    temp = outfile.with_name(".%s.tmp" % outfile.name)
    file_count = 0
    with zipfile.ZipFile(str(temp), "w") as archive:
        for root, dirs, files in os.walk(path):
            dirs.sort()
            for name in sorted(files):
                filepath = os.path.join(root, name)
                archive.write(filepath, os.path.relpath(filepath, directory))
                file_count += 1
    os.replace(str(temp), str(outfile))
    log("Wrote %s with %d files" % (outfile, file_count))


def run_tests(docs_dir, pythonpath, stamp):
    env = build_python_env([docs_dir] + [os.path.abspath(path) for path in pythonpath])
    run_command([sys.executable, os.path.join(docs_dir, "test_script_doc.py")], cwd=docs_dir, env=env)
    write_stamp(stamp)


def parse_formats(values):
    formats = []
    for value in values:
        for target_format in value.replace(",", ";").split(";"):
            target_format = target_format.strip()
            if target_format:
                formats.append(target_format)
    unknown = sorted(set(formats) - set(ALL_TARGET_FORMATS))
    if unknown:
        raise SystemExit("Unknown docs format(s): %s" % ", ".join(unknown))
    return formats


def main():
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)

    editor = subparsers.add_parser("editor")
    editor.add_argument("--defold-root", required=True)
    editor.add_argument("--output", required=True)

    extract = subparsers.add_parser("extract")
    extract.add_argument("--defold-root", required=True)
    extract.add_argument("--source", required=True)
    extract.add_argument("--output", required=True)

    convert = subparsers.add_parser("convert")
    convert.add_argument("--docs-dir", required=True)
    convert.add_argument("--input", required=True)
    convert.add_argument("--output-dir", required=True)
    convert.add_argument("--key", required=True)
    convert.add_argument("--formats", nargs="+", required=True)
    convert.add_argument("--pythonpath", action="append", default=[])

    sync = subparsers.add_parser("sync")
    sync.add_argument("--manifest", required=True)
    sync.add_argument("--output-dir", required=True)
    sync.add_argument("--all-formats", nargs="+", default=ALL_TARGET_FORMATS)
    sync.add_argument("--stamp", required=True)

    archive = subparsers.add_parser("zip")
    archive.add_argument("--input-dir", required=True)
    archive.add_argument("--archive", required=True)
    archive.add_argument("--archive-root", required=True)

    tests = subparsers.add_parser("test")
    tests.add_argument("--docs-dir", required=True)
    tests.add_argument("--pythonpath", action="append", default=[])
    tests.add_argument("--stamp", required=True)

    args = parser.parse_args()

    if args.command == "editor":
        timed("Generating editor API docs", lambda: write_editor_docs(os.path.abspath(args.defold_root), os.path.abspath(args.output)))
    elif args.command == "extract":
        timed("Extracting API docs from %s" % args.source, lambda: extract_source(os.path.abspath(args.defold_root), os.path.abspath(args.source), os.path.abspath(args.output)))
    elif args.command == "convert":
        timed("Converting API doc %s" % args.key, lambda: convert_apidoc(os.path.abspath(args.docs_dir), os.path.abspath(args.input), os.path.abspath(args.output_dir), args.key, parse_formats(args.formats), [os.path.abspath(path) for path in args.pythonpath]))
    elif args.command == "sync":
        timed("Installing API docs", lambda: sync_outputs(os.path.abspath(args.manifest), os.path.abspath(args.output_dir), parse_formats(args.all_formats), os.path.abspath(args.stamp)))
    elif args.command == "zip":
        timed("Packaging API docs", lambda: zip_tree(os.path.abspath(args.input_dir), os.path.abspath(args.archive), os.path.abspath(args.archive_root)))
    elif args.command == "test":
        timed("Running script_doc tests", lambda: run_tests(os.path.abspath(args.docs_dir), [os.path.abspath(path) for path in args.pythonpath], os.path.abspath(args.stamp)))
    else:
        parser.error("Unknown command: %s" % args.command)


if __name__ == "__main__":
    main()
