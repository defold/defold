#!/usr/bin/env python3

import argparse
import os
import shlex
import shutil
import subprocess
import sys
from pathlib import Path


def copytree(src, dst):
    if dst.exists():
        shutil.rmtree(dst)
    ignore = shutil.ignore_patterns("build", "__pycache__", ".DS_Store")
    shutil.copytree(src, dst, ignore=ignore)


def copy_missing_tree(src, dst):
    for path in src.rglob("*"):
        if not path.is_file():
            continue
        rel_path = path.relative_to(src)
        output = dst / rel_path
        if output.exists():
            continue
        output.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, output)


def run(args, cwd):
    subprocess.check_call(args, cwd=str(cwd))


def rewrite_resource_uri(stage_root):
    game_project = stage_root / "game.project"
    text = game_project.read_text()
    text = text.replace("uri = src/test/build/default", "uri = build/src/test/build/default")
    game_project.write_text(text)


def java_command(args, main_class, *bob_args):
    command = [args.java]
    runtime_flags = os.environ.get("DM_JAVA_RUNTIME_FLAGS", "")
    if runtime_flags:
        command.extend(shlex.split(runtime_flags))
    command.extend(["-cp", os.pathsep.join(args.classpath), main_class])
    command.extend(bob_args)
    return command


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", required=True)
    parser.add_argument("--stage-root", required=True)
    parser.add_argument("--builtins-root", required=True)
    parser.add_argument("--stamp", required=True)
    parser.add_argument("--platform", required=True)
    parser.add_argument("--java", required=True)
    parser.add_argument("--classpath", action="append", required=True)
    parser.add_argument("--settings", action="append", default=[])
    args = parser.parse_args()

    source_root = Path(args.source_root).resolve()
    stage_root = Path(args.stage_root).resolve()
    builtins_root = Path(args.builtins_root).resolve()
    stamp = Path(args.stamp).resolve()
    engine_root = source_root.parents[1]
    bob_root = os.path.relpath(stage_root, engine_root)

    copytree(source_root, stage_root)
    rewrite_resource_uri(stage_root)

    bob_flags = [
        "--platform=%s" % args.platform,
        "--variant=debug",
        "--use-vanilla-lua",
    ]

    run(java_command(args, "com.dynamo.bob.Bob", "-root", bob_root, "clean"), engine_root)

    for settings in args.settings:
        settings_path = os.path.relpath(stage_root / settings, engine_root)
        run(java_command(args, "com.dynamo.bob.Bob",
                         "-root", bob_root,
                         "build",
                         *bob_flags,
                         "--settings", settings_path), engine_root)

    run(java_command(args, "com.dynamo.bob.Bob",
                     "-root", bob_root,
                     "build",
                     *bob_flags), engine_root)

    runtime_builtins = stage_root / "build" / "default" / "builtins"
    if builtins_root.exists():
        copy_missing_tree(builtins_root, runtime_builtins)

    stamp.parent.mkdir(parents=True, exist_ok=True)
    stamp.write_text("engine test content\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
