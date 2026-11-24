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

"""Extract the portable SIMDe functions used by the tests; not part of the build."""

import argparse
from pathlib import Path
import re
import subprocess
import tempfile


def strip_annotation(source, name):
    pattern = re.compile(r"\b" + name + r"\s*\(")
    while match := pattern.search(source):
        end = match.end()
        depth = 1
        while depth:
            depth += (source[end] == "(") - (source[end] == ")")
            end += 1
        source = source[:match.start()] + source[end:]
    return source


def declarations(source):
    result = {}
    for match in re.finditer(r"\b(?:inline static|static inline)\s+[^;{}]+?\b(simde_\w+)\s*\([^;{}]*\)\s*\{", source):
        end, depth = match.end(), 1
        while depth:
            depth += (source[end] == "{") - (source[end] == "}")
            end += 1
        result[match.group(1)] = (match.start(), source[match.start():end])
    for match in re.finditer(r"\btypedef\s+", source):
        end, depth = match.end(), 0
        while source[end] != ";" or depth:
            depth += (source[end] in "({[") - (source[end] in ")}]")
            end += 1
        code = source[match.start():end + 1]
        if name := re.search(r"\b(simde_\w+)\s*;$", code):
            result[name.group(1)] = (match.start(), code)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("simde", type=Path, help="SIMDe checkout at the revision in simde.h")
    parser.add_argument("--clang", default="clang")
    parser.add_argument("--clang-format", default="clang-format")
    args = parser.parse_args()
    output_dir = Path(__file__).resolve().parent
    tests_dir = output_dir.parent
    tests = (tests_dir / "test_simd_simde.c").read_text() + (tests_dir / "test_simd_wide_emulate.h").read_text()
    roots = set(re.findall(r"\bsimde_\w+\b", tests))
    roots.update(("simde_mm_set_ps1", "simde_mm_bsrli_si128", "simde_mm_unpacklo_ps",
                  "simde_mm_unpackhi_ps", "simde_mm_movelh_ps", "simde_mm_movehl_ps"))
    for width in ("256", "512"):
        roots.update("simde_mm" + width + "_" + name
                     for name in re.findall(r"WIDE_REF\((\w+)\)", tests) if name != "name")
    with tempfile.TemporaryDirectory() as temp_dir:
        source = Path(temp_dir) / "reference.c"
        source.write_text("#define SIMDE_NO_NATIVE\n#define SIMDE_NO_VECTOR\n"
                          "#include <simde/x86/avx512.h>\n#include <simde/x86/fma.h>\n"
                          "#include <simde/arm/neon.h>\n")
        source = subprocess.check_output([args.clang, "-E", "-P", "-Wno-builtin-macro-redefined",
                                          "-D__has_builtin(x)=0", "-D__has_attribute(x)=0",
                                          "-I", str(args.simde), str(source)], text=True)
    source = strip_annotation(strip_annotation(source, "__attribute__"), "_Alignas")
    source = source.replace("__inline__", "inline")
    available = declarations(source)
    selected = {}
    pending = list(roots)
    while pending:
        name = pending.pop()
        if name in selected or name not in available:
            continue
        selected[name] = available[name]
        pending.extend(re.findall(r"\bsimde_\w+\b", selected[name][1]))
    # These aliases are unreferenced fields in upstream register unions.
    unused = ("simde_int128", "simde_uint128", "simde_float16")
    selected = {name: value for name, value in selected.items() if name not in unused}
    parts = []
    for _, code in sorted(selected.values()):
        # The pinned upstream OR loop indexes fast lanes, which can be 64 bits
        # on Linux even when int32_t lanes are 32 bits.
        if re.search(r"\bsimde_mm512_or_si512\s*\(", code):
            code = re.sub(r"\br_\.i32\b", "r_.i32f", code)
        code = "\n".join(line for line in code.splitlines()
                         if not any(field in line for field in
                                    ("simde_int128 i128", "simde_uint128 u128", "simde_float16 f16")))
        code = re.sub(r"^\s*#pragma[^\n]*\n", "\n", code, flags=re.M)
        for builtin in ("memcpy", "fmaf", "sqrtf"):
            code = code.replace("__builtin_" + builtin, builtin)
        parts.append(code)

    # Preserve all copyright notices from headers containing extracted declarations.
    notices = set()
    for header in (args.simde / "simde").rglob("*.h"):
        original = header.read_text()
        if not any(re.search(r"\b" + name + r"\s*(?:\(|;)", original) for name in selected):
            continue
        comment = original.split("*/", 1)[0]
        if "Copyright:" in comment:
            notices.update(line.strip(" *") for line in comment.split("Copyright:", 1)[1].splitlines() if line.strip(" *"))
        else:
            notices.update(line.strip(" *") for line in comment.splitlines() if "Copyright" in line)
    license_text = (args.simde / "COPYING").read_text()
    license_text = license_text.replace(license_text.splitlines()[0], "Copyright notices from SIMDe:\n" + "\n".join(sorted(notices)))
    header = "/* Generated test-only scalar subset of SIMDe.\n"
    header += " * Source: https://github.com/simd-everywhere/simde/tree/0ff5341927e15df0c3da8bb13bb32ec7f95aef84\n"
    header += " * Preprocessed with SIMDE_NO_NATIVE and SIMDE_NO_VECTOR; compiler annotations\n"
    header += " * and unused half/128-bit integer union fields removed.\n"
    header += " * Regenerate: python3 engine/dlib/src/test/simde/update_reference.py /path/to/simde\n"
    header += " *\n" + "\n".join(" * " + line if line else " *" for line in license_text.splitlines()) + "\n */\n\n"
    header += "#ifndef DM_TEST_SIMDE_REFERENCE_H\n#define DM_TEST_SIMDE_REFERENCE_H\n\n"
    header += "#include <stdint.h>\n#include <stddef.h>\n#include <string.h>\n#include <math.h>\n\n"
    header += "\n\n".join(parts) + "\n\n"
    header += "#define simde_mm_set1_ps simde_mm_set_ps1\n#define simde_mm_srli_si128 simde_mm_bsrli_si128\n"
    header += """#define SIMDE_MM_TRANSPOSE4_PS(row0, row1, row2, row3) \\
    do { \\
        simde__m128 tmp0 = simde_mm_unpacklo_ps(row0, row1); \\
        simde__m128 tmp1 = simde_mm_unpackhi_ps(row0, row1); \\
        simde__m128 tmp2 = simde_mm_unpacklo_ps(row2, row3); \\
        simde__m128 tmp3 = simde_mm_unpackhi_ps(row2, row3); \\
        row0 = simde_mm_movelh_ps(tmp0, tmp2); \\
        row1 = simde_mm_movehl_ps(tmp2, tmp0); \\
        row2 = simde_mm_movelh_ps(tmp1, tmp3); \\
        row3 = simde_mm_movehl_ps(tmp3, tmp1); \\
    } while (0)

#endif
"""
    output = output_dir / "simde.h"
    output.write_text(header)
    subprocess.check_call([args.clang_format, "-i", str(output)])
    subprocess.check_call([args.clang, "-std=c99", "-x", "c", "-fsyntax-only", str(output)])
    print(f"Wrote {output.stat().st_size} bytes; {len(selected)} declarations")


if __name__ == "__main__":
    main()
