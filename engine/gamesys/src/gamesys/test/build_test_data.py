#!/usr/bin/env python3

import argparse
import os
import shutil
import subprocess
import tempfile
from pathlib import Path


def ignored_source_file(path):
    return path.name == ".DS_Store" or path.name.startswith(".prebuilt_")


def copy_immediate_files(src, dst):
    if dst.exists():
        shutil.rmtree(dst)
    dst.mkdir(parents=True, exist_ok=True)

    for child in src.iterdir():
        if child.is_dir() or ignored_source_file(child):
            continue
        shutil.copy2(child, dst / child.name)


def replace_tree(src, dst):
    if not src.exists():
        raise FileNotFoundError(src)
    if dst.exists():
        shutil.rmtree(dst)
    shutil.copytree(src, dst)


def copy_immediate_outputs(src, dst):
    if not src.exists():
        raise FileNotFoundError(src)

    dst.mkdir(parents=True, exist_ok=True)
    for child in src.iterdir():
        if child.is_file():
            shutil.copy2(child, dst / child.name)


def copy_raw_outputs(stage_root, output_root, build_inputs):
    for build_input in build_inputs:
        input_path = Path(build_input.lstrip("/"))
        if input_path.suffix != ".raw":
            continue

        src = stage_root / input_path
        dst = output_root / input_path.with_suffix(input_path.suffix + "c")
        if not src.exists():
            raise FileNotFoundError(src)

        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)


def copy_optional_project_file(src, dst):
    project_file = src / "game.project"
    if project_file.exists():
        shutil.copy2(project_file, dst / project_file.name)


def copy_file_once(src, dst):
    if dst.exists():
        return

    dst.parent.mkdir(parents=True, exist_ok=True)
    tmp = None
    try:
        fd, tmp = tempfile.mkstemp(prefix=".%s." % dst.name, dir=str(dst.parent))
        tmp = Path(tmp)
        os.close(fd)
        shutil.copy2(src, tmp)
        try:
            os.replace(tmp, dst)
            tmp = None
        except PermissionError:
            # Parallel CMake jobs may copy the same bob-light helper at the same time.
            if not dst.exists() or dst.stat().st_size != src.stat().st_size:
                raise
    finally:
        if tmp is not None and tmp.exists():
            tmp.unlink()


def copy_optional_ogg_tools(dynamo_home, bob_root):
    ext_bin = dynamo_home / "ext" / "bin"
    ext_lib = dynamo_home / "ext" / "lib"
    if not ext_bin.exists():
        return

    for platform_dir in ext_bin.iterdir():
        oggz_validate = platform_dir / ("oggz-validate.exe" if platform_dir.name.endswith("win32") else "oggz-validate")
        if not oggz_validate.exists():
            continue

        dst_dir = bob_root / platform_dir.name
        dst_dir.mkdir(parents=True, exist_ok=True)
        copy_file_once(oggz_validate, dst_dir / oggz_validate.name)
        (dst_dir / oggz_validate.name).chmod(0o755)

        lib_dir = ext_lib / platform_dir.name
        if lib_dir.exists():
            for lib in lib_dir.glob("libogg*"):
                copy_file_once(lib, dst_dir / lib.name)


def get_bob_root(output_root, env):
    env_root = env.get("DM_BOB_ROOTFOLDER")
    if env_root:
        return Path(env_root).resolve()
    return output_root / ".bob" / "bob-root"


def read_build_inputs(path):
    inputs = []
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.split("#", 1)[0].strip()
        if line:
            inputs.append(line)
    return inputs


def build_bob_command(bob_light, inputs, stage_root, bob_output, platform):
    builtins_zip = bob_light.parent.parent / "builtins.zip"
    classpath = [str(bob_light)]
    if builtins_zip.exists():
        classpath.append(str(builtins_zip))

    cmd = [
        "java",
        "--enable-native-access=ALL-UNNAMED",
        "-cp",
        os.pathsep.join(classpath),
        "com.dynamo.bob.Bob",
        "--root",
        str(stage_root),
        "--output",
        bob_output.as_posix(),
        "--build-input-file",
        str(inputs),
    ]
    if platform:
        cmd.extend(["--platform", platform])
    cmd.append("build")
    return cmd


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--bob-light", required=True)
    parser.add_argument("--root", required=True)
    parser.add_argument("--folder", required=True)
    parser.add_argument("--inputs", required=True)
    parser.add_argument("--output-root", required=True)
    parser.add_argument("--stamp", required=True)
    parser.add_argument("--platform", default=None)
    args = parser.parse_args()

    root = Path(args.root).resolve()
    folder = args.folder.strip("/")
    inputs = Path(args.inputs).resolve()
    output_root = Path(args.output_root).resolve()
    stamp = Path(args.stamp).resolve()
    bob_light = Path(args.bob_light).resolve()

    if not folder or "/" in folder or "\\" in folder or folder == ".." or folder.startswith("../"):
        raise SystemExit("Expected a top-level test data folder, got: %s" % args.folder)

    stage_root = output_root / ".bob" / "roots" / folder
    stage_folder = stage_root / folder
    if stage_root.exists():
        shutil.rmtree(stage_root)
    copy_immediate_files(root / folder, stage_folder)
    copy_optional_project_file(root / folder, stage_root)

    bob_output = Path("build") / "bob-test-data"
    bob_output_abs = stage_root / bob_output
    cmd = build_bob_command(bob_light, inputs, stage_root, bob_output, args.platform)

    env = os.environ.copy()
    bob_root = get_bob_root(output_root, env)
    bob_root.mkdir(parents=True, exist_ok=True)
    dynamo_home = bob_light.parent.parent.parent
    copy_optional_ogg_tools(dynamo_home, bob_root)
    env["DM_BOB_ROOTFOLDER"] = str(bob_root)

    build_inputs = read_build_inputs(inputs)
    print("Building gamesys test data folder %s" % folder, flush=True)
    print("  Bob staged root: %s" % stage_root, flush=True)
    print("  Bob output: %s" % bob_output_abs, flush=True)
    print("  Bob root: %s" % bob_root, flush=True)
    for build_input in build_inputs:
        print("  Input: %s" % build_input, flush=True)

    try:
        subprocess.check_call(cmd, env=env)
    except subprocess.CalledProcessError as e:
        print("Failed building gamesys test data folder %s" % folder, flush=True)
        print("  build.inputs: %s" % inputs, flush=True)
        for build_input in build_inputs:
            print("  Input: %s" % build_input, flush=True)
        raise SystemExit(e.returncode)

    src = bob_output_abs / folder
    dst = output_root / folder
    replace_tree(src, dst)
    copy_immediate_outputs(bob_output_abs, output_root)
    copy_raw_outputs(stage_root, output_root, build_inputs)

    stamp.parent.mkdir(parents=True, exist_ok=True)
    stamp.write_text("ok\n", encoding="utf-8")


if __name__ == "__main__":
    main()
