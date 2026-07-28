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

# Symbolicate a production HTML5 callstack (the "CALL STACK:" block logged to the
# browser console / stored in the dmCrash dump) against the wasm debug sidecar.
#
#   symbolicate_web_callstack.py --wasm-debug Game.wasm.debug.wasm stack.txt
#
# Frames with a byte offset (Chrome/Firefox, "wasm-function[123]:0x4567") are
# resolved to function + file:line:column (including inlining) with
# llvm-symbolizer against the DWARF in the .wasm.debug.wasm sidecar — the
# sidecar is a full wasm module, so it is symbolized directly; the deployed
# .wasm is not needed. Offset-less frames (Safari, "wasm-function[123]") are
# resolved to a function name via the --emit-symbol-map output (.js.symbols).
# The symbol map also names offset frames whose DWARF lacks subprogram info
# (release builds carry -gline-tables-only, which has file:line but no names).

import argparse
import glob
import os
import pathlib
import re
import subprocess
import sys


SYMBOL_ESCAPE_RE = re.compile(r"\\([0-9A-Fa-f]{2})")
# Chrome:  at $func123 (https://host/Game.wasm:wasm-function[123]:0x4567)
# Firefox: name@https://host/Game.wasm:wasm-function[123]:0x4567
WASM_OFFSET_FRAME_RE = re.compile(r"wasm-function\[(\d+)\]:(0x[0-9A-Fa-f]+)")
# Safari: <?>.wasm-function[123]@[wasm code]
WASM_INDEX_FRAME_RE = re.compile(r"wasm-function\[(\d+)\]")


def decode_symbol_name(name):
    return SYMBOL_ESCAPE_RE.sub(lambda match: chr(int(match.group(1), 16)), name)


def load_symbol_map(symbols_path):
    symbol_map = {}
    with symbols_path.open("r", encoding="utf-8") as symbols_file:
        for line in symbols_file:
            line = line.rstrip("\n")
            if not line:
                continue
            index_text, name = line.split(":", 1)
            symbol_map[int(index_text)] = decode_symbol_name(name)
    return symbol_map


def infer_symbols_path(wasm_debug_path):
    # <Game>.wasm.debug.wasm -> <Game>.js.symbols (next to the sidecar)
    name = wasm_debug_path.name
    for suffix in (".wasm.debug.wasm", ".debug.wasm", ".wasm"):
        if name.endswith(suffix):
            return wasm_debug_path.parent / (name[: -len(suffix)] + ".js.symbols")
    return wasm_debug_path.parent / (name + ".js.symbols")


def find_emsdk():
    emsdk = os.environ.get("EMSDK")
    if emsdk and os.path.isdir(emsdk):
        return emsdk
    roots = []
    dynamo_home = os.environ.get("DYNAMO_HOME")
    if dynamo_home:
        roots.append(pathlib.Path(dynamo_home))
    # tmp/dynamo_home in a defold checkout, relative to this script
    roots.append(pathlib.Path(__file__).resolve().parent.parent.parent / "tmp" / "dynamo_home")
    for root in roots:
        candidates = sorted(glob.glob(str(root / "ext" / "SDKs" / "emsdk-*")))
        if candidates:
            return candidates[-1]
    return None


def find_llvm_tool(emsdk, name):
    if emsdk:
        tool = pathlib.Path(emsdk) / "upstream" / "bin" / name
        if tool.is_file():
            return str(tool)
    from shutil import which
    return which(name)


def read_leb(data, i):
    result = shift = 0
    while True:
        byte = data[i]
        i += 1
        result |= (byte & 0x7F) << shift
        if not (byte & 0x80):
            return result, i
        shift += 7


def get_code_section_offset(wasm_path):
    # Returns the file offset of the code section payload: DWARF addresses are
    # code-section-relative while browser stack offsets are file-relative.
    data = wasm_path.read_bytes()
    if data[:4] != b"\0asm":
        raise ValueError(f"not a wasm file: {wasm_path}")
    i = 8
    while i < len(data):
        section_id = data[i]
        i += 1
        size, i = read_leb(data, i)
        if section_id == 10:  # code section
            return i
        i += size
    raise ValueError(f"no code section found in {wasm_path}")


def run_symbolizer(symbolizer, wasm_debug_path, offsets):
    # llvm-symbolizer output per address: pairs of "function" / "file:line:col"
    # lines (multiple pairs when inlined), addresses separated by a blank line.
    code_offset = get_code_section_offset(wasm_debug_path)
    cmd = [symbolizer, "-e", str(wasm_debug_path), f"--adjust-vma={code_offset}"]
    cmd += [str(offset) for offset in offsets]
    result = subprocess.run(cmd, check=True, capture_output=True, text=True)
    blocks = [block for block in result.stdout.split("\n\n") if block.strip()]
    resolved = {}
    for offset, block in zip(offsets, blocks):
        lines = block.splitlines()
        frames = []
        for index in range(0, len(lines) - 1, 2):
            function = lines[index].strip()
            location = lines[index + 1].strip()
            if function or location:
                frames.append((function or "??", location or "??"))
        resolved[offset] = frames
    return resolved


def demangle(names, cxxfilt):
    if not cxxfilt or not names:
        return {name: name for name in names}
    try:
        result = subprocess.run(
            [cxxfilt], input="\n".join(names), capture_output=True, text=True, check=True
        )
        demangled = result.stdout.splitlines()
        if len(demangled) == len(names):
            return dict(zip(names, demangled))
    except (OSError, subprocess.CalledProcessError):
        pass
    return {name: name for name in names}


def main():
    parser = argparse.ArgumentParser(
        description="Symbolicate a browser wasm callstack using the .wasm.debug.wasm sidecar."
    )
    parser.add_argument(
        "stack",
        nargs="?",
        help="Path to a text file with the callstack. Reads stdin if omitted or '-'.",
    )
    parser.add_argument(
        "--wasm-debug",
        required=True,
        help="Path to the DWARF sidecar (<Game>.wasm.debug.wasm) from the same build.",
    )
    parser.add_argument(
        "--symbols",
        help="Path to the .js.symbols symbol map, used for offset-less (Safari) frames. "
        "Defaults to <Game>.js.symbols next to the sidecar.",
    )
    parser.add_argument(
        "--emsdk",
        help="Emscripten SDK root providing llvm-symbolizer. Defaults to $EMSDK, then "
        "$DYNAMO_HOME/ext/SDKs/emsdk-*.",
    )
    args = parser.parse_args()

    wasm_debug_path = pathlib.Path(args.wasm_debug).resolve()
    if not wasm_debug_path.is_file():
        print(f"error: wasm debug file not found: {wasm_debug_path}", file=sys.stderr)
        return 1

    if args.stack and args.stack != "-":
        stack_text = pathlib.Path(args.stack).read_text(encoding="utf-8", errors="replace")
    else:
        stack_text = sys.stdin.read()
    stack_lines = stack_text.splitlines()

    emsdk = args.emsdk or find_emsdk()
    symbolizer = find_llvm_tool(emsdk, "llvm-symbolizer")
    cxxfilt = find_llvm_tool(emsdk, "llvm-cxxfilt") or find_llvm_tool(None, "c++filt")

    symbols_path = pathlib.Path(args.symbols).resolve() if args.symbols else infer_symbols_path(wasm_debug_path)
    symbol_map = {}
    if symbols_path.is_file():
        symbol_map = load_symbol_map(symbols_path)

    offsets = []
    for line in stack_lines:
        match = WASM_OFFSET_FRAME_RE.search(line)
        if match:
            offset = int(match.group(2), 16)
            if offset not in offsets:
                offsets.append(offset)

    resolved = {}
    if offsets:
        if not symbolizer:
            print(
                "error: llvm-symbolizer not found; pass --emsdk or set $EMSDK",
                file=sys.stderr,
            )
            return 1
        resolved = run_symbolizer(symbolizer, wasm_debug_path, offsets)

    index_names = {}
    if symbol_map:
        mangled = sorted({name for name in symbol_map.values()})
        demangled = demangle(mangled, cxxfilt)
        index_names = {index: demangled[name] for index, name in symbol_map.items()}
    elif any(WASM_INDEX_FRAME_RE.search(line) for line in stack_lines):
        print(
            f"warning: symbol map not found ({symbols_path}); function names limited "
            "to what the DWARF carries — pass --symbols",
            file=sys.stderr,
        )

    for line in stack_lines:
        print(line)
        index_match = WASM_INDEX_FRAME_RE.search(line)
        map_name = index_names.get(int(index_match.group(1))) if index_match else None
        offset_match = WASM_OFFSET_FRAME_RE.search(line)
        if offset_match:
            frames = resolved.get(int(offset_match.group(2), 16), [])
            # the innermost frame is the function itself: name it from the symbol
            # map when the DWARF has no subprogram info (-gline-tables-only)
            for frame_index, (function, location) in enumerate(frames):
                if function == "??" and map_name and frame_index == len(frames) - 1:
                    function = map_name
                print(f"    -> {function} at {location}")
            if not frames and map_name:
                print(f"    -> {map_name}")
        elif map_name:
            print(f"    -> {map_name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
