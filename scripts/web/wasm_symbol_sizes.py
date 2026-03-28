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
import os
import pathlib
import re
import subprocess
import sys


SYMBOL_ESCAPE_RE = re.compile(r"\\([0-9A-Fa-f]{2})")
FUNC_RE = re.compile(r"^func\[(\d+)\]$")


def decode_symbol_name(name):
    return SYMBOL_ESCAPE_RE.sub(lambda match: chr(int(match.group(1), 16)), name)


def infer_symbols_path(wasm_path):
    if wasm_path.suffix == ".wasm":
        return wasm_path.with_suffix(".js.symbols")
    return wasm_path.parent / (wasm_path.name + ".js.symbols")


def infer_output_path(wasm_path):
    return pathlib.Path(str(wasm_path) + ".symbol-sizes.tsv")


def infer_wasm_map_path(wasm_path):
    return pathlib.Path(str(wasm_path) + ".map")


def find_default_wasm(dynamo_home, platform, engine):
    bin_dir = pathlib.Path(dynamo_home).resolve() / "bin" / platform

    if engine:
        wasm_path = bin_dir / f"{engine}.wasm"
        if not wasm_path.is_file():
            raise FileNotFoundError(f"wasm file not found: {wasm_path}")
        return wasm_path

    preferred_names = [
        "dmengine_release.wasm",
        "dmengine.wasm",
        "dmengine_headless.wasm",
    ]

    for name in preferred_names:
        wasm_path = bin_dir / name
        if wasm_path.is_file():
            return wasm_path

    candidates = sorted(bin_dir.glob("*.wasm"))
    if len(candidates) == 1:
        return candidates[0]
    if len(candidates) > 1:
        candidate_list = ", ".join(candidate.name for candidate in candidates)
        raise FileNotFoundError(
            f"multiple wasm files found in {bin_dir}, specify --engine or a wasm path: {candidate_list}"
        )
    raise FileNotFoundError(f"no wasm files found in {bin_dir}")


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


def run_bloaty(wasm_path, bloaty):
    try:
        result = subprocess.run(
            [bloaty, "--tsv", "-n", "0", "-d", "symbols", str(wasm_path)],
            check=True,
            capture_output=True,
            text=True,
        )
    except FileNotFoundError:
        print(f"error: '{bloaty}' was not found in PATH", file=sys.stderr)
        return None
    except subprocess.CalledProcessError as error:
        if error.stderr:
            print(error.stderr.strip(), file=sys.stderr)
        else:
            print("error: bloaty failed", file=sys.stderr)
        return None
    return result.stdout


def extract_function_rows(bloaty_tsv, symbol_map):
    reverse_symbol_map = {}
    for func_index, name in symbol_map.items():
        reverse_symbol_map.setdefault(name, []).append(func_index)

    rows = []
    lines = bloaty_tsv.splitlines()
    for line in lines[1:]:
        columns = line.split("\t")
        if len(columns) != 3:
            continue
        symbol = columns[0]
        if symbol.startswith("[section "):
            continue

        file_size_bytes = int(columns[2])
        match = FUNC_RE.match(symbol)
        if match:
            func_index = int(match.group(1))
            name = symbol_map.get(func_index, symbol)
        else:
            indices = reverse_symbol_map.get(symbol, [])
            func_index = indices[0] if len(indices) == 1 else -1
            name = symbol
        rows.append((file_size_bytes, func_index, name))

    rows.sort(key=lambda row: (-row[0], row[1], row[2]))
    return rows


def write_output(rows, output_path):
    with output_path.open("w", encoding="utf-8", newline="") as output_file:
        output_file.write("file_size_bytes\tfunc_index\tname\n")
        for file_size_bytes, func_index, name in rows:
            output_file.write(f"{file_size_bytes}\t{'' if func_index < 0 else func_index}\t{name}\n")


def print_top_rows(rows, top_count):
    if top_count <= 0 or not rows:
        return

    print(f"Top {min(top_count, len(rows))} functions:")
    for file_size_bytes, func_index, name in rows[:top_count]:
        print(f"{file_size_bytes:>8}  func[{func_index}]  {name}")


def main():
    parser = argparse.ArgumentParser(
        description="Generate a size-sorted TSV for functions in a WebAssembly binary."
    )
    parser.add_argument(
        "wasm",
        nargs="?",
        help="Path to the .wasm file. If omitted, resolve it from $DYNAMO_HOME/bin/<platform>/.",
    )
    parser.add_argument(
        "--symbols",
        help="Path to the matching .js.symbols file. Defaults to <wasm>.with_suffix('.js.symbols').",
    )
    parser.add_argument(
        "--output",
        help="Path to the output TSV. Defaults to <wasm>.symbol-sizes.tsv.",
    )
    parser.add_argument(
        "--bloaty",
        default="bloaty",
        help="Bloaty executable to use. Defaults to 'bloaty'.",
    )
    parser.add_argument(
        "--platform",
        default="wasm-web",
        help="Platform subdirectory under $DYNAMO_HOME/bin when wasm is omitted. Defaults to 'wasm-web'.",
    )
    parser.add_argument(
        "--engine",
        help="Engine basename under $DYNAMO_HOME/bin/<platform>/, for example 'dmengine_release'.",
    )
    parser.add_argument(
        "--top",
        type=int,
        default=0,
        help="Print the N biggest functions after writing the TSV. Defaults to 0.",
    )
    args = parser.parse_args()

    if args.wasm:
        wasm_path = pathlib.Path(args.wasm).resolve()
        if not wasm_path.is_file():
            print(f"error: wasm file not found: {wasm_path}", file=sys.stderr)
            return 1
    else:
        dynamo_home = os.environ.get("DYNAMO_HOME")
        if not dynamo_home:
            print("error: either pass a wasm path or set DYNAMO_HOME", file=sys.stderr)
            return 1
        try:
            wasm_path = find_default_wasm(dynamo_home, args.platform, args.engine)
        except FileNotFoundError as error:
            print(f"error: {error}", file=sys.stderr)
            return 1

    symbols_path = pathlib.Path(args.symbols).resolve() if args.symbols else infer_symbols_path(wasm_path)
    output_path = pathlib.Path(args.output).resolve() if args.output else infer_output_path(wasm_path)
    wasm_map_path = infer_wasm_map_path(wasm_path)

    symbol_map = {}
    if symbols_path.is_file():
        symbol_map = load_symbol_map(symbols_path)
    else:
        print(f"warning: symbol map not found, using raw func[N] names: {symbols_path}", file=sys.stderr)

    if not wasm_map_path.is_file():
        print(
            f"note: source map not found: {wasm_map_path}. "
            f"Rebuild with --wasm-size-analysis if you want source-level attribution.",
            file=sys.stderr,
        )

    bloaty_tsv = run_bloaty(wasm_path, args.bloaty)
    if bloaty_tsv is None:
        return 1

    rows = extract_function_rows(bloaty_tsv, symbol_map)
    write_output(rows, output_path)

    print(f"Wrote {len(rows)} function rows to {output_path}")
    print_top_rows(rows, args.top)
    return 0


if __name__ == "__main__":
    sys.exit(main())
