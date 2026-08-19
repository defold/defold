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
import pathlib
import shutil
import subprocess
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
    "FontcCreate",
    "FontcDestroy",
    "FontcMeasure",
    "FontcGenerateGlyph",
    "FontcFreeGlyph",
    "FontcGetGlyphMetrics",
    "FontcGetSupportedGlyphMetrics",
    "FontcDecodeImage",
    "FontcFreeImage",
    "FontcSetProperties",
    "FontcSetText",
    "FontcHash",
    "FontcBeginBatch",
    "FontcGenerateTexture",
    "FontcFreeTexture",
    "FontcGetVertexBufferSize",
    "FontcGetVertices",
)
STRUCTS = (
    "FontcParams",
    "FontcLayout",
    "FontcGlyph",
    "FontcGlyphMetrics",
    "FontcImage",
    "FontcProperties",
    "FontcTexture",
)
TYPEDEFS = (
    "HFontRenderer",
    "FontRendererResult",
    "FontRendererLayer",
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
)
HEADER_HASH_PREFIX = "// Font renderer header SHA-256: "


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
    header_bytes = args.header.read_bytes().replace(b"\r\n", b"\n")
    header_hash = hashlib.sha256(header_bytes).hexdigest()
    generated_package = args.output.joinpath(*PACKAGE.split("."))
    expected_files = {f"{HEADER_CLASS}.java", f"{SYMBOLS_CLASS}.java"} | {f"{struct}.java" for struct in STRUCTS}
    if args.check:
        stale_files = []
        expected_hash_line = HEADER_HASH_PREFIX + header_hash
        for filename in sorted(expected_files):
            output_file = generated_package / filename
            if not output_file.is_file():
                stale_files.append(filename)
                continue
            output_lines = output_file.read_text(encoding="utf-8").splitlines()
            hash_lines = [line for line in output_lines if line.startswith(HEADER_HASH_PREFIX)]
            has_platform_dependent_long = (filename == f"{SYMBOLS_CLASS}.java" and
                                           any(" C_LONG =" in line for line in output_lines))
            if hash_lines != [expected_hash_line] or has_platform_dependent_long:
                stale_files.append(filename)
        if stale_files:
            raise SystemExit(
                "error: Font renderer FFM bindings are stale or missing: " + ", ".join(stale_files) +
                ". Regenerate them with the generate_font_renderer_ffm target and a jextract executable.")
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
            output_file.write_text(LICENSE + HEADER_HASH_PREFIX + header_hash + "\n\n" + contents.rstrip() + "\n", encoding="utf-8")
            shutil.copymode(generated_file, output_file)


if __name__ == "__main__":
    main()
