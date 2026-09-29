# Copyright 2026 The Defold Foundation
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

"""Measure unsanitized archives and equivalent linked ECS examples on macOS."""

import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import platform
import re
import subprocess
import tempfile


def one(build, pattern):
    paths = list(build.glob(pattern))
    if len(paths) != 1:
        raise RuntimeError(f'Expected one {pattern} under {build}, found {len(paths)}')
    return paths[0]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def add_source_counts(result, build):
    """Count each library's original source once, excluding tests and dependencies."""
    root = Path(__file__).resolve().parents[4]
    cache = (build / 'CMakeCache.txt').read_text().splitlines()
    checkout = lambda key: Path(next(line.split('=', 1)[1] for line in cache if line.startswith(key + ':')))
    flecs = checkout('DEFOLD_DATA_FLECS_DIR')
    entt = checkout('DEFOLD_DATA_ENTT_DIR')
    inputs = (
        ('Defold data', root, [root / 'engine/data/src'], 'engine/data/src, excluding test/ and dlib'),
        ('Flecs', flecs, [flecs / 'src', flecs / 'include'], 'src/ + include/, including addons and C++ API; excludes distr/'),
        ('EnTT', entt, [entt / 'src/entt'], 'src/entt/, including all library modules'),
    )
    records = {}
    for name, base, folders, scope in inputs:
        files = sorted(p for folder in folders for p in folder.rglob('*')
                       if p.suffix in ('.c', '.cpp', '.h', '.hpp', '.inl') and p.is_file()
                       and 'test' not in p.relative_to(base).parts)
        with tempfile.TemporaryDirectory(prefix='data-cloc-') as tmp:
            listing = Path(tmp) / 'sources.txt'
            listing.write_text(''.join(str(p) + '\n' for p in files))
            raw = subprocess.check_output(['cloc', '--json', '--quiet', '--skip-uniqueness',
                                           '--force-lang=C++,inl', '--list-file=' + str(listing)], text=True)
        counted = json.loads(raw)
        if counted['SUM']['nFiles'] != len(files):
            raise RuntimeError(f'cloc did not count every {name} source file')
        row = next(row for row in result['rows'] if row['backend'] == name)
        row['cloc'] = counted['SUM']
        row['cloc_scope'] = scope
        records[name] = {'root': str(base), 'report': counted,
                         'source_sha256': {str(p.relative_to(base)): digest(p) for p in files}}
    result['cloc'] = {
        'version': subprocess.check_output(['cloc', '--version'], text=True).strip(),
        'generated_utc': datetime.now(timezone.utc).isoformat(),
        'method': 'Physical C/C++ code lines, excluding comments and blank lines. Original library '
                  'sources and headers only; excludes tests, examples, third-party dependencies and '
                  'duplicate amalgamated distributions. Counts all source regardless of build flags.',
        'libraries': records,
    }


def measure(build, output):
    if platform.system() != 'Darwin':
        raise RuntimeError('Library-size collection currently supports macOS Mach-O builds')
    build = build.resolve()
    commands = json.loads((build / 'compile_commands.json').read_text())
    targets = ('data', 'dlib', 'data_benchmark_flecs_threaded') + tuple(
        f'benchmark_data_size_{backend}' for backend in ('empty', 'defold', 'flecs', 'entt'))
    selected = [c for c in commands if any(f'CMakeFiles/{target}.dir/' in c['command'] for target in targets)]
    if any('-fsanitize=' in c['command'] for c in selected):
        raise RuntimeError('Size measurements require unsanitized libraries and probes')
    subprocess.run(['cmake', '--build', str(build), '--target', *targets[3:], '-j', '8'], check=True,
                   stdout=(output / 'size-build.log').open('w'), stderr=subprocess.STDOUT)
    work = build / 'library-size-stripped'
    work.mkdir(exist_ok=True)
    hashes = {}
    stripped_hashes = {}

    def strip(path):
        hashes[str(path)] = digest(path)
        stripped = work / path.name
        subprocess.run(['strip', '-S', '-x', '-o', str(stripped), str(path)], check=True, capture_output=True)
        stripped_hashes[stripped.name] = digest(stripped)
        return stripped

    linked = {}
    for backend in ('empty', 'defold', 'flecs', 'entt'):
        binary = one(build, f'engine/data/build/*/src/test/benchmark_data_size_{backend}')
        symbols = subprocess.check_output(['nm', '-u', str(binary)], text=True)
        if re.search(r'__(?:a|t|ub|m|l|hwa)san_|__sanitizer_', symbols):
            raise RuntimeError(f'Sanitizer instrumentation in {binary}')
        for count in (1, 100, 1000):
            result = subprocess.check_output([str(binary), str(count)], text=True).strip()
            if result != str(count * 75):
                raise RuntimeError(f'Unexpected {backend} lifecycle checksum: {result}')
        stripped = strip(binary)
        layout = subprocess.check_output(['size', '-m', str(stripped)], text=True)
        (output / f'size-{backend}.log').write_text(layout)
        # Sections omit Mach-O page padding, headers and link metadata. BSS is
        # included as resident data, even though it occupies no bytes on disk.
        sections = sum(int(n) for n in re.findall(r'Section \S+: (\d+)', layout))
        linked[backend] = {'file_bytes': stripped.stat().st_size, 'section_bytes': sections}
    rows = []
    for name, backend, archive in (
        ('Defold data', 'defold', 'engine/data/build/*/libdata.a'),
        ('Flecs', 'flecs', 'engine/data/build/*/src/test/libdata_benchmark_flecs_threaded.a'),
        ('EnTT', 'entt', None),
    ):
        row = {'backend': name, 'archive_bytes': None, 'archive_stripped_bytes': None, **linked[backend]}
        row['extra_file_bytes'] = row['file_bytes'] - linked['empty']['file_bytes']
        row['extra_section_bytes'] = row['section_bytes'] - linked['empty']['section_bytes']
        if archive:
            path = one(build, archive)
            row['archive_bytes'] = path.stat().st_size
            row['archive_stripped_bytes'] = strip(path).stat().st_size
        rows.append(row)
    dlib = one(build, 'engine/dlib/build/*/libdlib.a')
    hashes[str(dlib)] = digest(dlib)
    result = {
        'host': platform.platform(), 'baseline': linked['empty'], 'rows': rows,
        'method': 'Clang Release -O2, no LTO or sanitizers; strip -S -x; linker dead_strip. '
                  'Linked deltas are relative to identical argument parsing/output without an ECS. '
                  'Probes create position/health entities, query/update, get values and destroy. '
                  'Defold archive excludes dlib; its linked probe includes required dlib code. '
                  'Flecs uses the benchmark full default addon configuration, C API, counters/asserts disabled. '
                  'EnTT is header-only, C++20, 64-bit entities, ENTT_USE_ATOMIC and ENTT_DISABLE_ASSERT. '
                  'Linked sizes describe this two-component workload, not a complete engine or all features. '
                  'System dynamic libraries and their code are excluded.',
        'input_sha256': hashes, 'stripped_sha256': stripped_hashes,
        'compile_commands': {Path(c['file']).name: c['command'] for c in selected if 'CMakeFiles/dlib.dir/' not in c['command']},
        'source_sha256': {p.name: digest(p) for p in Path(__file__).parent.glob('benchmark_data_size_*.cpp')},
    }
    add_source_counts(result, build)
    (output / 'library-sizes.json').write_text(json.dumps(result, indent=2) + '\n')
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    print(json.dumps(measure(args.build, args.output)['rows'], indent=2))
