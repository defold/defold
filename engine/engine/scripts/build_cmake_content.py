#!/usr/bin/env python3

import argparse
import os
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path


BUILD_INPUT_EXT_EXCLUDES = {
    ".fp",
    ".vp",
    ".cp",
    ".ttf",
}

ARCHIVE_EXT_EXCLUDES = {
    ".fp",
    ".vp",
    ".cp",
    ".ttf",
    ".texture_profiles",
}

ARCHIVE_NAME_EXCLUDES = {
    "LICENSE",
}

BOB_RELATIVE_DIRS = [
    "builtins/input",
    "builtins/render",
    "builtins/fonts",
    "builtins/connect",
    "builtins/materials",
    "builtins/graphics",
    "builtins/scripts",
]

RAW_ARCHIVE_EXTS = {
    ".glsl",
}

RAW_ARCHIVE_FILES = {
    "builtins/connect/game.project",
}


def run(args, cwd):
    subprocess.check_call(args, cwd=str(cwd))


def copytree(src, dst):
    if dst.exists():
        shutil.rmtree(dst)
    shutil.copytree(src, dst)


def iter_content_files(root):
    for rel_dir in BOB_RELATIVE_DIRS:
        current = root / rel_dir
        if not current.exists():
            continue
        for path in sorted(current.rglob("*")):
            if path.is_file() and path.name != ".DS_Store":
                yield path


def should_build_input(path):
    if path.name in ARCHIVE_NAME_EXCLUDES:
        return False
    if path.suffix in BUILD_INPUT_EXT_EXCLUDES:
        return False
    if path.as_posix().endswith("/builtins/connect/game.project"):
        return False
    return True


def should_archive_raw(rel_path):
    if rel_path in RAW_ARCHIVE_FILES:
        return True
    return Path(rel_path).suffix in RAW_ARCHIVE_EXTS


def write_build_inputs(stage_root, output):
    inputs = []
    for path in iter_content_files(stage_root):
        if should_build_input(path):
            inputs.append("/" + path.relative_to(stage_root).as_posix())
    output.write_text("\n".join(inputs) + "\n")


def stage_raw_archive_inputs(stage_root, build_root):
    raw_paths = []
    for path in iter_content_files(stage_root):
        rel_path = path.relative_to(stage_root).as_posix()
        if not should_archive_raw(rel_path):
            continue
        staged = build_root / rel_path
        staged.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, staged)
        raw_paths.append(staged)
    return raw_paths


def collect_archive_inputs(build_root):
    inputs = []
    for path in sorted(build_root.rglob("*")):
        if not path.is_file():
            continue
        rel = path.relative_to(build_root).as_posix()
        if rel in ("digest_cache", "_BobBuildState_"):
            continue
        if path.name.startswith("builtins.") or path.name == "liveupdate.game.dmanifest":
            continue
        if path.suffix in (".arci", ".arcd", ".dmanifest", ".manifest_hash", ".zip", ".shbundlec"):
            continue
        inputs.append(path)
    return inputs


def build_builtins(args):
    source_root = Path(args.source_root).resolve()
    work_root = Path(args.work_root).resolve()
    output_root = Path(args.output_root).resolve()
    stamp = Path(args.stamp).resolve()
    bob_light = Path(args.bob_light).resolve()

    copytree(source_root, work_root)
    output_root.mkdir(parents=True, exist_ok=True)

    build_inputs = work_root / "builtins-build.inputs"
    write_build_inputs(work_root, build_inputs)

    java_cmd = [
        args.java,
        "-cp",
        str(bob_light),
        "com.dynamo.bob.Bob",
        "--root",
        ".",
        "--settings",
        "builtins/connect/game.project",
        "--platform",
        args.platform,
        "--variant=debug",
        "--use-vanilla-lua",
        "--build-input-file",
        build_inputs.name,
        "clean",
        "build",
    ]
    run(java_cmd, work_root)

    build_root = work_root / "build/default"
    stage_raw_archive_inputs(work_root, build_root)

    archive_inputs = collect_archive_inputs(build_root)
    if not archive_inputs:
        raise RuntimeError("No builtin archive inputs were produced")

    archive_output = output_root / "builtins"
    archive_cmd = [
        args.java,
        "-cp",
        str(bob_light),
        "com.dynamo.bob.archive.ArchiveBuilder",
        str(build_root),
        str(archive_output),
        "-m",
        "-c",
    ] + [str(path) for path in archive_inputs]
    run(archive_cmd, work_root)

    for suffix in (".arci", ".arcd", ".dmanifest"):
        output = archive_output.with_suffix(suffix)
        if not output.exists():
            raise RuntimeError("Missing builtin archive output: %s" % output)

    stamp.parent.mkdir(parents=True, exist_ok=True)
    stamp.write_text("builtins\n")


def package_bob(args):
    source_root = Path(args.source_root).resolve()
    output = Path(args.output).resolve()
    bob_light = Path(args.bob_light).resolve()

    output.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(bob_light, output)

    with zipfile.ZipFile(output, "a", zipfile.ZIP_DEFLATED) as archive:
        for path in sorted((source_root / "builtins").rglob("*")):
            if path.is_file():
                archive.write(path, path.relative_to(source_root).as_posix())


def main():
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)

    builtins = subparsers.add_parser("builtins")
    builtins.add_argument("--source-root", required=True)
    builtins.add_argument("--work-root", required=True)
    builtins.add_argument("--output-root", required=True)
    builtins.add_argument("--stamp", required=True)
    builtins.add_argument("--bob-light", required=True)
    builtins.add_argument("--platform", required=True)
    builtins.add_argument("--java", required=True)
    builtins.set_defaults(func=build_builtins)

    bob = subparsers.add_parser("package-bob")
    bob.add_argument("--source-root", required=True)
    bob.add_argument("--output", required=True)
    bob.add_argument("--bob-light", required=True)
    bob.set_defaults(func=package_bob)

    args = parser.parse_args()
    try:
        args.func(args)
    except subprocess.CalledProcessError as exc:
        return exc.returncode
    except Exception as exc:
        print(exc, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
