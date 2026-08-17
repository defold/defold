#!/usr/bin/env python
# Copyright 2020-2026 The Defold Foundation
# Licensed under the Defold License version 1.0

import argparse
import io
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
import urllib.request
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
    if not matches:
        return None
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
    document_found = False
    for comment_str in re.findall(r"/(\*[\*#].*?)\*/", source, re.DOTALL):
        comment = parse_comment(comment_str)
        if comment:
            namespace = comment.get("namespace")
            if comment["is_document"]:
                document_found = True
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

    # ScriptDoc Document.info has required fields populated from @document.
    if not document_found:
        elements = {}

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

    if not re.search(r"/\*[\*#]", doc_str):
        for _, output in output_specs:
            write_if_changed(output, "")
        log("Skipped empty API doc: %s" % key)
        return

    add_python_paths([docs_dir] + pythonpath)
    import script_doc

    script_doc.write_formats(doc_str, output_specs)
    log("Converted API doc %s to %d formats" % (key, len(output_specs)))


def read_input_list(path):
    path = Path(path).resolve()
    inputs = []
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line:
            continue
        input_path = Path(line)
        if not input_path.is_absolute():
            input_path = path.parent / input_path
        inputs.append(str(input_path.resolve()))
    return inputs


def generate_lua_annotations(docs_dir, inputs, output_dir, manifest, metadata, pythonpath, stamp):
    add_python_paths([docs_dir] + pythonpath)
    import lua_annotations
    import script_doc

    documents = []
    for input_path in inputs:
        input_path = os.path.abspath(input_path)
        doc_str = Path(input_path).read_text(encoding="utf-8")
        if not re.search(r"/\*[\*#]", doc_str):
            continue
        documents.append((input_path, script_doc.parse_document(doc_str, input_path)))

    output_names = lua_annotations.generate(
        documents,
        os.path.abspath(output_dir),
        os.path.abspath(metadata),
        strict=True)
    manifest_lines = [
        "%s|%s" % (os.path.join(os.path.abspath(output_dir), name), name)
        for name in output_names
    ]
    write_if_changed(manifest, "\n".join(manifest_lines) + "\n")
    write_stamp(stamp)
    log("Generated %d aggregate Lua annotation files" % len(output_names))


def run_lua_language_server(executable, input_dir):
    with tempfile.TemporaryDirectory(prefix="defold-lua-annotations.") as temp_dir:
        result_path = os.path.join(temp_dir, "diagnostics.json")
        log_path = os.path.join(temp_dir, "log")
        run_command([
            os.path.abspath(executable),
            "--check=%s" % input_dir,
            "--checklevel=Warning",
            "--logpath=%s" % log_path,
            "--check_out_path=%s" % result_path,
        ])
        diagnostics = {}
        if os.path.exists(result_path):
            diagnostics = json.loads(Path(result_path).read_text(encoding="utf-8") or "{}")
        return [
            problem
            for file_problems in diagnostics.values()
            for problem in file_problems
        ]


def diagnostic_counts(problems):
    counts = {}
    for problem in problems:
        code = problem.get("code", "unknown")
        counts[code] = counts.get(code, 0) + 1
    return counts


def validate_lua_annotations(executable, input_dir, metadata, stamp):
    import yaml

    input_dir = os.path.abspath(input_dir)
    metadata_data = yaml.safe_load(Path(metadata).read_text(encoding="utf-8"))
    allowed_diagnostics = set(metadata_data.get("allowed_diagnostics", []))
    unexpected_directives = []
    directive_pattern = re.compile(r"^---@diagnostic disable:\s*(\S+)\s*$")
    for path in sorted(Path(input_dir).rglob("*.lua")):
        for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            match = directive_pattern.match(line)
            if match and match.group(1) not in allowed_diagnostics:
                unexpected_directives.append("%s:%d: %s" % (path, line_number, match.group(1)))
    if unexpected_directives:
        raise RuntimeError(
            "Generated Lua annotations contain non-allowlisted diagnostic directives:\n"
            + "\n".join(unexpected_directives))

    problems = run_lua_language_server(executable, input_dir)
    if problems:
        counts = diagnostic_counts(problems)
        raise RuntimeError(
            "LuaLS reported %d problem(s): %s" % (
                len(problems),
                ", ".join("%s=%d" % item for item in sorted(counts.items()))))
    write_stamp(stamp)
    log("LuaLS validation completed without diagnostics")


def validate_lua_archive(executable, archive, metadata, stamp):
    with tempfile.TemporaryDirectory(prefix="defold-ref-doc.") as temp_dir:
        with zipfile.ZipFile(archive) as ref_doc:
            ref_doc.extractall(temp_dir)
        validate_lua_annotations(executable, temp_dir, metadata, stamp)
    log("Clean ref-doc.zip extraction passed LuaLS validation")


def validate_lua_behavior(executable, annotations_dir, fixture_dir, stamp):
    fixture_dir = Path(fixture_dir)
    expected_negative = {
        "assign-type-mismatch": 5,
        "param-type-mismatch": 6,
    }
    with tempfile.TemporaryDirectory(prefix="defold-lua-behavior.") as temp_dir:
        temp_dir = Path(temp_dir)
        results = {}
        for fixture_name in ("positive", "negative"):
            workspace = temp_dir / fixture_name
            workspace.mkdir()
            for annotation in Path(annotations_dir).glob("*.lua"):
                shutil.copy2(annotation, workspace / annotation.name)
            shutil.copy2(
                fixture_dir / ("%s.lua" % fixture_name),
                workspace / "main.lua")
            results[fixture_name] = run_lua_language_server(
                executable,
                str(workspace))

    if results["positive"]:
        raise RuntimeError(
            "Positive LuaLS behavior fixture reported diagnostics: %s"
            % diagnostic_counts(results["positive"]))
    negative_counts = diagnostic_counts(results["negative"])
    if negative_counts != expected_negative:
        raise RuntimeError(
            "Negative LuaLS behavior fixture expected %s, got %s"
            % (expected_negative, negative_counts))
    write_stamp(stamp)
    log("LuaLS positive and negative behavior fixtures passed")


def lua_language_server_version(project_clj):
    project = Path(project_clj).read_text(encoding="utf-8")
    match = re.search(r':lua-language-server-version\s+"([^"]+)"', project)
    if not match:
        raise RuntimeError(
            "Could not find :lua-language-server-version in %s" % project_clj)
    return match.group(1)


def install_lua_language_server(project_clj, platform, output_dir, github_env):
    version = lua_language_server_version(project_clj)
    release_url = (
        "https://github.com/defold/lua-language-server/releases/download/"
        "%s/release.zip" % version)
    inner_name = "lsp-lua-language-server/plugins/%s.zip" % platform
    output_dir = Path(output_dir).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    log("Downloading Defold LuaLS %s for %s" % (version, platform))
    with tempfile.TemporaryDirectory(prefix="defold-lualls-download.") as temp_dir:
        release_path = Path(temp_dir) / "release.zip"
        urllib.request.urlretrieve(release_url, str(release_path))
        with zipfile.ZipFile(str(release_path)) as release:
            try:
                inner_archive = release.read(inner_name)
            except KeyError:
                raise RuntimeError(
                    "LuaLS release %s does not contain %s" % (version, inner_name))

        prefix = ("bin", platform)
        with zipfile.ZipFile(io.BytesIO(inner_archive)) as platform_archive:
            for info in platform_archive.infolist():
                parts = Path(info.filename).parts
                if info.is_dir() or len(parts) <= 2:
                    continue
                if tuple(parts[:2]) != prefix:
                    raise RuntimeError(
                        "Unexpected path in LuaLS %s archive: %s" % (
                            platform, info.filename))
                target = output_dir.joinpath(*parts[2:])
                target.parent.mkdir(parents=True, exist_ok=True)
                write_if_changed(target, platform_archive.read(info), mode="wb")

    executable_name = "lua-language-server.exe" if platform.endswith("win32") else "lua-language-server"
    executable = output_dir / "bin" / executable_name
    if not executable.is_file():
        raise RuntimeError("LuaLS executable was not extracted to %s" % executable)
    executable.chmod(executable.stat().st_mode | 0o111)
    with open(github_env, "a", encoding="utf-8") as env_file:
        env_file.write("DEFOLD_DOCS_LUALS_EXECUTABLE=%s\n" % executable)
    log("Installed Defold LuaLS %s at %s" % (version, executable))


def read_manifest(path):
    entries = []
    for line in Path(path).read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line:
            continue
        source, target_name = line.split("|", 1)
        entries.append((Path(source), target_name))
    return entries


def sync_outputs(manifests, output_dir, all_formats, stamp):
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    entries = []
    for manifest in manifests:
        entries.extend(read_manifest(manifest))
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
    for target in output_dir.glob("*.lua"):
        if target.name not in expected_names:
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
    run_command(
        [sys.executable, "-m", "unittest", "discover", "-p", "test_*.py"],
        cwd=docs_dir,
        env=env)
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

    lua = subparsers.add_parser("lua")
    lua.add_argument("--docs-dir", required=True)
    lua_inputs = lua.add_mutually_exclusive_group(required=True)
    lua_inputs.add_argument("--input", action="append")
    lua_inputs.add_argument("--input-list")
    lua.add_argument("--output-dir", required=True)
    lua.add_argument("--manifest", required=True)
    lua.add_argument("--metadata", required=True)
    lua.add_argument("--pythonpath", action="append", default=[])
    lua.add_argument("--stamp", required=True)

    validate_lua = subparsers.add_parser("validate-lua")
    validate_lua.add_argument("--executable", required=True)
    validate_lua.add_argument("--input-dir", required=True)
    validate_lua.add_argument("--metadata", required=True)
    validate_lua.add_argument("--stamp", required=True)

    validate_lua_archive_parser = subparsers.add_parser("validate-lua-archive")
    validate_lua_archive_parser.add_argument("--executable", required=True)
    validate_lua_archive_parser.add_argument("--archive", required=True)
    validate_lua_archive_parser.add_argument("--metadata", required=True)
    validate_lua_archive_parser.add_argument("--stamp", required=True)

    validate_lua_behavior_parser = subparsers.add_parser("validate-lua-behavior")
    validate_lua_behavior_parser.add_argument("--executable", required=True)
    validate_lua_behavior_parser.add_argument("--annotations-dir", required=True)
    validate_lua_behavior_parser.add_argument("--fixture-dir", required=True)
    validate_lua_behavior_parser.add_argument("--stamp", required=True)

    install_lua_ls = subparsers.add_parser("install-lua-language-server")
    install_lua_ls.add_argument("--project-clj", required=True)
    install_lua_ls.add_argument("--platform", required=True)
    install_lua_ls.add_argument("--output-dir", required=True)
    install_lua_ls.add_argument("--github-env", required=True)

    sync = subparsers.add_parser("sync")
    sync.add_argument("--manifest", action="append", required=True)
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
    elif args.command == "lua":
        inputs = args.input or read_input_list(args.input_list)
        timed("Generating aggregate Lua annotations", lambda: generate_lua_annotations(
            os.path.abspath(args.docs_dir),
            [os.path.abspath(path) for path in inputs],
            os.path.abspath(args.output_dir),
            os.path.abspath(args.manifest),
            os.path.abspath(args.metadata),
            [os.path.abspath(path) for path in args.pythonpath],
            os.path.abspath(args.stamp)))
    elif args.command == "validate-lua":
        timed("Validating Lua annotations", lambda: validate_lua_annotations(
            os.path.abspath(args.executable),
            os.path.abspath(args.input_dir),
            os.path.abspath(args.metadata),
            os.path.abspath(args.stamp)))
    elif args.command == "validate-lua-archive":
        timed("Validating packaged Lua annotations", lambda: validate_lua_archive(
            os.path.abspath(args.executable),
            os.path.abspath(args.archive),
            os.path.abspath(args.metadata),
            os.path.abspath(args.stamp)))
    elif args.command == "validate-lua-behavior":
        timed("Validating Lua annotation behavior", lambda: validate_lua_behavior(
            os.path.abspath(args.executable),
            os.path.abspath(args.annotations_dir),
            os.path.abspath(args.fixture_dir),
            os.path.abspath(args.stamp)))
    elif args.command == "install-lua-language-server":
        timed("Installing pinned Lua language server", lambda: install_lua_language_server(
            os.path.abspath(args.project_clj),
            args.platform,
            os.path.abspath(args.output_dir),
            os.path.abspath(args.github_env)))
    elif args.command == "sync":
        timed("Installing API docs", lambda: sync_outputs([os.path.abspath(path) for path in args.manifest], os.path.abspath(args.output_dir), parse_formats(args.all_formats), os.path.abspath(args.stamp)))
    elif args.command == "zip":
        timed("Packaging API docs", lambda: zip_tree(os.path.abspath(args.input_dir), os.path.abspath(args.archive), os.path.abspath(args.archive_root)))
    elif args.command == "test":
        timed("Running script_doc tests", lambda: run_tests(os.path.abspath(args.docs_dir), [os.path.abspath(path) for path in args.pythonpath], os.path.abspath(args.stamp)))
    else:
        parser.error("Unknown command: %s" % args.command)


if __name__ == "__main__":
    main()
