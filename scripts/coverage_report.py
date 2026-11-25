"""Merge raw profiles from a WITH_COVERAGE engine build and write coverage reports.

Writes, into the output directory: html/, lcov.info, summary.json, report.txt and
summary.md (per engine library totals, suitable for a CI job summary).
"""

import argparse
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

REPOSITORY = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPOSITORY))
import license_data  # noqa: E402

# Engine tests live in src/test, src/<lib>/test or test/ directories.
IGNORED_PATTERNS = (
    r'/test/',
    r'/tmp/dynamo_home/',
    r'/engine/[^/]+/build/',
    r'^/(usr|opt|Library|Applications)/',
)


def read_cmake_cache(build_dir):
    cache = {}
    path = build_dir / 'CMakeCache.txt'
    if not path.is_file():
        raise SystemExit('No CMakeCache.txt in %s' % build_dir)
    for line in path.read_text(encoding='utf-8', errors='replace').splitlines():
        match = re.match(r'^([A-Za-z0-9_.-]+):[A-Z_]+=(.*)$', line)
        if match:
            cache[match.group(1)] = match.group(2)
    return cache


def find_llvm_tools(compiler, llvm_bin):
    # The raw profile format follows the compiler revision, so prefer the tools
    # shipped next to it. Apple clang versions do not match LLVM versions.
    names = ('llvm-profdata', 'llvm-cov')
    candidates = []
    if llvm_bin:
        candidates.append(Path(llvm_bin))
    else:
        compiler = Path(compiler)
        candidates += [compiler.resolve().parent, compiler.parent]
        version = subprocess.run([str(compiler), '--version'], stdout=subprocess.PIPE,
                                 stderr=subprocess.STDOUT, text=True).stdout
        match = re.search(r'^(?!Apple).*clang version (\d+)', version, re.MULTILINE)
        if match:
            major = match.group(1)
            candidates.append(Path('/usr/lib/llvm-%s/bin' % major))
    for directory in candidates:
        tools = [directory / name for name in names]
        if all(tool.is_file() for tool in tools):
            return tools
    if not llvm_bin and match:
        versioned = [shutil.which('%s-%s' % (name, major)) for name in names]
        if all(versioned):
            return [Path(tool) for tool in versioned]
    raise SystemExit('llvm-profdata and llvm-cov matching %s were not found in: %s\n'
                     'Pass --llvm-bin to choose them explicitly.'
                     % (compiler, ', '.join(str(path) for path in candidates)))


def escape_regex(text):
    return re.sub(r'([\\.^$|()\[\]{}*+?])', r'\\\1', text)


def ignore_regex(cache, source_root):
    patterns = list(IGNORED_PATTERNS)
    for path in license_data.excluded_paths:
        if path.startswith('./engine/') or path == './external':
            patterns.append(escape_regex(path[1:] + '/'))
    # Generated sources live in the build directories, and staged copies of public
    # headers in the SDK root. DEFOLD_BUILD_HOME is the repository root by
    # default, which /engine/<lib>/build/ already covers.
    for key in ('CMAKE_CACHEFILE_DIR', 'DEFOLD_BUILD_HOME', 'DEFOLD_SDK_ROOT'):
        if cache.get(key):
            directory = Path(cache[key]).resolve()
            if directory == source_root or directory in source_root.parents:
                continue
            patterns.append('^' + escape_regex(directory.as_posix() + '/'))
    return '|'.join('(%s)' % pattern for pattern in patterns)


def read_objects(build_dir, config):
    if config:
        lists = [build_dir / ('defold_coverage_objects_%s.txt' % config)]
    else:
        lists = sorted(build_dir.glob('defold_coverage_objects_*.txt'))
        if len(lists) > 1:
            raise SystemExit('Several configurations found, pass --config: %s'
                             % ', '.join(path.name for path in lists))
    if not lists or not lists[0].is_file():
        raise SystemExit('No coverage object list in %s. Configure with -DWITH_COVERAGE=ON.' % build_dir)
    objects = []
    seen = set()
    for line in lists[0].read_text(encoding='utf-8').splitlines():
        path = line.strip()
        if path and path not in seen and Path(path).is_file():
            seen.add(path)
            objects.append(path)
    if not objects:
        raise SystemExit('None of the binaries in %s exist. Build the tests first.' % lists[0])
    return objects


def run(command, stdout=None):
    print('+ ' + ' '.join(str(arg) for arg in command[:4]) + (' ...' if len(command) > 4 else ''), flush=True)
    subprocess.run(command, check=True, stdout=stdout)


def library_name(source_root, filename):
    try:
        parts = Path(filename).resolve().relative_to(source_root).parts
    except ValueError:
        return 'other'
    if len(parts) > 1 and parts[0] == 'engine':
        return parts[1]
    return parts[0] if parts else 'other'


def percent(covered, count):
    return '%.1f%%' % (100.0 * covered / count) if count else '-'


def write_markdown_summary(summary_path, source_root, output):
    data = json.loads(summary_path.read_text(encoding='utf-8'))['data'][0]
    keys = ('lines', 'functions', 'branches')
    libraries = {}
    for entry in data['files']:
        totals = libraries.setdefault(library_name(source_root, entry['filename']),
                                      {key: [0, 0] for key in keys})
        for key in keys:
            totals[key][0] += entry['summary'][key]['covered']
            totals[key][1] += entry['summary'][key]['count']
    lines = ['## C/C++ code coverage', '',
             '| Library | Lines | Functions | Branches |', '|---|---:|---:|---:|']
    for name in sorted(libraries):
        totals = libraries[name]
        lines.append('| %s | %s |' % (name, ' | '.join(
            '%s (%d/%d)' % (percent(*totals[key]), *totals[key]) for key in keys)))
    total = data['totals']
    lines.append('| **Total** | %s |' % ' | '.join(
        '**%s** (%d/%d)' % (percent(total[key]['covered'], total[key]['count']),
                            total[key]['covered'], total[key]['count']) for key in keys))
    output.write_text('\n'.join(lines) + '\n', encoding='utf-8')


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--build-dir', type=Path, help='CMake build directory (default: engine/build/<platform>)')
    parser.add_argument('--platform', help='Target platform, used for the default build directory')
    parser.add_argument('--config', help='Configuration of the coverage object list, for multi-config generators')
    parser.add_argument('--out', type=Path, help='Output directory (default: <build-dir>/coverage/report)')
    parser.add_argument('--llvm-bin', help='Directory containing llvm-profdata and llvm-cov')
    args = parser.parse_args()

    if not args.build_dir:
        if not args.platform:
            parser.error('pass --build-dir or --platform')
        args.build_dir = REPOSITORY / 'engine' / 'build' / args.platform
    build_dir = args.build_dir.resolve()
    cache = read_cmake_cache(build_dir)
    if cache.get('WITH_COVERAGE', '').upper() not in ('ON', 'TRUE', '1', 'YES'):
        raise SystemExit('%s was not configured with WITH_COVERAGE=ON' % build_dir)
    source_root = Path(cache.get('CMAKE_HOME_DIRECTORY', REPOSITORY)).resolve()
    profile_dir = Path(cache['DEFOLD_COVERAGE_PROFILE_DIR'])
    output = (args.out or build_dir / 'coverage' / 'report').resolve()

    profdata, llvm_cov = find_llvm_tools(cache['CMAKE_CXX_COMPILER'], args.llvm_bin)
    profiles = sorted(profile_dir.glob('*.profraw'))
    if not profiles:
        raise SystemExit('No .profraw files in %s. Run the tests first.' % profile_dir)
    objects = read_objects(build_dir, args.config)

    if output.exists():
        shutil.rmtree(output)
    output.mkdir(parents=True)
    profile_list = output / 'profiles.txt'
    profile_list.write_text(''.join('%s\n' % path for path in profiles), encoding='utf-8')
    merged = output / 'coverage.profdata'
    # A crashing test can leave a truncated profile; report the rest.
    run([profdata, 'merge', '-sparse', '--failure-mode=all', '-f', profile_list, '-o', merged])

    # Thousands of object files do not fit on a command line.
    response = output / 'objects.rsp'
    response.write_text('\n'.join(['"%s"' % objects[0]] + ['-object="%s"' % path for path in objects[1:]]) + '\n',
                        encoding='utf-8')
    common = ['@%s' % response, '-instr-profile=%s' % merged, '-ignore-filename-regex=%s' % ignore_regex(cache, source_root)]

    run([llvm_cov, 'show', *common, '-format=html', '-output-dir=%s' % (output / 'html'),
         '-show-branches=count', '-show-line-counts-or-regions'])
    with (output / 'lcov.info').open('wb') as lcov:
        run([llvm_cov, 'export', *common, '-format=lcov'], stdout=lcov)
    summary = output / 'summary.json'
    with summary.open('wb') as json_summary:
        run([llvm_cov, 'export', *common, '-summary-only'], stdout=json_summary)
    with (output / 'report.txt').open('wb') as report:
        run([llvm_cov, 'report', *common], stdout=report)
    write_markdown_summary(summary, source_root, output / 'summary.md')
    print((output / 'summary.md').read_text(encoding='utf-8'))
    print('Coverage report: %s' % (output / 'html' / 'index.html'))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
