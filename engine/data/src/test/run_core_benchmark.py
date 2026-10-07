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

"""Measure the seven standalone core cases; the eighth uses the TSAN-validated threaded run."""
import argparse
import csv
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import subprocess
import sys
from build_benchmark_report import CORE_CASES, CORE_BACKENDS, read_csv

ROOT = Path(__file__).resolve().parents[4]
def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=ROOT/'engine/data/build/threaded-release')
    parser.add_argument('--output', type=Path, default=ROOT/'engine/data/benchmarks/core')
    parser.add_argument('--rows', type=int, default=1000000)
    parser.add_argument('--samples', type=int, default=7)
    parser.add_argument('--cargo-home', type=Path, default=Path(os.environ.get('CARGO_HOME', Path.home()/'.cargo')))
    args = parser.parse_args()
    args.build = args.build.resolve()
    args.output.mkdir(parents=True, exist_ok=True)
    commands = []
    def execute(command, label, env=None):
        command = [str(v) for v in command]
        commands.append({'command':command,'env':env or {}})
        with (args.output/f'{label}.log').open('w') as stream:
            subprocess.run(command, env={**os.environ,**(env or {})}, stdout=stream,stderr=subprocess.STDOUT,check=True)
    cache = (args.build/'CMakeCache.txt').read_text()
    for flag in ('WITH_ASAN','WITH_TSAN','WITH_UBSAN'):
        if f'{flag}:BOOL=OFF' not in cache:
            raise ValueError(f'{flag} must be disabled for measurements')
    execute(['cmake','--build',args.build,'--target','benchmark_data_mixed','benchmark_data_memory','benchmark_data_core_entt','benchmark_data_core_entt_memory','-j','6'],'cpp-build')
    cpp = {}
    for key,name in (('timing','benchmark_data_mixed'),('memory','benchmark_data_memory'),('entt-timing','benchmark_data_core_entt'),('entt-memory','benchmark_data_core_entt_memory')):
        found = list(args.build.glob(f'engine/data/build/*/src/test/{name}'))
        assert len(found)==1
        cpp[key] = found[0]
    rust = {}
    env = {'CARGO_HOME':str(args.cargo_home.resolve()),'RUSTFLAGS':'','CARGO_ENCODED_RUSTFLAGS':''}
    for mode in ('timing','memory'):
        target = args.build.parent/('bevy-core-release' if mode=='timing' else 'bevy-core-memory-release')
        command = ['cargo','build','--offline','--locked','--release','--bin','benchmark-data-bevy','--manifest-path',ROOT/'engine/data/src/test/bevy/Cargo.toml','--target-dir',target]
        if mode=='memory': command += ['--features','memory']
        execute(command,f'bevy-{mode}-build',env)
        rust[mode] = target/'release/benchmark-data-bevy'
    binaries = {}
    samples = {}
    cases = {c['id'] for c in CORE_CASES[:-1]}
    for mode in ('timing','memory'):
        for backend,label in CORE_BACKENDS:
            binary = rust[mode] if backend=='bevy_columns' else cpp[f'entt-{mode}' if backend=='entt' else mode]
            symbols = subprocess.check_output(['nm','-u',str(binary)],text=True)
            if any(s in symbols for s in ('___asan_','___tsan_','___ubsan_')):
                raise ValueError(f'Sanitizer in measurement binary: {binary}')
            name = f'{backend}-{mode}'
            command = [str(binary),str(args.rows),str(args.samples),backend,'core']
            commands.append({'command':command})
            print(f'{label} {mode}: {args.rows:,} instances, {args.samples} samples + warmup',flush=True)
            with (args.output/f'{name}.csv').open('w') as output, (args.output/f'{name}.log').open('w') as error:
                subprocess.run(command,stdout=output,stderr=error,check=True)
            rows = read_csv((args.output/f'{name}.csv').read_text())
            actual = {(r['operation'],int(r['sample'])):r for r in rows}
            operations = cases | ({'create_population_soa'} if backend == 'data' else set())
            expected = {(case,s) for case in operations for s in range(1,args.samples+1)}
            if len(rows)!=len(expected) or set(actual)!=expected:
                raise ValueError(f'Expected seven cases per sample, with AoS/SoA creation for Defold: {name}')
            samples[name] = actual
            binaries[name] = digest(binary)
    for name,actual in samples.items():
        reference = samples['data-timing']
        for key,row in actual.items():
            reference_key = ('create_population', key[1]) if key[0] == 'create_population_soa' else key
            for field in ('rows','operations','hits'):
                if row[field]!=reference[reference_key][field]: raise ValueError(f'{name} {key}: {field} differs')
            if not math.isclose(float(row['checksum']),float(reference[reference_key]['checksum']),rel_tol=1e-7,abs_tol=1e-7):
                raise ValueError(f'{name} {key}: checksum differs')
    source = ROOT/'engine/data/src'
    sources = [*source.glob('*.cpp'),*source.glob('*.h'),*(source/'dmsdk/data').glob('*.h'),* (source/'test').glob('benchmark_data_*.cpp'),source/'test/benchmark_data_common.h',source/'test/benchmark_data_boids.h',source/'test/benchmark_memory.cpp',source/'test/benchmark_memory.h',source/'test/CMakeLists.txt',source/'test/data_fixture_writer.cpp',Path(__file__)]
    sources += list((source/'test/bevy/src').rglob('*.rs')) + [source/'test/bevy/Cargo.toml',source/'test/bevy/Cargo.lock',source/'test/benchmark_data_entt.h']
    flecs = Path(next(line.split('=',1)[1] for line in cache.splitlines() if line.startswith('DEFOLD_DATA_FLECS_DIR:')))
    entt = Path(next(line.split('=',1)[1] for line in cache.splitlines() if line.startswith('DEFOLD_DATA_ENTT_DIR:')))
    compile_commands = json.loads((args.build/'compile_commands.json').read_text())
    manifest = {'completed_utc':datetime.now(timezone.utc).isoformat(),'rows':args.rows,'samples':args.samples,'platform':platform.platform(),
        'compiler_cpp':subprocess.check_output([next(c['command'].split()[0] for c in compile_commands if c['file'].endswith('benchmark_data_core.cpp')),'--version'],text=True).splitlines()[0],
        'compiler_rust':subprocess.check_output(['rustc','--version'],text=True).strip(),
        'flecs':subprocess.check_output(['git','-C',str(flecs),'describe','--tags','--always'],text=True).strip(),'bevy':'0.19.1',
        'entt':{'path':str(entt),'revision':subprocess.check_output(['git','-C',str(entt),'describe','--tags','--always','--dirty'],text=True).strip(),
            'headers':{str(p.relative_to(entt)):digest(p) for p in sorted((entt/'src/entt').rglob('*')) if p.suffix in ('.h','.hpp')}},
        'cases':list(c['id'] for c in CORE_CASES[:-1]),
        'case_variants':{'create_population':{'data':{'create_population':'AoS', 'create_population_soa':'SoA'}}},
        'configuration':'One dense mutable Defold table per type; Create population compares public DataCreateRows with prepared native rows (AoS) and DataCreateRowsSoA with prepared native field arrays (SoA), using a fresh store for each and alternating their order. Subsequent cases use the AoS population. Defold uses typed batch ID reads; Flecs/Bevy separate components; EnTT separate component pools and ordinary views without owning groups; inline Light; Boids uses two tagged flocks with reusable scratch and includes snapshot, cell build and steering; one step; no sanitizers; separate allocation builds.',
        'validation':'Every backend, sample and measurement mode agrees on operation count, hit count and checksum (1e-7 relative tolerance for reductions). Per-row boid position/velocity, health, creation and stale-ID checks plus tracked teardown run outside timing.',
        'fixtures':{p.name:digest(p) for p in sorted(cpp['timing'].parent.joinpath('fixtures').glob('*.datac'))},
        'commands':commands,'binaries':binaries,'sources':{str(p.relative_to(ROOT)):digest(p) for p in sources},
        'files':{p.name:digest(p) for p in args.output.iterdir() if p.suffix in ('.csv','.log')}}
    (args.output/'run.json').write_text(json.dumps(manifest,indent=2)+'\n')
    if args.output.resolve()==ROOT/'engine/data/benchmarks/core':
        subprocess.run([sys.executable,str(Path(__file__).with_name('build_benchmark_report.py'))],check=True)
    print('Seven standalone cases validated; threaded update is the eighth report case.',flush=True)

if __name__=='__main__':
    main()
