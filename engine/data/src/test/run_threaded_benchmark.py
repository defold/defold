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

"""Build the current offline comparison from the latest completed benchmark run."""

import argparse
import csv
from datetime import datetime, timezone
import hashlib
import io
import json
import math
import os
import platform
from pathlib import Path
import statistics
import subprocess
import sys

from measure_library_sizes import measure as measure_library_sizes
from build_benchmark_report import threaded_activity_html

ROOT = Path(__file__).resolve().parents[4]
BACKENDS = (('Defold', ''), ('Flecs', 'flecs-'), ('Bevy', 'bevy-'), ('EnTT', 'entt-'))
WAVE_FIELDS = ('content_spawned', 'content_despawned', 'enemies_spawned', 'enemies_despawned', 'live_enemies')


def run(command, output, sanitizer=False, extra_env=None):
    env = os.environ.copy()
    env.update(extra_env or {})
    if sanitizer:
        env['TSAN_OPTIONS'] = 'halt_on_error=1:exitcode=66'
    error_file = output.with_suffix('.stderr.log')
    with output.open('w') as stdout, error_file.open('w') as stderr:
        subprocess.run([str(v) for v in command], check=True, env=env, stdout=stdout, stderr=stderr, timeout=1200)
    if sanitizer and error_file.read_text().strip():
        raise RuntimeError(f'TSAN diagnostics in {error_file}')


def executable(build, name):
    paths = list(build.glob(f'engine/data/build/*/src/test/{name}'))
    if len(paths) != 1:
        raise RuntimeError(f'Expected one {name} in source build {build}, found {len(paths)}')
    return paths[0]


def verify_instrumentation(build):
    commands = json.loads((build / 'compile_commands.json').read_text())
    required = {
        'data.cpp': 'data', 'data_query.cpp': 'data', 'data_iter.cpp': 'data',
        'data_field.cpp': 'data', 'data_io.cpp': 'data', 'jobsystem.cpp': 'dlib',
        'test_data_threaded.cpp': 'test_data_threaded',
        'benchmark_data_threaded.cpp': 'benchmark_data_threaded',
        'benchmark_data_threaded_memory.cpp': 'benchmark_data_threaded_memory',
        'benchmark_data_threaded_flecs.cpp': 'benchmark_data_threaded_flecs',
        'benchmark_data_threaded_entt.cpp': 'benchmark_data_threaded_entt',
        'benchmark_data_threaded_entt_explosion.cpp': 'benchmark_data_threaded_entt',
        'flecs.c': 'data_benchmark_flecs_threaded',
    }
    for source, target in required.items():
        matches = [c for c in commands if Path(c['file']).name == source and f'CMakeFiles/{target}.dir/' in c['command']]
        if len(matches) != 1 or '-fsanitize=thread' not in matches[0]['command']:
            raise RuntimeError(f'Missing TSAN instrumentation for {source}')


def read_rows(path, sanitized=False, memory=False):
    text = path.read_text()
    expected = '# sanitizer=thread;' if sanitized else '# sanitizer=none;'
    if expected not in text or memory != ('# memory=' in text):
        raise RuntimeError(f'Unexpected instrumentation: {path}')
    return list(csv.DictReader(io.StringIO('\n'.join(line for line in text.splitlines() if not line.startswith('#')))))


def validate_rows(rows, frames, population):
    for workers in (1, 2, 4, 8):
        subset = [r for r in rows if int(r['workers']) == workers]
        if [int(r['frame']) for r in subset] != list(range(frames)):
            raise RuntimeError(f'Incomplete {workers}-worker run')
        live_rows, live_enemies = population, population * 24 // 100
        enemy_waves = 0
        for row in subset:
            if sorted(row['admission_order']) != list('0123') or (row['backend'] != 'Bevy' and int(row['busy_attempts']) == 0):
                raise RuntimeError('Workload did not exercise all updates and conflicts')
            content_add, content_remove, enemy_add, enemy_remove, actual_enemies = (int(row[key]) for key in WAVE_FIELDS)
            if content_add != content_remove or any(n < 0 or n % 100 for n in (content_add, content_remove, enemy_add, enemy_remove)):
                raise RuntimeError('Invalid 100-instance content/enemy wave')
            if enemy_remove > enemy_waves:
                raise RuntimeError('Despawned more wave enemies than were alive')
            enemy_waves += enemy_add - enemy_remove
            live_rows += enemy_add - enemy_remove
            live_enemies += enemy_add - enemy_remove
            if int(row['live_rows']) != live_rows or actual_enemies != live_enemies:
                raise RuntimeError('Wave counts do not match live population')
            if int(row['frame']) % 8 in (4, 7) and enemy_waves:
                raise RuntimeError('Enemy wave was not completely despawned')
        if frames >= 8 and not (any(int(r['enemies_spawned']) for r in subset) and any(int(r['enemies_despawned']) for r in subset)
                               and any(not int(r['enemies_spawned']) and not int(r['enemies_despawned']) for r in subset)):
            raise RuntimeError('Missing enemy spawn, despawn or quiet frames')
    if {int(r['workers']) for r in rows} != {1, 2, 4, 8}:
        raise RuntimeError('Unexpected worker counts')


def report(output, timing, memory, population, warmup, sizes):
    summaries = []
    domains = ('store', 'job', 'caller', 'resource')
    for workers in (1, 2, 4, 8):
        for backend, _ in BACKENDS:
            timed = [r for r in timing if r['backend'] == backend and int(r['workers']) == workers and int(r['frame']) >= warmup]
            measured = [r for r in memory if r['backend'] == backend and int(r['workers']) == workers and int(r['frame']) >= warmup]
            times = sorted(int(r['frame_us']) / 1000 for r in timed)
            requests = lambda r: sum(int(r[f'{domain}_requests']) for domain in domains)
            delta = lambda r: sum(int(r[f'{domain}_delta_bytes']) for domain in domains)
            median = lambda key: statistics.median(int(r[key]) for r in measured)
            summaries.append(dict(backend=backend, workers=workers, frame=statistics.median(times),
                p95=times[math.ceil(len(times) * .95) - 1],
                update=statistics.median((int(r['frame_us'])-int(r['mutation_us'])) / 1000 for r in timed),
                mutation=statistics.median(int(r['mutation_us']) / 1000 for r in timed),
                requests=statistics.median(requests(r) for r in measured),
                update_requests=statistics.median(requests(r)-int(r['stream_store_requests']) for r in measured),
                stream_requests=median('stream_store_requests'),
                delta=statistics.median(delta(r) for r in measured) / 1024,
                peak=median('peak_additional_bytes') / 1024))
    charts = []
    for key, title, unit in (('frame', 'Complete frame · median', 'ms'), ('p95', 'Complete frame · p95', 'ms'),
                             ('update', 'Updates and preparation · median', 'ms'), ('mutation', 'Unload and instantiate · median', 'ms'),
                             ('requests', 'Allocation requests per frame', ''), ('delta', 'Retained heap change per frame', 'KiB'),
                             ('peak', 'Peak additional heap per frame', 'KiB')):
        maximum = max(s[key] for s in summaries) or 1
        bars = []
        for workers in (1, 2, 4, 8):
            bars.append(f'<h3>{workers} worker{"s" if workers != 1 else ""}</h3>')
            for item in [s for s in summaries if s['workers'] == workers]:
                bars.append(f'<div class="row"><span>{item["backend"]}</span><div class="track"><i class="{item["backend"]}" style="width:{max(0,100 * item[key] / maximum):.3f}%"></i></div><b>{item[key]:,.2f} {unit}</b></div>')
        charts.append(f'<section><h2>{title}</h2>{"".join(bars)}</section>')
    table = ''.join(f'<tr><td>{s["backend"]}</td><td>{s["workers"]}</td><td>{s["frame"]:.3f}</td><td>{s["p95"]:.3f}</td><td>{s["update"]:.3f}</td><td>{s["mutation"]:.3f}</td><td>{s["requests"]:g}</td><td>{s["update_requests"]:g}</td><td>{s["stream_requests"]:g}</td><td>{s["delta"]:.2f}</td></tr>' for s in summaries)
    embedded = json.dumps({'summary': summaries, 'timing': timing, 'memory': memory}).replace('<', '\\u003c')
    size_rows = []
    for row in sizes['rows']:
        archive = f"{row['archive_stripped_bytes'] / 1024:,.1f} KiB" if row['archive_stripped_bytes'] is not None else 'Header-only'
        size_rows.append(f"<tr><td>{row['backend']}</td><td>{archive}</td><td>{row['extra_file_bytes'] / 1024:,.1f} KiB</td><td>{row['extra_section_bytes'] / 1024:,.1f} KiB</td></tr>")
    content = f'''<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Threaded ECS comparison · Defold, Flecs, Bevy and EnTT</title>
<style>body{{font:16px system-ui;margin:32px auto;padding:0 18px;max-width:1150px;background:#f6f8fb;color:#15263c}}h1{{font-size:29px}}h2{{font-size:18px}}h3{{font-size:12px;margin:14px 0 3px;color:#54677e}}p,li{{line-height:1.5}}.charts{{display:grid;grid-template-columns:repeat(auto-fit,minmax(min(100%,330px),1fr));gap:14px}}section{{background:white;border:1px solid #dce3ed;padding:16px;border-radius:8px}}.row{{display:flex;gap:6px;align-items:center;margin:2px 0;font-size:12px}}.row span{{width:43px}}.track{{flex:1;background:#edf1f7;height:12px}}i{{display:block;height:100%}}.Defold{{background:#367ad0}}.Flecs{{background:#df8b29}}.Bevy{{background:#248b71}}.EnTT{{background:#9854bd}}b{{width:88px;text-align:right;font-variant-numeric:tabular-nums}}.table{{overflow-x:auto}}table{{width:100%;border-collapse:collapse;font-size:13px}}th,td{{padding:8px;border-bottom:1px solid #dce3ed;text-align:right;white-space:nowrap}}th:first-child,td:first-child{{text-align:left}}code{{font-size:.9em}}</style>
<h1>Threaded ECS comparison</h1>
<p><strong>Defold · Flecs · Bevy · EnTT</strong> — {population:,} initial instances across SpotLight, PointLight, Player, Enemy, Pickup and Breakable. Four ready updates run every frame while the application prepares incoming content. {1 + max(int(r["frame"]) for r in timing)} frames per worker count; labels give the number of row workers.</p>
<p>TSAN passed for all four backends at 1, 2, 4 and 8 workers, including separate allocation builds. These charts use release builds without sanitizers. The first {warmup} frames are warmup; subsequent frames belong to one continuous stream.</p>
<h2>Performance</h2><div class="charts">{''.join(charts[:4])}</div>
<h2>Memory</h2><div class="charts">{''.join(charts[4:])}</div>
<p>Allocation requests and heap changes cover the whole frame, including scheduling, resource preparation and content changes. Heap values are changes after the operation, not the store's total size. Allocation instrumentation runs separately from timing.</p>
<h2>Current results</h2><p>Columns show independent medians, except p95. Phase medians need not sum to the frame median.</p><div class="table"><table><thead><tr><th>Backend</th><th>Workers</th><th>Frame ms</th><th>p95 ms</th><th>Updates ms</th><th>Streaming ms</th><th>Allocations</th><th>Update/prep</th><th>Streaming</th><th>Heap Δ KiB</th></tr></thead><tbody>{table}</tbody></table></div>
<h2>Library size</h2><div class="table"><table><thead><tr><th>Library</th><th>Stripped static archive</th><th>Linked file Δ</th><th>Code/data sections Δ</th></tr></thead><tbody>{''.join(size_rows)}</tbody></table></div>
<p>Optimized, unsanitized arm64 macOS builds, no LTO. Archives have debug/local symbols stripped. EnTT is header-only: its generated code is measured in the linked example, not as a zero-byte library. Each example creates entities with position and health, queries and updates them, reads values and destroys them. Linked deltas subtract the same empty program ({sizes['baseline']['file_bytes'] / 1024:g} KiB on disk); dead stripping is enabled. Section deltas omit page padding and link metadata, and include zero-filled data.</p>
<p>The Defold archive contains only <code>data</code>; its linked example includes the required dlib code. Flecs uses its default addons with the C API and optional counters/asserts disabled. EnTT uses C++20, 64-bit entities, atomic type IDs and disabled assertions. These are workload-specific linked footprints, not equivalent feature sets or a complete engine. System shared libraries are excluded. <a href="library-sizes.json">Exact bytes, source hashes and build commands</a>.</p>
<h2>Workload</h2><ul><li><strong>Movement:</strong> update Player/Enemy positions from velocity.</li><li><strong>Explosion:</strong> radius 50 at the origin; subtract 25 health, clamped to zero.</li><li><strong>Regenerate:</strong> add 0.25 health, capped at 100. It competes with Explosion; no prescribed ordering.</li><li><strong>Nearby lights:</strong> sum distance-weighted color × intensity within radius 50. Position is separate from the nested Light value.</li></ul>
<p>The main thread prepares incoming content while updates run, then commits all structural changes after jobs finish. Seeded mixed-content replacements vary each frame, preserving the initial population. Enemy-only waves add bursts of 10–20% of the initial population, interleave smaller additions/removals and quiet frames, and fully despawn between waves. All changes use 100-instance bundles. The eight-frame pattern repeats with different seeded sizes. A sequential replay follows actual admitted system order and checks every live row, task count and checksum outside timing. Removed handles are checked after reuse.</p>
<section>{threaded_activity_html(timing, population)}</section>
<h2>Scheduling and storage differences</h2><ul><li><strong>Defold:</strong> caller-owned HJobSystem jobs, query access reservations and 4096-row ranges. Content is instantiated from shared immutable resources.</li><li><strong>Flecs:</strong> the same HJobSystem coordinator, stages, readonly structural phase and <code>ecs_worker_iter</code> partitions averaging about 4096 rows. Caller conflict checks use matched tables and declared query terms. Each worker owns a cached query; optional statistics counters are disabled in both builds. Component values use separate columns.</li><li><strong>Bevy:</strong> unchained systems in its multithreaded scheduler, with conflicts declared through <code>Query&lt;&amp;mut T&gt;</code>. Parallel queries request 4096-row batches. A persistent schedule coordinator receives exclusive ownership of World while the application prepares content. These Send systems execute row work on the labelled worker pool; the coordinator handles scheduling and ownership transfer.</li><li><strong>EnTT:</strong> caller-owned HJobSystem jobs, public views over sparse component pools, and 4096-candidate ranges of the public driver-pool iterator. The caller serializes updates sharing a writable component pool and performs structural changes after updates. This conservatively serializes Movement and nearby-light reads despite their disjoint entities. Views bind pre-created pools on the main thread. Handles are 64-bit; optional assertions are disabled in both builds.</li></ul>
<p>Defold, Flecs and EnTT request a 1 µs sleep when completion polling makes no progress; OS rounding is included. Bevy uses its scheduler's waiting policy. Flecs/Bevy/EnTT prepare decoded 100-instance mixed/enemy templates, while Defold prepares borrowed packed-resource handles. These are complete integration workloads, not identical dispatch or file-format implementations.</p>
<p>Memory counts include C++ new/delete and dlib containers; Flecs also uses its OS allocation hooks; EnTT pools use C++ new/delete; Bevy uses Rust's global allocator on application, coordinator and worker threads. Bevy's raw store columns combine ECS and scheduler allocations. All exclude libc/pthread malloc, OS thread stacks, allocator headers and sanitizer runtime memory. Persistent worker pools are initialized before measurement.</p>
<p><a href="manifest.json">Build and TSAN manifest</a> · <a href="timing.csv">Defold CSV</a> · <a href="flecs-timing.csv">Flecs CSV</a> · <a href="bevy-timing.csv">Bevy CSV</a> · <a href="entt-timing.csv">EnTT CSV</a></p>
<script type="application/json" id="results">{embedded}</script></html>'''
    (output / 'report.html').write_text(content)
    (output / 'summary.json').write_text(json.dumps(summaries, indent=2) + '\n')


def bevy_build(args, sanitized, memory, test=False):
    build = args.release_build.parent / ('bevy-threaded-' + ('memory-' if memory else '') + ('tsan' if sanitized else 'release'))
    label = ('tsan-' if sanitized else '') + 'bevy-' + ('memory' if memory else 'timing') + ('-test-build' if test else '-build')
    env = {'CARGO_HOME': str(args.cargo_home.resolve())}
    if args.rust_bootstrap:
        env['RUSTC_BOOTSTRAP'] = '1'
    flags = []
    if sanitized:
        flags = ['--cfg', 'data_tsan', '-Zsanitizer=thread', '-Cforce-frame-pointers=yes', '-Cdebuginfo=1']
        if args.rust_tsan_runtime:
            runtime = args.rust_tsan_runtime.resolve()
            flags += ['-Zexternal-clangrt', f'-Clink-arg={runtime}', f'-Clink-arg=-Wl,-rpath,{runtime.parent}']
    env['RUSTFLAGS'] = ''
    env['CARGO_ENCODED_RUSTFLAGS'] = '\x1f'.join(flags)
    command = ['cargo', 'test' if test else 'build', '--offline', '--release', '--locked', '--features', 'threaded,memory' if memory else 'threaded',
               '--bin', 'benchmark-data-bevy-threaded', '--manifest-path', ROOT / 'engine/data/src/test/bevy/Cargo.toml', '--target-dir', build]
    if sanitized:
        command += ['--target', args.rust_target, '-Zbuild-std']
    if test:
        command += ['--no-run', '--message-format=json']
    print(label, flush=True)
    run(command, args.output / f'{label}.log', extra_env=env)
    if test:
        artifacts = [json.loads(line) for line in (args.output / f'{label}.log').read_text().splitlines() if line.startswith('{')]
        paths = [a['executable'] for a in artifacts if a.get('executable') and a.get('profile', {}).get('test')]
        if len(paths) != 1:
            raise RuntimeError('Expected one Bevy test binary')
        binary = Path(paths[0])
    else:
        binary = build / (args.rust_target if sanitized else '') / 'release/benchmark-data-bevy-threaded'
    if sanitized:
        symbols = subprocess.check_output(['nm', '-u', str(binary)], text=True)
        if '___tsan_read' not in symbols or '___tsan_write' not in symbols:
            raise RuntimeError('Bevy executable lacks TSAN read/write instrumentation')
    return binary, {'command': [str(c) for c in command], 'env': env}


def main():
    parser = argparse.ArgumentParser(description='Run required TSAN validation before collecting threaded benchmark results.')
    parser.add_argument('--tsan-build', type=Path, required=True)
    parser.add_argument('--release-build', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--rows', type=int, default=1000000)
    parser.add_argument('--frames', type=int, default=32)
    parser.add_argument('--warmup', type=int, default=2)
    parser.add_argument('--cargo-home', type=Path, default=Path(os.environ.get('CARGO_HOME', Path.home() / '.cargo')))
    parser.add_argument('--rust-target', default='aarch64-apple-darwin')
    parser.add_argument('--rust-tsan-runtime', type=Path, help='Optional external clang TSAN runtime, e.g. Homebrew Rust on macOS')
    parser.add_argument('--rust-bootstrap', action='store_true', help='Enable unstable sanitizer/build-std flags on the installed stable compiler; benchmark builds only')
    args = parser.parse_args()
    if not 0 <= args.warmup < args.frames:
        parser.error('warmup must be less than frames')
    args.output.mkdir(parents=True, exist_ok=True)
    for build in (args.tsan_build, args.release_build):
        targets = ['benchmark_data_threaded', 'benchmark_data_threaded_memory', 'benchmark_data_threaded_flecs', 'benchmark_data_threaded_flecs_memory', 'benchmark_data_threaded_entt', 'benchmark_data_threaded_entt_memory']
        if build == args.tsan_build:
            targets += ['test_data', 'test_data_threaded']
        run(['cmake', '--build', build, '--target', *targets, '-j', '8'], args.output / ('tsan-build.log' if build == args.tsan_build else 'release-build.log'))
    verify_instrumentation(args.tsan_build)
    release_commands = json.loads((args.release_build / 'compile_commands.json').read_text())
    measured_targets = ('CMakeFiles/data.dir/', 'CMakeFiles/dlib.dir/', 'CMakeFiles/benchmark_data_threaded.dir/', 'CMakeFiles/benchmark_data_threaded_flecs.dir/', 'CMakeFiles/data_benchmark_flecs_threaded.dir/', 'CMakeFiles/benchmark_data_threaded_entt.dir/')
    for command in release_commands:
        if any(target in command['command'] for target in measured_targets) and '-fsanitize=' in command['command']:
            raise RuntimeError('Release timing dependencies must be built without sanitizers')
    binaries = {}
    for name in ('test_data', 'test_data_threaded'):
        binary = executable(args.tsan_build, name)
        print(f'TSAN {name}', flush=True)
        run([binary], args.output / f'tsan-{name}.log', True)
        binaries[f'tsan/{name}'] = hashlib.sha256(binary.read_bytes()).hexdigest()
    rust_builds = []
    bevy_tests, settings = bevy_build(args, True, False, test=True)
    rust_builds.append(settings)
    run([bevy_tests, '--test-threads=1'], args.output / 'tsan-bevy-tests.log', True)
    binaries['tsan/bevy-tests'] = hashlib.sha256(bevy_tests.read_bytes()).hexdigest()
    workloads = []
    for sanitized, build in ((True, args.tsan_build), (False, args.release_build)):
        for backend, prefix in BACKENDS:
            for memory in (False, True):
                if backend == 'Bevy':
                    binary, settings = bevy_build(args, sanitized, memory)
                    rust_builds.append(settings)
                else:
                    name = 'benchmark_data_threaded' + ('_flecs' if backend == 'Flecs' else '_entt' if backend == 'EnTT' else '') + ('_memory' if memory else '')
                    binary = executable(build, name)
                label = ('tsan-' if sanitized else '') + prefix + ('memory' if memory else 'timing')
                print(f'{label}: {args.rows} rows, {args.frames} frames, 1/2/4/8 workers', flush=True)
                if backend == 'Bevy':
                    # ComputeTaskPool is process-global; a fresh process sets each size.
                    combined = []
                    for workers in (1, 2, 4, 8):
                        path = args.output / f'{label}-{workers}.csv'
                        run([binary, args.rows, args.frames, workers], path, sanitized)
                        combined += read_rows(path, sanitized, memory)
                    first = (args.output / f'{label}-1.csv').read_text()
                    with (args.output / f'{label}.csv').open('w', newline='') as dest:
                        dest.write('\n'.join(line for line in first.splitlines() if line.startswith('#')) + '\n')
                        writer = csv.DictWriter(dest, fieldnames=combined[0].keys())
                        writer.writeheader(); writer.writerows(combined)
                else:
                    run([binary, args.rows, args.frames], args.output / f'{label}.csv', sanitized)
                rows = read_rows(args.output / f'{label}.csv', sanitized, memory)
                validate_rows(rows, args.frames, args.rows)
                workloads += rows
                binaries[label] = hashlib.sha256(binary.read_bytes()).hexdigest()
    timing = sum((read_rows(args.output / f'{prefix}timing.csv') for _, prefix in BACKENDS), [])
    memory = sum((read_rows(args.output / f'{prefix}memory.csv', memory=True) for _, prefix in BACKENDS), [])
    reference = {r['frame']: r for r in timing if r['backend'] == 'Defold' and r['workers'] == '1'}
    for row in workloads:
        expected = reference[row['frame']]
        if any(row[key] != expected[key] for key in ('live_rows', *WAVE_FIELDS)) or not math.isclose(float(row['light_sum']), float(expected['light_sum']), rel_tol=1e-7):
            raise RuntimeError('Backend/worker content and enemy wave workload mismatch')
    sizes = measure_library_sizes(args.release_build, args.output)
    source_files = list((ROOT / 'engine/data/src').glob('*.cpp')) + list((ROOT / 'engine/data/src').glob('*.h'))
    source_files += list((ROOT / 'engine/data/src/test').glob('*threaded*'))
    source_files += list((ROOT / 'engine/data/src/test').glob('benchmark_data_size_*.cpp'))
    source_files += [ROOT / 'engine/data/src/test/measure_library_sizes.py']
    source_files += list((ROOT / 'engine/data/src/dmsdk/data').glob('*.h'))
    source_files += [ROOT / 'engine/dlib/src/dlib/jobsystem.cpp']
    source_files += list((ROOT / 'engine/data/src/test/bevy/src').rglob('*.rs'))
    source_files += [ROOT / 'engine/data/src/test/bevy/Cargo.toml', ROOT / 'engine/data/src/test/bevy/Cargo.lock', ROOT / 'engine/data/src/test/CMakeLists.txt']
    flecs_command = next(c for c in json.loads((args.tsan_build / 'compile_commands.json').read_text()) if 'CMakeFiles/data_benchmark_flecs_threaded.dir/' in c['command'])
    flecs_source = Path(flecs_command['file'])
    build_settings = {}
    for label, build in (('tsan', args.tsan_build), ('release', args.release_build)):
        commands = json.loads((build / 'compile_commands.json').read_text())
        build_settings[label] = {Path(c['file']).name: c['command'] for c in commands if any(f'CMakeFiles/{t}.dir/' in c['command'] for t in ('data_benchmark_flecs_threaded', 'benchmark_data_threaded_flecs', 'benchmark_data_threaded_entt'))}
    entt_dir = Path(next(line.split('=', 1)[1] for line in (args.release_build / 'CMakeCache.txt').read_text().splitlines() if line.startswith('DEFOLD_DATA_ENTT_DIR:')))
    entt_headers = {str(p.relative_to(entt_dir)): hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted((entt_dir / 'src/entt').rglob('*')) if p.is_file()}
    manifest = {'generated_utc': datetime.now(timezone.utc).isoformat(), 'host': platform.platform(), 'rustc': subprocess.check_output(['rustc', '--version'], text=True).strip(),
                'rust_builds': rust_builds, 'cpp_builds': build_settings,
                'entt': {'source': str(entt_dir), 'revision': subprocess.check_output(['git', '-C', str(entt_dir), 'describe', '--tags', '--always'], text=True).strip(), 'headers': entt_headers},
                'library_sizes_sha256': hashlib.sha256((args.output / 'library-sizes.json').read_bytes()).hexdigest(),
                'flecs': {'source': str(flecs_source), 'revision': subprocess.check_output(['git', '-C', str(flecs_source.parent), 'describe', '--tags', '--always'], text=True).strip(), 'sha256': hashlib.sha256(flecs_source.read_bytes()).hexdigest(), 'header_sha256': hashlib.sha256(flecs_source.with_suffix('.h').read_bytes()).hexdigest()}, 'rows': args.rows, 'frames': args.frames, 'warmup': args.warmup, 'workers': [1, 2, 4, 8],
                'workload': 'mixed-content streaming with seeded enemy waves',
                'wave_seed': '0x91e10da5 xor frame', 'group_rows': 100,
                'tsan': 'passed; halt_on_error=1:exitcode=66; no suppressions', 'binaries': binaries,
                'sources': {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in source_files if p.is_file()},
                'files': {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in args.output.iterdir() if p.suffix in ('.csv', '.log')}}
    (args.output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    report(args.output, timing, memory, args.rows, args.warmup, sizes)
    print(args.output / 'report.html', flush=True)
    if args.output.resolve() == ROOT / 'engine/data/benchmarks/threaded':
        subprocess.run([sys.executable, str(Path(__file__).with_name('build_benchmark_report.py'))], check=True)


if __name__ == '__main__':
    main()
