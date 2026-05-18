#!/usr/bin/env python3

import argparse
import os
import re
import shutil
import subprocess
import tempfile
from pathlib import Path


CUSTOM_EXTENSIONS = (".a", ".b", ".c", ".mt", ".it", ".rt", ".no_user_datac")
CUSTOM_SOURCE_EXTENSIONS = {
    ".a_pb": ".a",
    ".b_pb": ".b",
    ".c_pb": ".c",
    ".mt_pb": ".mt",
    ".it_pb": ".it",
    ".rt_pb": ".rt",
    ".no_user_data": ".no_user_datac",
}
CUSTOM_BUILDER_CLASSES = {
    ".a": "GameObjectTestCopyA",
    ".b": "GameObjectTestCopyB",
    ".c": "GameObjectTestCopyC",
    ".mt": "GameObjectTestCopyMt",
    ".it": "GameObjectTestCopyIt",
    ".rt": "GameObjectTestCopyRt",
    ".no_user_datac": "GameObjectTestCopyNoUserData",
    ".": "GameObjectTestCopyNoExt",
}
SOURCE_EXTENSIONS = (".go", ".collection", ".script", ".lua") + CUSTOM_EXTENSIONS
COMPILED_EXTENSIONS = {
    ".go": ".goc",
    ".collection": ".collectionc",
    ".script": ".scriptc",
    ".lua": ".luac",
}


def ignored_source_file(path):
    return path.name == ".DS_Store" or path.name.startswith(".prebuilt_") or path.name == "build.inputs"


def bob_source_name(path):
    if path.name.endswith(".go_pb"):
        return path.name[:-6] + ".go"
    for src_ext, bob_ext in CUSTOM_SOURCE_EXTENSIONS.items():
        if path.name.endswith(src_ext):
            return path.name[:-len(src_ext)] + bob_ext
    return path.name


def bob_source_data(path):
    data = path.read_text(encoding="utf-8")
    if path.name.endswith(".go_pb"):
        data = data.replace(".scriptc", ".script")
        data = data.replace(".luac", ".lua")
    if path.name.endswith(".collection"):
        data = data.replace(".goc", ".go")
        data = data.replace(".collectionc", ".collection")
    if path.name.endswith(".script") or path.name.endswith(".lua"):
        data = strip_nested_go_properties(data)
        data = data.replace("vmath.quat()", "vmath.quat(0, 0, 0, 1)")
        data = re.sub(r"resource\.material\((.*?)\)", r"hash(\1)", data)
        data = data.replace("hash()", "hash(\"\")")
    return data


def strip_nested_go_properties(data):
    lines = []
    for line in data.splitlines():
        if line != line.lstrip() and line.lstrip().startswith("go.property("):
            lines.append("")
        else:
            lines.append(line)
    return "\n".join(lines) + ("\n" if data.endswith("\n") else "")


def copy_immediate_files(src, dst):
    if dst.exists():
        shutil.rmtree(dst)
    dst.mkdir(parents=True, exist_ok=True)

    for child in src.iterdir():
        if child.is_dir() or ignored_source_file(child):
            continue

        out = dst / bob_source_name(child)
        if child.suffix in (".cpp", ".h", ".proto") or child.name == "CMakeLists.txt":
            continue
        if child.name.endswith(".go_pb") or child.name.endswith(".collection") or child.name.endswith(".script") or child.name.endswith(".lua"):
            out.write_text(bob_source_data(child), encoding="utf-8")
        else:
            shutil.copy2(child, out)


def replace_tree(src, dst):
    if not src.exists():
        raise FileNotFoundError(src)
    if dst.exists():
        shutil.rmtree(dst)
    shutil.copytree(src, dst)


def strip_zero_component_type_counts(root, protoc, gameobject_proto, proto_includes):
    if not protoc or not gameobject_proto:
        return

    component_type_block = re.compile(r"component_types\s*\{[^{}]*\}\s*", re.MULTILINE)
    include_args = []
    for include in proto_includes:
        include_args.extend(["-I", str(include)])

    for collection in root.rglob("*.collectionc"):
        data = collection.read_bytes()
        decode_cmd = [
            protoc,
            "--decode=dmGameObjectDDF.CollectionDesc",
            *include_args,
            str(gameobject_proto),
        ]
        decoded = subprocess.check_output(decode_cmd, input=data).decode("utf-8")

        def replace_block(match):
            block = match.group(0)
            if re.search(r"\bmax_count:\s*0\b", block):
                return ""
            return block

        stripped = component_type_block.sub(replace_block, decoded)
        if stripped == decoded:
            continue

        encode_cmd = [
            protoc,
            "--encode=dmGameObjectDDF.CollectionDesc",
            *include_args,
            str(gameobject_proto),
        ]
        encoded = subprocess.check_output(encode_cmd, input=stripped.encode("utf-8"))
        collection.write_bytes(encoded)


def remove_custom_outputs(path):
    if not path.exists():
        return
    for child in path.iterdir():
        if child.is_file() and child.name.endswith(CUSTOM_EXTENSIONS):
            child.unlink()


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


def ensure_placeholders(stage_root):
    placeholder_outputs = []
    resource_ref = re.compile(r'["\'](/[^"\']+(?:%s))["\']' % "|".join(re.escape(ext) for ext in SOURCE_EXTENSIONS))
    material_ref = re.compile(r'resource\.material\("([^"]+)"\)')

    for source in stage_root.rglob("*"):
        if not source.is_file() or source.name.endswith(CUSTOM_EXTENSIONS):
            continue
        try:
            data = source.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            continue
        for match in resource_ref.finditer(data):
            placeholder = stage_root / match.group(1).lstrip("/")
            if not placeholder.exists():
                placeholder.parent.mkdir(parents=True, exist_ok=True)
                placeholder.write_text("", encoding="utf-8")
                placeholder_outputs.append(output_path_for_placeholder(placeholder.relative_to(stage_root)))
        for match in material_ref.finditer(data):
            placeholder = stage_root / match.group(1).lstrip("/")
            if not placeholder.exists():
                placeholder.parent.mkdir(parents=True, exist_ok=True)
                placeholder.write_text("", encoding="utf-8")
                placeholder_outputs.append(placeholder.relative_to(stage_root))
    return placeholder_outputs


def output_path_for_placeholder(rel_path):
    rel_path = Path(rel_path)
    name = rel_path.name
    for src_ext, out_ext in COMPILED_EXTENSIONS.items():
        if name.endswith(src_ext):
            return rel_path.with_name(name[:-len(src_ext)] + out_ext)
    return rel_path


def compile_custom_builders(classes_dir, bob_light, javac):
    expected_classes = [
        classes_dir / "com" / "defold" / "extension" / "pipeline" / ("%s.class" % class_name)
        for class_name in CUSTOM_BUILDER_CLASSES.values()
    ]
    if all(path.exists() for path in expected_classes):
        return

    source_dir = classes_dir.parent / "java" / "com" / "defold" / "extension" / "pipeline"
    source_dir.mkdir(parents=True, exist_ok=True)
    java_sources = []
    for ext, class_name in CUSTOM_BUILDER_CLASSES.items():
        source = source_dir / ("%s.java" % class_name)
        if ext == ".":
            source.write_text("""package com.defold.extension.pipeline;

import java.io.IOException;
import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;

@BuilderParams(name = "%s", inExts = "%s", outExt = "")
public class %s extends Builder {
    @Override
    public Task create(IResource input) throws IOException, CompileExceptionError {
        return Task.newBuilder(this).setName(params.name()).addInput(input).addOutput(input.output()).build();
    }

    @Override
    public void build(Task task) throws IOException, CompileExceptionError {
        task.output(0).setContent(task.input(0).getContent());
    }
}
""" % (class_name, ext, class_name), encoding="utf-8")
        else:
            source.write_text("""package com.defold.extension.pipeline;

import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CopyBuilder;

@BuilderParams(name = "%s", inExts = "%s", outExt = "%s")
public class %s extends CopyBuilder {
}
""" % (class_name, ext, ext, class_name), encoding="utf-8")
        java_sources.append(str(source))

    classes_dir.mkdir(parents=True, exist_ok=True)
    cmd = [javac, "-cp", str(bob_light), "-d", str(classes_dir)] + java_sources
    subprocess.check_call(cmd)


def build_bob_command(bob_light, custom_builders_dir, inputs, stage_root, bob_output, platform):
    builtins_zip = bob_light.parent.parent / "builtins.zip"
    classpath = [str(custom_builders_dir), str(bob_light)]
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
        "--use-uncompressed-lua-source",
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
    parser.add_argument("--javac", default="javac")
    parser.add_argument("--protoc", default=None)
    parser.add_argument("--gameobject-proto", default=None)
    parser.add_argument("--proto-include", action="append", default=[])
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
    copy_immediate_files(root / folder, stage_root)
    placeholder_outputs = ensure_placeholders(stage_root)

    bob_output = Path("build") / "bob-test-data"
    bob_output_abs = stage_root / bob_output
    custom_builders_dir = output_root / ".bob" / "custom-builders" / "classes"
    compile_custom_builders(custom_builders_dir, bob_light, args.javac)
    cmd = build_bob_command(bob_light, custom_builders_dir, inputs, stage_root, bob_output, args.platform)

    env = os.environ.copy()
    bob_root = get_bob_root(output_root, env)
    bob_root.mkdir(parents=True, exist_ok=True)
    env["DM_BOB_ROOTFOLDER"] = str(bob_root)

    build_inputs = read_build_inputs(inputs)
    print("Building gameobject test data folder %s" % folder, flush=True)
    print("  Bob staged root: %s" % stage_root, flush=True)
    print("  Bob output: %s" % bob_output_abs, flush=True)
    print("  Bob root: %s" % bob_root, flush=True)
    for build_input in build_inputs:
        print("  Input: %s" % build_input, flush=True)

    try:
        subprocess.check_call(cmd, env=env)
    except subprocess.CalledProcessError as e:
        print("Failed building gameobject test data folder %s" % folder, flush=True)
        print("  build.inputs: %s" % inputs, flush=True)
        for build_input in build_inputs:
            print("  Input: %s" % build_input, flush=True)
        raise SystemExit(e.returncode)

    src = bob_output_abs
    for rel_path in placeholder_outputs:
        placeholder_output = src / rel_path
        if placeholder_output.exists():
            placeholder_output.unlink()
    strip_zero_component_type_counts(src, args.protoc, args.gameobject_proto, args.proto_include)
    remove_custom_outputs(src)
    dst = output_root / folder
    replace_tree(src, dst)

    stamp.parent.mkdir(parents=True, exist_ok=True)
    stamp.write_text("ok\n", encoding="utf-8")


if __name__ == "__main__":
    main()
