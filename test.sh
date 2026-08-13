#!/usr/bin/env bash
set -euo pipefail

if [ -z "${AR:-}" ]; then
    echo "Error: set AR to your llvm-ar path (e.g. AR=\$(which llvm-ar))." >&2
    exit 1
fi

if ! command -v wasm-objdump >/dev/null 2>&1; then
    echo "Error: wasm-objdump not found in PATH." >&2
    exit 1
fi

if [ -z "${DIS:-}" ]; then
    echo "Error: set DIS to your llvm-dis path (e.g. DIS=\$(which llvm-dis))." >&2
    exit 1
fi

orig_dir=$(pwd)
dirs=(
    # "tmp/dynamo_home/ext/lib/wasm_pthread-web"
    # "tmp/dynamo_home/lib/wasm_pthread-web"
    "/Users/mathiaswesterdahl/work/projects/users/defold/extension-rive/defold-rive/lib/wasm_pthread-web"
)

missing=()

workdir="./tempobj"
rm -rf "$workdir"
mkdir -p "$workdir"

for dir in "${dirs[@]}"; do
    if [ ! -d "$dir" ]; then
        echo "Skipping missing directory: $dir"
        continue
    fi

    shopt -s nullglob
    for lib in "$dir"/*.a; do
        echo "Checking $lib"
        libtmp="$workdir/$(basename "$lib")"
        rm -rf "$libtmp"
        mkdir -p "$libtmp"
        if [[ "$lib" = /* ]]; then
            libpath="$lib"
        else
            libpath="$orig_dir/$lib"
        fi
        ( cd "$libtmp" && "$AR" x "$libpath" >/dev/null )

        bad=false
        while IFS= read -r -d '' obj; do
            if file "$obj" | grep -qi "bitcode"; then
                if ! ir=$("$DIS" "$obj" -o - 2>/dev/null); then
                    echo "  Failed to disassemble bitcode: $obj"
                    bad=true
                    continue
                fi
                if ! printf '%s' "$ir" | grep -q "target-features.*+atomics"; then
                    echo "  Missing +atomics (bitcode): $obj"
                    bad=true
                fi
                if ! printf '%s' "$ir" | grep -q "target-features.*+bulk-memory"; then
                    echo "  Missing +bulk-memory (bitcode): $obj"
                    bad=true
                fi
            else
                if ! output=$(wasm-objdump -x "$obj" 2>/dev/null); then
                    echo "  Failed to inspect $obj (wasm-objdump error)."
                    bad=true
                    continue
                fi
                if ! printf '%s' "$output" | grep -Eq "\\[\\+\\] atomics|\\+atomics"; then
                    echo "  Missing +atomics: $obj"
                    bad=true
                fi
                if ! printf '%s' "$output" | grep -Eq "\\[\\+\\] bulk-memory|\\+bulk-memory"; then
                    echo "  Missing +bulk-memory: $obj"
                    bad=true
                fi
            fi
        done < <(find "$libtmp" -type f \( -name '*.o' -o -name '*.obj' \) -print0)

        if [ "$bad" = true ]; then
            missing+=("$lib")
        fi
    done
    shopt -u nullglob
done

if [ ${#missing[@]} -eq 0 ]; then
    echo "All archives have +atomics and +bulk-memory."
else
    echo "Archives with missing features:"
    for lib in "${missing[@]}"; do
        echo "  $lib"
    done
    exit 1
fi
