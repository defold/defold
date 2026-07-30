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
    "FontRendererCreate",
    "FontRendererDestroy",
    "FontRendererMeasure",
    "FontRendererGenerateGlyph",
    "FontRendererFreeGlyph",
    "FontRendererRender",
    "FontRendererFreeRenderResult",
)
STRUCTS = (
    "FontRendererParams",
    "FontRendererLayout",
    "FontRendererGlyph",
    "FontRendererRenderResult",
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


def parse_args():
    parser = argparse.ArgumentParser(description="Generate Java 25 FFM bindings for the font renderer")
    parser.add_argument("--jextract", required=True, help="Path to the Java 25 jextract executable")
    parser.add_argument("--header", required=True, type=pathlib.Path, help="Front-facing C header")
    parser.add_argument("--output", required=True, type=pathlib.Path, help="Java source root")
    return parser.parse_args()


def main():
    args = parse_args()
    with tempfile.TemporaryDirectory(prefix="font-renderer-jextract-") as temporary_directory:
        generated_root = pathlib.Path(temporary_directory)
        command = [
            args.jextract,
            "--output", str(generated_root),
            "--target-package", PACKAGE,
            "--header-class-name", HEADER_CLASS,
            "--symbols-class-name", SYMBOLS_CLASS,
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
            output_file = output_package / generated_file.name
            output_file.write_text(LICENSE + contents, encoding="utf-8")
            shutil.copymode(generated_file, output_file)


if __name__ == "__main__":
    main()
