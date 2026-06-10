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


def run(args, cwd, env=None):
    subprocess.check_call(args, cwd=str(cwd), env=env)


def rewrite_resource_uri(stage_root):
    game_project = stage_root / "game.project"
    text = game_project.read_text()
    text = text.replace("uri = src/test/build/default", "uri = build/src/test/build/default")
    game_project.write_text(text)


def copy_compiled_builtins(builtins_root, stage_root):
    if not builtins_root:
        return

    source = Path(builtins_root).resolve()
    if not source.exists():
        raise FileNotFoundError(source)

    destination = stage_root / "build/default/builtins"
    shutil.copytree(source, destination, dirs_exist_ok=True)


def java_command(args, main_class, *bob_args):
    command = [args.java]
    runtime_flags = os.environ.get("DM_JAVA_RUNTIME_FLAGS", "")
    if runtime_flags:
        command.extend(shlex.split(runtime_flags))
    command.extend(["-cp", os.pathsep.join(args.classpath), main_class])
    command.extend(bob_args)
    return command


def get_bob_root(stage_root, env):
    env_root = env.get("DM_BOB_ROOTFOLDER")
    if env_root:
        return Path(env_root) / "engine" / "test_content"
    return stage_root / ".bob" / "bob-root" / "test_content"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", required=True)
    parser.add_argument("--stage-root", required=True)
    parser.add_argument("--stamp", required=True)
    parser.add_argument("--platform", required=True)
    parser.add_argument("--java", required=True)
    parser.add_argument("--builtins-root")
    parser.add_argument("--classpath", action="append", required=True)
    parser.add_argument("--settings", action="append", default=[])
    args = parser.parse_args()

    source_root = Path(args.source_root).resolve()
    stage_root = Path(args.stage_root).resolve()
    stamp = Path(args.stamp).resolve()
    engine_root = source_root.parents[1]
    bob_root = os.path.relpath(stage_root, engine_root)
    env = os.environ.copy()
    bob_workspace_root = get_bob_root(stage_root, env)
    bob_workspace_root.mkdir(parents=True, exist_ok=True)
    env["DM_BOB_ROOTFOLDER"] = str(bob_workspace_root)

    copytree(source_root, stage_root)
    rewrite_resource_uri(stage_root)

    bob_flags = [
        "--platform=%s" % args.platform,
        "--variant=debug",
    ]
    if args.platform in ("js-web", "wasm-web", "wasm_pthread-web"):
        bob_flags.append("--use-uncompressed-lua-source")

    print("  Bob root: %s" % bob_workspace_root, flush=True)

    run(java_command(args, "com.dynamo.bob.Bob", "-root", bob_root, "clean"), engine_root, env=env)

    for settings in args.settings:
        settings_path = os.path.relpath(stage_root / settings, engine_root)
        run(java_command(args, "com.dynamo.bob.Bob",
                         "-root", bob_root,
                         "build",
                         *bob_flags,
                         "--settings", settings_path), engine_root, env=env)

    run(java_command(args, "com.dynamo.bob.Bob",
                     "-root", bob_root,
                     "build",
                     *bob_flags), engine_root, env=env)

    copy_compiled_builtins(args.builtins_root, stage_root)

    stamp.parent.mkdir(parents=True, exist_ok=True)
    stamp.write_text("engine test content\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
