#!/usr/bin/env python3
"""LongMap handoff: run from an initialized editor checkout with JDK 25.

  python3 scripts/long_map.py validate
  python3 scripts/long_map.py benchmark /tmp/long-map/run.edn 0 sparse-lookup
  python3 scripts/long_map.py memory /tmp/long-map/memory.edn
  python3 scripts/long_map.py report /tmp/long-map/run.edn
  python3 scripts/long_map.py profile long-map sparse-lookup cpu /tmp/long-map/profiles

All generated output must be in the computer's /tmp directory. macOS needs no
Linux setup: leave LONG_MAP_CPUS unset; affinity/thermal sensors are optional.
Defaults: 30 samples, 10s warmup, 100ms sample target. LONG_MAP_CASE_PAUSE_MS=30000 LONG_MAP_JVM_PAUSE_MS=60000
add cooling pauses. Run indices rotate implementation order; use fresh output
files after code/settings changes. Reports compare exactly three maps.

Memory uses the project's existing Jamm dependency. Resolve it first with
lein with-profile +jamm deps, or provide LONG_MAP_JAMM_JAR. CPU/allocation
profiles use the existing clj-async-profiler dependency, separately from timing.

The editor has NOT been switched to LongMap. Current code uses 64-key pages.
Linux screening favored clustered lookup/build/traversal; sparse lookup ranking
was unstable across runs. Remeasure on macOS before making performance claims.
"""
import hashlib
import json
import os
import re
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = Path('/tmp/defold-long-map')
DEPENDENCIES = [
    'org/clojure/clojure/1.12.0/clojure-1.12.0.jar',
    'org/clojure/spec.alpha/0.5.238/spec.alpha-0.5.238.jar',
    'org/clojure/core.specs.alpha/0.4.74/core.specs.alpha-0.4.74.jar',
    'org/clojure/test.check/1.1.1/test.check-1.1.1.jar',
    'org/clojure/data.int-map/1.3.0/data.int-map-1.3.0.jar',
    'criterium/criterium/0.4.3/criterium-0.4.3.jar',
    'com/clojure-goes-fast/clj-async-profiler/2.0.0-beta1/clj-async-profiler-2.0.0-beta1.jar',
]


def source_hashes():
    paths = [ROOT / 'src/java/com/defold/util/LongMap.java', ROOT / 'test/util/long_map_test.clj', ROOT / 'test/benchmark/long_map_bench.clj']
    return {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest() for path in paths}


def write_manifest(output):
    require_temporary(output)
    path = Path(output).with_suffix('.manifest.json')
    manifest = {'sources': source_hashes(),
                'cpu_affinity': os.environ.get('LONG_MAP_CPUS'),
                'samples': int(os.environ.get('LONG_MAP_SAMPLES', '30')),
                'warmup_ms': int(os.environ.get('LONG_MAP_WARMUP_MS', '10000')),
                'sample_ms': int(os.environ.get('LONG_MAP_SAMPLE_MS', '100'))}
    for name, variable in [('case_pause_ms', 'LONG_MAP_CASE_PAUSE_MS'),
                           ('jvm_pause_ms', 'LONG_MAP_JVM_PAUSE_MS')]:
        if int(os.environ.get(variable, '0')):
            manifest[name] = int(os.environ[variable])
    if path.exists() and json.loads(path.read_text()) != manifest:
        sys.exit('Output belongs to different code or settings; choose a fresh measurement file.')
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(manifest, indent=2) + '\n')


def thermal_state():
    temperatures = {}
    for name in Path('/sys/class/hwmon').glob('hwmon*/name'):
        if name.read_text().strip() == 'coretemp':
            for sensor in name.parent.glob('temp*_input'):
                label = sensor.with_name(sensor.name.replace('_input', '_label'))
                temperatures[label.read_text().strip() if label.exists() else sensor.name] = int(sensor.read_text()) / 1000
    counters = {}
    for name in Path('/sys/devices/system/cpu').glob('cpu[0-9]*/thermal_throttle/*throttle_count'):
        try:
            counters[str(name)] = int(name.read_text())
        except OSError:
            pass
    return {'temperature_c': temperatures, 'throttle_counts': counters}



def require_temporary(path):
    if not Path(path).resolve().is_relative_to(Path('/tmp').resolve()):
        sys.exit('Generated output must be under the computer /tmp directory.')


def report(timing, memory):
    from collections import defaultdict
    from html import escape
    import math
    from statistics import median
    require_temporary(timing)
    groups = defaultdict(lambda: defaultdict(list))
    for line in timing.read_text().splitlines():
        fields = dict(re.findall(r':(scenario|implementation|ns-per-operation|bytes-per-operation) ([^\s,}]+)', line))
        groups[fields['scenario'][1:]][fields['implementation'][1:]].append(fields)
    keys = ['clojure', 'int-map', 'long-map']
    labels = ['Clojure', 'int-map', 'LongMap']
    weights = {'dense-lookup': 5, 'fragmented-lookup': 5, 'sparse-lookup': 0,
               'persistent-edit': 3, 'persistent-replace': 5, 'small-editable-batch': 3,
               'persistent-delete': 4, 'build': 4, 'contiguous-append': 4,
               'reduction': 3, 'dense-values': 2}
    def chart(title, values, unit):
        maximum = max(values) or 1
        bars = ''.join(f'<div class="bar"><span>{label}</span><i style="width:{value / maximum * 65:.2f}%"></i><b>{value:.3f}</b></div>' for label, value in zip(labels, values))
        return f'<article><h2>{escape(title)}</h2><p>{escape(unit)}</p>{bars}</article>'
    means = {}
    allocations = {}
    for scenario, implementations in groups.items():
        if set(implementations) != set(keys):
            raise ValueError('Each scenario needs all three implementations')
        means[scenario] = [median(float(row['ns-per-operation']) for row in implementations[key]) for key in keys]
        allocations[scenario] = [median(float(row['bytes-per-operation']) for row in implementations[key]) for key in keys]
    html = '<!doctype html><meta charset="utf-8"><title>LongMap performance</title><style>body{font:15px system-ui;max-width:1000px;margin:30px auto;background:#f5f6f9;padding:20px}article{background:white;padding:20px;margin:16px 0;border-radius:10px}.bar{display:flex;align-items:center;gap:12px;margin:15px 0}.bar span{width:90px}.bar i{display:block;height:20px;background:#159b94}.bar b{margin-left:auto}p{line-height:1.5}</style><h1>LongMap performance</h1>'
    weight_sum = sum(weights.get(name, 0) for name in means)
    if weight_sum:
        scores = [100 * math.exp(sum(weights.get(name, 0) * math.log(values[1] / values[index]) for name, values in means.items()) / weight_sum) for index in range(3)]
        html += chart('Total score', scores, 'Importance-weighted timing index; int-map = 100; higher is better')
    html += '<p>CPU, retained memory and temporary allocation are separate. Importance is source-informed relevance, not measured editor CPU shares. One JVM pass does not establish a performance lead. Inspect raw samples and thermal records; these are map microbenchmarks.</p>'
    for name, values in means.items():
        html += chart(name + f' · importance {weights.get(name, 0)}/5', values, 'ns/op; lower is better')
    if memory:
        require_temporary(memory)
        retained = defaultdict(dict)
        for line in memory.read_text().splitlines():
            fields = dict(re.findall(r':(distribution|implementation|history|bytes) ([^\s,}]+)', line))
            retained[(fields['distribution'], fields['history'])][fields['implementation'][1:]] = int(fields['bytes'])
        for (distribution, history), values in retained.items():
            html += chart(distribution[1:] + (' + 64 snapshots' if history == 'true' else ''), [values[key] / 1048576 for key in keys], 'Retained MiB; payloads excluded; lower is better')
    for name, values in allocations.items():
        html += chart(name + ' allocation', values, 'B/op; lower is better')
    output = timing.with_suffix('.html')
    output.write_text(html)
    print(output)


def main():
    if len(sys.argv) > 1 and sys.argv[1] in {'--help', '-h'}:
        print(__doc__)
        return
    if len(sys.argv) > 1 and sys.argv[1] == 'report':
        report(Path(sys.argv[2]), Path(sys.argv[3]) if len(sys.argv) > 3 else None)
        return
    os.chdir(ROOT)
    classes = OUTPUT / 'classes'
    classes.mkdir(parents=True, exist_ok=True)
    jars = [ROOT / '.m2/repository' / name for name in DEPENDENCIES]
    for jar in jars:
        if not jar.is_file():
            sys.exit(f'Missing {jar}; follow README_BUILD.md and run lein init first.')
    subprocess.run([
        'javac', '-Xlint:all', '-cp', str(jars[0]), '-d', str(classes),
        str(ROOT / 'src/java/com/defold/util/LongMap.java'),
    ], check=True)
    jamm = Path(os.environ.get('LONG_MAP_JAMM_JAR', str(ROOT / '.m2/repository/com/github/jbellis/jamm/0.4.0/jamm-0.4.0.jar')))
    if not jamm.is_file():
        jamm = Path.home() / '.m2/repository/com/github/jbellis/jamm/0.4.0/jamm-0.4.0.jar'
    if jamm.is_file():
        jars.append(jamm)
    classpath = os.pathsep.join(map(str, [classes, ROOT / 'test', ROOT / 'src/clj', *jars]))
    (OUTPUT / 'classpath.txt').write_text(classpath)
    if os.environ.get('LONG_MAP_CPUS') and sys.platform != 'linux':
        sys.exit('LONG_MAP_CPUS affinity is Linux-only; leave it unset on macOS.')
    prefix = ['taskset', '-c', os.environ['LONG_MAP_CPUS']] if os.environ.get('LONG_MAP_CPUS') else []
    properties = [f'-Dlong-map.samples={os.environ.get("LONG_MAP_SAMPLES", "30")}',
                  f'-Dlong-map.warmup-ns={int(os.environ.get("LONG_MAP_WARMUP_MS", "10000")) * 1000000}',
                  f'-Dlong-map.sample-ns={int(os.environ.get("LONG_MAP_SAMPLE_MS", "100")) * 1000000}']
    if int(os.environ.get('LONG_MAP_SAMPLES', '30')) < 4:
        sys.exit('Criterium requires at least four samples for these statistics.')
    mode = sys.argv[1] if len(sys.argv) > 1 else 'validate'
    if mode == 'compile':
        return
    if mode == 'validate':
        requested = sys.argv[2:] or ['long-map']
        if requested not in (['long-map'], ['all']):
            sys.exit('One implementation: long-map')
        expression = """(require 'util.long-map-test)
(let [result (clojure.test/run-tests 'util.long-map-test)]
  (assert (zero? (+ (:fail result) (:error result)))))
(prn {:generated-cases 3060})
(shutdown-agents)"""
        args = ['-e', expression]
    elif mode == 'profile':
        if len(sys.argv) != 6:
            sys.exit('Usage: python3 scripts/long_map.py profile IMPLEMENTATION SCENARIO cpu|alloc OUTPUT_DIRECTORY')
        write_manifest(Path(sys.argv[5]) / 'capture.edn')
        args = ['-m', 'benchmark.long-map-bench', 'profile', *sys.argv[2:]]
    elif mode == 'memory':
        if not jamm.is_file():
            sys.exit('Resolve the existing Jamm dependency: lein with-profile +jamm deps; or set LONG_MAP_JAMM_JAR.')
        agent = jamm
        memory_output = sys.argv[2] if len(sys.argv) > 2 else str(OUTPUT / 'memory.edn')
        if Path(memory_output).exists():
            sys.exit('Choose a fresh memory output file.')
        write_manifest(memory_output)
        args = ['-m', 'benchmark.long-map-bench', 'memory', memory_output]
    elif mode in {'benchmark', 'probe', 'small-brush'}:
        output = sys.argv[2] if len(sys.argv) > 2 else str(OUTPUT / 'screening.edn')
        run = int(sys.argv[3]) if len(sys.argv) > 3 else 0
        implementations = ['long-map'] if mode == 'probe' else ['clojure', 'int-map', 'long-map']
        write_manifest(output)
        offset = run % len(implementations)
        order = implementations[offset:] + implementations[:offset]
        case_pause_ms = int(os.environ.get('LONG_MAP_CASE_PAUSE_MS', '0'))
        jvm_pause_ms = int(os.environ.get('LONG_MAP_JVM_PAUSE_MS', '0'))
        if min(case_pause_ms, jvm_pause_ms) < 0:
            sys.exit('Cooldown durations must be nonnegative')
        for index, implementation in enumerate(order):
            if jvm_pause_ms:
                print(f'Cooling down {jvm_pause_ms / 1000:g}s before {implementation}', flush=True)
                time.sleep(jvm_pause_ms / 1000)
            if mode == 'small-brush':
                expression = f"""(require 'benchmark.long-map-bench)
(in-ns 'benchmark.long-map-bench)
(doseq [[index scenario] (map-indexed vector [[:tile-single-paint :tile :local 1] [:tile-small-paint :tile :local 8]])]
  (when (pos? index) (Thread/sleep {case_pause_ms}))
  (measure! scenario :{implementation} {json.dumps(output)} {run}))
(shutdown-agents)"""
                args = ['-e', expression]
            elif case_pause_ms:
                selected = '#{' + ' '.join(':' + name for name in sys.argv[4:]) + '}'
                expression = f"""(require 'benchmark.long-map-bench)
(in-ns 'benchmark.long-map-bench)
(let [selected {selected}
      cases (if (empty? selected) scenarios (filter #(contains? selected (first %)) scenarios))]
  (assert (or (empty? selected) (= (count selected) (count cases))))
  (doseq [[index scenario] (map-indexed vector cases)]
    (when (pos? index) (Thread/sleep {case_pause_ms}))
    (measure! scenario :{implementation} {json.dumps(output)} {run})))
(shutdown-agents)"""
                args = ['-e', expression]
            else:
                args = ['-m', 'benchmark.long-map-bench', implementation, output, str(run), *sys.argv[4:]]
            before = thermal_state()
            subprocess.run([*prefix, 'java', '--enable-preview', '-XX:+UseG1GC', '-XX:+UseCompactObjectHeaders', '-Xmx768m',
                            *properties, '-cp', classpath, 'clojure.main', *args], check=True)
            after = thermal_state()
            thermal_path = Path(output).with_suffix('.thermal.json')
            thermal = json.loads(thermal_path.read_text()) if thermal_path.exists() else []
            thermal.append({'implementation': implementation, 'run': run, 'before': before, 'after': after,
                            'throttle_delta': {name: value - before['throttle_counts'][name]
                                               for name, value in after['throttle_counts'].items()
                                               if name in before['throttle_counts']}})
            thermal_path.write_text(json.dumps(thermal, indent=2) + '\n')
        return
    else:
        sys.exit('Usage: python3 scripts/long_map.py [compile|validate [long-map]|benchmark|small-brush|probe|memory|profile|report]')
    command = [*prefix, 'java', '--enable-preview', '-XX:+UseG1GC', '-XX:+UseCompactObjectHeaders', '-Xmx768m',
                    *(['-Djdk.attach.allowAttachSelf', '-XX:+UnlockDiagnosticVMOptions', '-XX:+DebugNonSafepoints',
                       '--enable-native-access=ALL-UNNAMED'] if mode == 'profile' else []),
                    *(['-javaagent:' + str(agent), '--add-opens=java.base/java.lang=ALL-UNNAMED',
                       '--add-opens=java.base/java.util.concurrent.atomic=ALL-UNNAMED'] if mode == 'memory' else []),
                    *properties, '-cp', classpath, 'clojure.main', *args]
    if mode != 'validate':
        subprocess.run(command, check=True)
        return
    # Validation loads the implementation and test suite, not the benchmark.
    tested_sources = {name: digest for name, digest in source_hashes().items()
                      if name != 'test/benchmark/long_map_bench.clj'}
    generated_cases = None
    with subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True) as process:
        for line in process.stdout:
            print(line, end='', flush=True)
            match = re.search(r':generated-cases (\d+)', line)
            if match:
                generated_cases = int(match[1])
        if process.wait() != 0:
            sys.exit(process.returncode)
    current_sources = {name: digest for name, digest in source_hashes().items()
                       if name in tested_sources}
    if generated_cases is None or tested_sources != current_sources:
        changed = [name for name in tested_sources if tested_sources[name] != current_sources.get(name)]
        sys.exit(f'Validation provenance failed: generated_cases={generated_cases}, changed={changed}')
    (OUTPUT / 'last-validation.json').write_text(json.dumps({
        'status': 'passed', 'generated_cases': generated_cases, 'sources': tested_sources,
    }, indent=2) + '\n')


if __name__ == '__main__':
    main()
