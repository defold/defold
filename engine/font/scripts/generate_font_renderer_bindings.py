#!/usr/bin/env python3

# Copyright 2020-2026 The Defold Foundation
# Copyright 2014-2020 King
# Copyright 2009-2014 Ragnar Svensson, Christian Murray
# Licensed under the Defold License version 1.0 (the "License"); you may not use
# this file except in compliance with the License.
#
# You may obtain a copy of the License, together with FAQs at
# https://www.defold.com/license
#
# Unless required by applicable law or agreed to in writing, software distributed
# under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied. See the License for the
# specific language governing permissions and limitations under the License.

import argparse
import hashlib
import json
import pathlib
import shlex
import shutil
import subprocess
import sys
import tempfile


LICENSE = """// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the \"License\"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an \"AS IS\" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

"""

PACKAGE = "com.dynamo.bob.font.generated"
HEADER_CLASS = "FontRendererFFM"
SYMBOLS_CLASS = "FontRendererSymbols"
FUNCTIONS = (
    "FontcCompileStyle",
    "FontcFreeStyle",
    "FontcSetStyle",
    "FontcCreate",
    "FontcCreateGlyphBank",
    "FontcDestroy",
    "FontcParseMarkup",
    "FontcDestroyMarkup",
    "FontcGetMarkupData",
    "FontcMeasure",
    "FontcMeasureMarkup",
    "FontcMeasureParsedMarkup",
    "FontcFilterMarkup",
    "FontcGenerateGlyph",
    "FontcFreeGlyph",
    "FontcGetGlyphMetrics",
    "FontcGetSupportedGlyphMetrics",
    "FontcDecodeImage",
    "FontcFreeImage",
    "FontcSetProperties",
    "FontcSetText",
    "FontcSetMarkup",
    "FontcHash",
    "FontcBeginBatch",
    "FontcGenerateTexture",
    "FontcFreeTexture",
    "FontcGetVertexBufferSize",
    "FontcGetVertices",
)
STRUCTS = (
    "FontcStyle",
    "FontcStyleEffect",
    "FontcStyleData",
    "FontcParams",
    "FontcGlyphBankGlyph",
    "FontcLayout",
    "FontcGlyph",
    "FontcGlyphMetrics",
    "FontcImage",
    "FontcMarkupString",
    "FontcMarkupAttribute",
    "FontcMarkupNode",
    "FontcMarkupSpan",
    "FontcMarkupError",
    "FontcMarkupData",
    "FontcProperties",
    "FontcTexture",
)
TYPEDEFS = (
    "HFontRenderer",
    "HFontcMarkup",
    "FontRendererResult",
    "FontRendererLayer",
    "FontcMarkupErrorType",
)
CONSTANTS = (
    "FONT_RENDERER_RESULT_OK",
    "FONT_RENDERER_RESULT_INVALID_ARGUMENT",
    "FONT_RENDERER_RESULT_FONT_ERROR",
    "FONT_RENDERER_RESULT_TEXT_ERROR",
    "FONT_RENDERER_RESULT_GLYPH_ERROR",
    "FONT_RENDERER_RESULT_OUT_OF_MEMORY",
    "FONT_RENDERER_LAYER_FACE",
    "FONT_RENDERER_LAYER_OUTLINE",
    "FONT_RENDERER_LAYER_SHADOW",
    "FONTC_MARKUP_ERROR_NONE",
    "FONTC_MARKUP_ERROR_INCOMPLETE_TAG",
    "FONTC_MARKUP_ERROR_INCOMPLETE_ENTITY",
    "FONTC_MARKUP_ERROR_UNCLOSED_TAG",
    "FONTC_MARKUP_ERROR_INVALID_TAG",
    "FONTC_MARKUP_ERROR_INVALID_ATTRIBUTE",
    "FONTC_MARKUP_ERROR_INVALID_ENTITY",
    "FONTC_MARKUP_ERROR_UNEXPECTED_CLOSING_TAG",
    "FONTC_MARKUP_ERROR_MISMATCHED_CLOSING_TAG",
    "FONTC_MARKUP_ERROR_INVALID_UTF8",
    "FONTC_MARKUP_ERROR_LIMIT_EXCEEDED",
    "FONTC_MARKUP_ERROR_UNSUPPORTED",
    "FONTC_MARKUP_ERROR_UNKNOWN_TAG",
    "FONTC_MARKUP_ERROR_UNKNOWN_ATTRIBUTE",
    "FONTC_MARKUP_ERROR_INVALID_ATTRIBUTE_VALUE",
)
MANIFEST = "bindings.json"


def file_hash(path):
    return hashlib.sha256(path.read_bytes().replace(b"\r\n", b"\n")).hexdigest()


def write_if_changed(path, contents):
    if path.is_file() and path.read_text(encoding="utf-8") == contents:
        return False
    path.write_text(contents, encoding="utf-8")
    return True


def parse_args():
    parser = argparse.ArgumentParser(description="Generate Java 25 FFM bindings for the font renderer")
    parser.add_argument("--check", action="store_true", help="Verify that generated bindings match the header")
    parser.add_argument("--jextract", help="Path to the Java 25 jextract executable")
    parser.add_argument("--header", required=True, type=pathlib.Path, help="Front-facing C header")
    parser.add_argument("--include-dir", required=True, type=pathlib.Path, help="C header include directory")
    parser.add_argument("--output", required=True, type=pathlib.Path, help="Java source root")
    return parser.parse_args()


def main():
    args = parse_args()
    header_hash = file_hash(args.header)
    generated_package = args.output.joinpath(*PACKAGE.split("."))
    manifest_file = generated_package / MANIFEST
    expected_files = {f"{HEADER_CLASS}.java", f"{SYMBOLS_CLASS}.java"} | {f"{struct}.java" for struct in STRUCTS}
    if args.check:
        stale_files = []
        try:
            manifest = json.loads(manifest_file.read_text(encoding="utf-8"))
        except (FileNotFoundError, json.JSONDecodeError):
            manifest = {}
        if manifest.get("header_sha256") != header_hash:
            stale_files.append(MANIFEST)
        file_hashes = manifest.get("files", {})
        for filename in sorted(expected_files):
            output_file = generated_package / filename
            if not output_file.is_file() or file_hashes.get(filename) != file_hash(output_file):
                stale_files.append(filename)
        if stale_files:
            regeneration_command = shlex.join((
                sys.executable,
                str(pathlib.Path(__file__).resolve()),
                "--jextract", "/path/to/jextract",
                "--header", str(args.header.resolve()),
                "--include-dir", str(args.include_dir.resolve()),
                "--output", str(args.output.resolve()),
            ))
            raise SystemExit(
                "error: Font renderer FFM bindings are stale or missing: " + ", ".join(stale_files) +
                ".\nRegenerate them with Java 25 jextract (https://jdk.java.net/jextract/):\n  " +
                regeneration_command)
        return
    if not args.jextract:
        raise RuntimeError("--jextract is required when generating bindings")

    with tempfile.TemporaryDirectory(prefix="font-renderer-jextract-") as temporary_directory:
        generated_root = pathlib.Path(temporary_directory)
        command = [
            args.jextract,
            "--output", str(generated_root),
            "--target-package", PACKAGE,
            "--header-class-name", HEADER_CLASS,
            "--symbols-class-name", SYMBOLS_CLASS,
            "--include-dir", str(args.include_dir.resolve()),
        ]
        for function in FUNCTIONS:
            command.extend(("--include-function", function))
        for struct in STRUCTS:
            command.extend(("--include-struct", struct))
        for typedef in TYPEDEFS:
            command.extend(("--include-typedef", typedef))
        for constant in CONSTANTS:
            command.extend(("--include-constant", constant))
        command.append(str(args.header.resolve()))
        subprocess.run(command, check=True)

        generated_package = generated_root.joinpath(*PACKAGE.split("."))
        output_package = args.output.joinpath(*PACKAGE.split("."))
        output_package.mkdir(parents=True, exist_ok=True)
        generated_files = sorted(generated_package.glob("*.java"))
        if not generated_files:
            raise RuntimeError("jextract did not generate any Java sources")
        generated_names = {generated_file.name for generated_file in generated_files}
        for stale_file in output_package.glob("*.java"):
            if stale_file.name not in generated_names:
                stale_file.unlink()
        file_hashes = {}
        for generated_file in generated_files:
            contents = generated_file.read_text(encoding="utf-8")
            if generated_file.name == f"{SYMBOLS_CLASS}.java":
                # C long is 64-bit on Unix and 32-bit on Windows. The renderer API
                # uses fixed-width integers, so jextract's unused helper is removed.
                lines = contents.splitlines()
                c_long_lines = [line for line in lines if " C_LONG =" in line]
                if len(c_long_lines) != 1:
                    raise RuntimeError("jextract output did not contain exactly one C_LONG declaration")
                contents = "\n".join(line for line in lines if line != c_long_lines[0])
            output_file = output_package / generated_file.name
            contents = LICENSE + contents.rstrip() + "\n"
            if write_if_changed(output_file, contents):
                shutil.copymode(generated_file, output_file)
            file_hashes[generated_file.name] = file_hash(output_file)
        manifest = {"header_sha256": header_hash, "files": file_hashes}
        write_if_changed(manifest_file, json.dumps(manifest, indent=2, sort_keys=True) + "\n")


if __name__ == "__main__":
    main()
