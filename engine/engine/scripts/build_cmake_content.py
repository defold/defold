#!/usr/bin/env python3

import argparse
import os
import shlex
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

PROJECT_OWNED_BUILD_INPUTS = {
    "builtins/input/default.gamepads",
    "builtins/input/gamecontrollerdb.txt",
}


def run(args, cwd):
    subprocess.check_call(args, cwd=str(cwd))


def java_command(java, main_class, classpath, *args):
    command = [java]
    runtime_flags = os.environ.get("DM_JAVA_RUNTIME_FLAGS", "")
    if runtime_flags:
        command.extend(shlex.split(runtime_flags))
    command.extend(["-cp", classpath, main_class])
    command.extend(args)
    return command


def copytree(src, dst):
    if dst.exists():
        shutil.rmtree(dst)
    shutil.copytree(src, dst)


def overlay_font_content(font_content_root, destination_root):
    if not font_content_root:
        return
    source = Path(font_content_root).resolve()
    for path in sorted(source.rglob("*")):
        if path.is_file():
            destination = destination_root / path.relative_to(source)
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, destination)


def iter_content_files(root):
    for rel_dir in BOB_RELATIVE_DIRS:
        current = root / rel_dir
        if not current.exists():
            continue
        for path in sorted(current.rglob("*")):
            if path.is_file() and path.name != ".DS_Store":
                yield path


def should_build_input(path):
    rel_path = path.as_posix()
    if path.name in ARCHIVE_NAME_EXCLUDES:
        return False
    if path.suffix in BUILD_INPUT_EXT_EXCLUDES:
        return False
    if rel_path.endswith("/builtins/connect/game.project"):
        return False
    for project_owned_input in PROJECT_OWNED_BUILD_INPUTS:
        if rel_path.endswith("/" + project_owned_input):
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


def rebuild_builtin_fonts(args, stage_root, build_root, bob_classpath):
    for source in sorted((stage_root / "builtins").rglob("*.font")):
        rel_path = source.relative_to(stage_root).with_suffix(".fontc")
        output = build_root / rel_path
        output.parent.mkdir(parents=True, exist_ok=True)
        run(java_command(
            args.java,
            "com.dynamo.bob.font.Fontc",
            bob_classpath,
            str(source),
            str(output),
            str(stage_root),
            "false",
        ), stage_root)


def remove_root_generated_font_outputs(build_root):
    for path in build_root.glob("_generated_*.glyph_bankc"):
        path.unlink()


def rebuild_builtin_gamepads(args, stage_root, build_root, bob_classpath):
    default_gamepads = stage_root / "builtins/input/default.gamepads"
    gamecontrollerdb = stage_root / "builtins/input/gamecontrollerdb.txt"
    output = build_root / "builtins/input/default.gamepadsc"
    inputs = []

    if gamecontrollerdb.exists():
        inputs.append(str(gamecontrollerdb))
    if default_gamepads.exists():
        inputs.append(str(default_gamepads))
    if not inputs:
        return

    output.parent.mkdir(parents=True, exist_ok=True)
    run(java_command(
        args.java,
        "com.dynamo.bob.pipeline.GamepadBuilder",
        bob_classpath,
        *inputs,
        str(output),
        args.platform,
    ), stage_root)


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
    bob_classpath = args.bob_classpath or str(bob_light)

    copytree(source_root, work_root)
    overlay_font_content(args.font_content_root, work_root)
    output_root.mkdir(parents=True, exist_ok=True)

    build_inputs = work_root / "builtins-build.inputs"
    write_build_inputs(work_root, build_inputs)

    java_cmd = java_command(
        args.java,
        "com.dynamo.bob.Bob",
        bob_classpath,
        "--root",
        ".",
        "--settings",
        "builtins/connect/game.project",
        "--platform",
        args.platform,
        "--variant=debug",
        "--use-uncompressed-lua-source",
    )
    for shader_output in args.shader_output:
        java_cmd.append("--debug-output-%s=true" % shader_output)
    java_cmd += [
        "--build-input-file",
        build_inputs.name,
        "clean",
        "build",
    ]
    run(java_cmd, work_root)

    build_root = work_root / "build/default"
    rebuild_builtin_gamepads(args, work_root, build_root, bob_classpath)
    rebuild_builtin_fonts(args, work_root, build_root, bob_classpath)
    remove_root_generated_font_outputs(build_root)
    stage_raw_archive_inputs(work_root, build_root)

    archive_inputs = collect_archive_inputs(build_root)
    if not archive_inputs:
        raise RuntimeError("No builtin archive inputs were produced")

    archive_output = output_root / "builtins"
    archive_cmd = java_command(
        args.java,
        "com.dynamo.bob.archive.ArchiveBuilder",
        bob_classpath,
        str(build_root),
        str(archive_output),
        "-m",
        "-c",
        *[str(path) for path in archive_inputs],
    )
    run(archive_cmd, work_root)

    for suffix in (".arci", ".arcd", ".dmanifest"):
        output = archive_output.with_suffix(suffix)
        if not output.exists():
            raise RuntimeError("Missing builtin archive output: %s" % output)

    stamp.parent.mkdir(parents=True, exist_ok=True)
    stamp.write_text("builtins\n")


def package_bob(args):
    source_root = Path(args.source_root).resolve()
    work_root = Path(args.work_root).resolve()
    output = Path(args.output).resolve()
    bob_light = Path(args.bob_light).resolve()

    copytree(source_root, work_root)
    overlay_font_content(args.font_content_root, work_root)

    output.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(bob_light, output)

    with zipfile.ZipFile(output, "a", zipfile.ZIP_DEFLATED) as archive:
        for path in sorted((work_root / "builtins").rglob("*")):
            if path.is_file():
                archive.write(path, path.relative_to(work_root).as_posix())


def main():
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)

    builtins = subparsers.add_parser("builtins")
    builtins.add_argument("--source-root", required=True)
    builtins.add_argument("--font-content-root")
    builtins.add_argument("--work-root", required=True)
    builtins.add_argument("--output-root", required=True)
    builtins.add_argument("--stamp", required=True)
    builtins.add_argument("--bob-light", required=True)
    builtins.add_argument("--bob-classpath")
    builtins.add_argument("--platform", required=True)
    builtins.add_argument("--shader-output", action="append", default=[])
    builtins.add_argument("--java", required=True)
    builtins.set_defaults(func=build_builtins)

    bob = subparsers.add_parser("package-bob")
    bob.add_argument("--source-root", required=True)
    bob.add_argument("--font-content-root")
    bob.add_argument("--work-root", required=True)
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
