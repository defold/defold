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
#   symbolicate_web_callstack.py --symbols Game_wasm.js.symbols stack.txt
#
# Frames with a byte offset (Chrome/Firefox, "wasm-function[123]:0x4567") are
# resolved to file:line:column with llvm-symbolizer against the DWARF in the
# .wasm.debug.wasm sidecar — the sidecar is a full wasm module, so it is
# symbolized directly; the deployed .wasm is not needed. Offset-less frames
# (Safari, "wasm-function[123]") and builds without a sidecar only get a
# function name.
#
# Function names come from the frame's function index, via the --emit-symbol-map
# output (.js.symbols) or the name section of the sidecar. llvm-symbolizer is not
# trusted for them: it matches --adjust-vma addresses against file-relative name
# section symbols and reports a neighbouring function. Inlined frames are not
# printed either: on optimized code the line table stays accurate while the
# inlined subroutine DIEs can point at unrelated functions.

import argparse
import glob
import json
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
    name = wasm_debug_path.name
    for suffix in (".wasm.debug.wasm", ".debug.wasm", ".wasm"):
        if name.endswith(suffix):
            name = name[: -len(suffix)]
            break
    # bundle layout: <Game>/<Game>[_pthread].wasm.debug.wasm and
    # <Game>_symbols/<Game>[_pthread]_wasm.js.symbols next to the bundle folder
    title = name[: -len("_pthread")] if name.endswith("_pthread") else name
    bundled = wasm_debug_path.parent.parent / f"{title}_symbols" / f"{name}_wasm.js.symbols"
    return bundled if bundled.is_file() else wasm_debug_path.parent / f"{name}.js.symbols"


def expand_stack_lines(stack_text):
    # The engine crash handler logs the frames as a single line JSON array, which
    # may share its line with the log prefix and the Lua callstack following it.
    decoder = json.JSONDecoder()
    lines = []
    for line in stack_text.splitlines():
        start = line.find('["')
        frames = None
        if start != -1:
            try:
                frames, end = decoder.raw_decode(line, start)
            except ValueError:
                frames = None
        if isinstance(frames, list) and all(isinstance(frame, str) for frame in frames):
            lines += [text for text in (line[:start], *frames, line[end:]) if text.strip()]
        else:
            lines.append(line)
    return lines


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


def iter_sections(data, wasm_path):
    if data[:4] != b"\0asm":
        raise ValueError(f"not a wasm file: {wasm_path}")
    i = 8
    while i < len(data):
        section_id = data[i]
        size, payload = read_leb(data, i + 1)
        yield section_id, payload, payload + size
        i = payload + size


def get_code_section_offset(data, wasm_path):
    # Returns the file offset of the code section payload: DWARF addresses are
    # code-section-relative while browser stack offsets are file-relative.
    for section_id, payload, _ in iter_sections(data, wasm_path):
        if section_id == 10:  # code section
            return payload
    raise ValueError(f"no code section found in {wasm_path}")


def load_name_section(data, wasm_path):
    # function index -> name from the function names of the name custom section
    names = {}
    for section_id, payload, end in iter_sections(data, wasm_path):
        if section_id != 0:
            continue
        length, i = read_leb(data, payload)
        if data[i : i + length] != b"name":
            continue
        i += length
        while i < end:
            subsection = data[i]
            size, i = read_leb(data, i + 1)
            if subsection == 1:
                count, cursor = read_leb(data, i)
                for _ in range(count):
                    index, cursor = read_leb(data, cursor)
                    length, cursor = read_leb(data, cursor)
                    names[index] = data[cursor : cursor + length].decode("utf-8", "replace")
                    cursor += length
            i += size
    return names


def run_symbolizer(symbolizer, wasm_debug_path, code_offset, offsets):
    # llvm-symbolizer output per address: a "function" and a "file:line:col" line,
    # addresses separated by a blank line.
    cmd = [symbolizer, "-e", str(wasm_debug_path), f"--adjust-vma={code_offset}", "--no-inlines"]
    cmd += [str(offset) for offset in offsets]
    result = subprocess.run(cmd, check=True, capture_output=True, text=True)
    blocks = [block for block in result.stdout.split("\n\n") if block.strip()]
    resolved = {}
    for offset, block in zip(offsets, blocks):
        lines = block.splitlines()
        if len(lines) >= 2:
            resolved[offset] = (lines[0].strip() or "??", lines[1].strip() or "??")
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
        help="Path to the DWARF sidecar (<Game>.wasm.debug.wasm) from the same build. "
        "Without it, frames only get a function name from --symbols.",
    )
    parser.add_argument(
        "--symbols",
        help="Path to the .js.symbols symbol map, the source of function names. Defaults to "
        "<Game>_symbols/<Game>_wasm.js.symbols next to the bundle folder, then to "
        "<Game>.js.symbols next to the sidecar, then to the name section of the sidecar.",
    )
    parser.add_argument(
        "--emsdk",
        help="Emscripten SDK root providing llvm-symbolizer. Defaults to $EMSDK, then "
        "$DYNAMO_HOME/ext/SDKs/emsdk-*.",
    )
    args = parser.parse_args()

    if not args.wasm_debug and not args.symbols:
        parser.error("pass --wasm-debug, --symbols or both")

    wasm_debug_path = pathlib.Path(args.wasm_debug).resolve() if args.wasm_debug else None
    if wasm_debug_path and not wasm_debug_path.is_file():
        print(f"error: wasm debug file not found: {wasm_debug_path}", file=sys.stderr)
        return 1

    symbols_path = pathlib.Path(args.symbols).resolve() if args.symbols else infer_symbols_path(wasm_debug_path)
    if args.symbols and not symbols_path.is_file():
        print(f"error: symbol map not found: {symbols_path}", file=sys.stderr)
        return 1

    if args.stack and args.stack != "-":
        stack_text = pathlib.Path(args.stack).read_text(encoding="utf-8", errors="replace")
    else:
        stack_text = sys.stdin.read()
    stack_lines = expand_stack_lines(stack_text)

    emsdk = args.emsdk or find_emsdk()
    symbolizer = find_llvm_tool(emsdk, "llvm-symbolizer")
    cxxfilt = find_llvm_tool(emsdk, "llvm-cxxfilt") or find_llvm_tool(None, "c++filt")

    symbol_map = {}
    if symbols_path.is_file():
        symbol_map = load_symbol_map(symbols_path)

    wasm_debug_data = wasm_debug_path.read_bytes() if wasm_debug_path else None

    offsets = []
    if wasm_debug_path:
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
        code_offset = get_code_section_offset(wasm_debug_data, wasm_debug_path)
        resolved = run_symbolizer(symbolizer, wasm_debug_path, code_offset, offsets)

    index_names = {}
    if symbol_map:
        mangled = sorted({name for name in symbol_map.values()})
        demangled = demangle(mangled, cxxfilt)
        index_names = {index: demangled[name] for index, name in symbol_map.items()}
    else:
        index_names = load_name_section(wasm_debug_data, wasm_debug_path)
        if not index_names and any(WASM_INDEX_FRAME_RE.search(line) for line in stack_lines):
            print(
                f"warning: symbol map not found ({symbols_path}) and the sidecar has no name "
                "section; function names may be wrong — pass --symbols",
                file=sys.stderr,
            )

    for line in stack_lines:
        print(line)
        index_match = WASM_INDEX_FRAME_RE.search(line)
        name = index_names.get(int(index_match.group(1))) if index_match else None
        offset_match = WASM_OFFSET_FRAME_RE.search(line)
        frame = resolved.get(int(offset_match.group(2), 16)) if offset_match else None
        if frame:
            print(f"    -> {name or frame[0]} at {frame[1]}")
        elif name:
            print(f"    -> {name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
