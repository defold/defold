"""Merge raw profiles from a WITH_COVERAGE engine build and write coverage reports.

Writes, into the output directory: html/, lcov.info, summary.json, report.txt,
meta.json (commit and source root, for use as a later baseline), and summary.md
(per engine library totals, suitable for a CI job summary). With --baseline, the
summary includes changes against an earlier report; with --diff-base or a
baseline commit, it also reports coverage of the lines changed since that commit
and writes changed_lines.json.
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


METRICS = ('lines', 'functions', 'branches')


def relative_path(source_root, filename):
    try:
        return Path(filename).resolve().relative_to(source_root).as_posix()
    except ValueError:
        return Path(filename).as_posix()


def library_name(path):
    if path.startswith('/'):
        return 'other'
    parts = path.split('/')
    if len(parts) > 2 and parts[0] == 'engine':
        return parts[1]
    return parts[0] if len(parts) > 1 else 'other'


def read_summary(path, source_root):
    """Returns {relative file: {metric: (covered, count)}} from llvm-cov export -summary-only."""
    data = json.loads(Path(path).read_text(encoding='utf-8'))['data'][0]
    return {relative_path(source_root, entry['filename']):
            {key: (entry['summary'][key]['covered'], entry['summary'][key]['count']) for key in METRICS}
            for entry in data['files']}


def group_totals(files, key=library_name):
    groups = {}
    for path, metrics in files.items():
        totals = groups.setdefault(key(path), {metric: [0, 0] for metric in METRICS})
        for metric in METRICS:
            totals[metric][0] += metrics[metric][0]
            totals[metric][1] += metrics[metric][1]
    return groups


def ratio(covered, count):
    return 100.0 * covered / count if count else None


def percent(covered, count):
    value = ratio(covered, count)
    return '-' if value is None else '%.1f%%' % value


def delta(current, baseline):
    if baseline is None:
        return ''
    now, before = ratio(*current), ratio(*baseline)
    if now is None or before is None:
        return ''
    return ' %+.1f' % (now - before)


def read_lcov(path, source_root):
    """Returns {relative file: {line: execution count}} for instrumented lines."""
    lines = {}
    current = None
    for line in Path(path).read_text(encoding='utf-8', errors='replace').splitlines():
        if line.startswith('SF:'):
            current = lines.setdefault(relative_path(source_root, line[3:]), {})
        elif line.startswith('DA:') and current is not None:
            number, count = line[3:].split(',')[:2]
            current[int(number)] = max(current.get(int(number), 0), int(count))
        elif line == 'end_of_record':
            current = None
    return lines


def parse_changed_lines(diff):
    """Returns {path: set of added or modified line numbers} from git diff --unified=0."""
    changed = {}
    current = None
    for line in diff.splitlines():
        if line.startswith('+++ '):
            target = line[4:]
            current = None if target == '/dev/null' else changed.setdefault(target[2:] if target.startswith('b/') else target, set())
        elif line.startswith('@@') and current is not None:
            match = re.match(r'^@@ -\S+ \+(\d+)(?:,(\d+))? @@', line)
            if match:
                start = int(match.group(1))
                count = 1 if match.group(2) is None else int(match.group(2))
                current.update(range(start, start + count))
    return {path: lines for path, lines in changed.items() if lines}


def changed_line_coverage(lcov, changed):
    """Coverage of changed lines that are instrumented; other changed lines are not executable code."""
    files = {}
    for path, numbers in sorted(changed.items()):
        counts = lcov.get(path)
        if not counts:
            continue
        executable = sorted(number for number in numbers if number in counts)
        if executable:
            files[path] = {'covered': [n for n in executable if counts[n] > 0],
                           'uncovered': [n for n in executable if counts[n] == 0]}
    covered = sum(len(entry['covered']) for entry in files.values())
    count = covered + sum(len(entry['uncovered']) for entry in files.values())
    return {'covered': covered, 'count': count, 'files': files}


def line_ranges(numbers, limit=None):
    ranges = []
    for number in numbers:
        if ranges and number == ranges[-1][1] + 1:
            ranges[-1][1] = number
        else:
            ranges.append([number, number])
    text = ', '.join(str(a) if a == b else '%d-%d' % (a, b) for a, b in ranges[:limit])
    if limit is not None and len(ranges) > limit:
        text += ', … (%d more)' % (len(ranges) - limit)
    return text


def git(source_root, *args):
    return subprocess.run(['git', '-C', str(source_root), *args], check=True,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                          encoding='utf-8', errors='replace').stdout


def short_ref(ref):
    return ref[:12] if ref and re.fullmatch(r'[0-9a-f]{40}', ref) else ref


def markdown_summary(files, baseline=None, baseline_commit=None, changed=None, diff_base=None, limit=20):
    lines = ['## C/C++ code coverage', '']
    if baseline is not None:
        lines += ['Changes are in percentage points against %s.' % (short_ref(baseline_commit) or 'the baseline'), '']
    lines += ['| Library | Lines | Functions | Branches |', '|---|---:|---:|---:|']
    groups = group_totals(files)
    before = group_totals(baseline) if baseline is not None else {}
    for name in sorted(groups):
        lines.append('| %s | %s |' % (name, ' | '.join(
            '%s%s (%d/%d)' % (percent(*groups[name][m]), delta(groups[name][m], before.get(name, {}).get(m)),
                              *groups[name][m]) for m in METRICS)))
    total = group_totals(files, key=lambda path: 'total').get('total', {m: [0, 0] for m in METRICS})
    total_before = group_totals(baseline, key=lambda path: 'total').get('total') if baseline is not None else None
    lines.append('| **Total** | %s |' % ' | '.join(
        '**%s**%s (%d/%d)' % (percent(*total[m]), delta(total[m], total_before and total_before[m]), *total[m])
        for m in METRICS))

    if baseline is not None:
        changes = []
        for path in set(files) | set(baseline):
            now = ratio(*files[path]['lines']) if path in files else None
            then = ratio(*baseline[path]['lines']) if path in baseline else None
            if now != then and not (now is not None and then is not None and abs(now - then) < 0.05):
                changes.append((path, then, now))
        if changes:
            changes.sort(key=lambda change: ((change[2] or 0) - (change[1] or 0), change[0]))
            lines += ['', '### Largest line coverage changes', '', '| File | Before | After |', '|---|---:|---:|']
            for path, then, now in changes[:limit]:
                lines.append('| `%s` | %s | %s |' % (path, '-' if then is None else '%.1f%%' % then,
                                                     'removed' if now is None else '%.1f%%' % now))

    if changed is not None:
        lines += ['', '### Changed lines since %s' % short_ref(diff_base), '']
        if changed['count']:
            lines.append('%s of changed executable lines are covered (%d/%d).' % (
                percent(changed['covered'], changed['count']), changed['covered'], changed['count']))
            uncovered = [(path, entry) for path, entry in changed['files'].items() if entry['uncovered']]
            if uncovered:
                uncovered.sort(key=lambda item: (-len(item[1]['uncovered']), item[0]))
                lines += ['', '| File | Covered | Uncovered lines |', '|---|---:|---|']
                for path, entry in uncovered[:limit]:
                    total_lines = len(entry['covered']) + len(entry['uncovered'])
                    lines.append('| `%s` | %d/%d | %s |' % (path, len(entry['covered']), total_lines,
                                                             line_ranges(entry['uncovered'], 12)))
        else:
            lines.append('No changed executable lines in the reported sources.')
    return '\n'.join(lines) + '\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--build-dir', type=Path, help='CMake build directory (default: engine/build/<platform>)')
    parser.add_argument('--platform', help='Target platform, used for the default build directory')
    parser.add_argument('--config', help='Configuration of the coverage object list, for multi-config generators')
    parser.add_argument('--out', type=Path, help='Output directory (default: <build-dir>/coverage/report)')
    parser.add_argument('--llvm-bin', help='Directory containing llvm-profdata and llvm-cov')
    parser.add_argument('--baseline', type=Path,
                        help='Directory with summary.json and meta.json of an earlier report to compare against')
    parser.add_argument('--diff-base', help='Git commit to report changed-line coverage against, such as the '
                        'merge base with dev (default: the baseline commit)')
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

    commit = None
    try:
        commit = git(source_root, 'rev-parse', 'HEAD').strip()
    except (OSError, subprocess.CalledProcessError):
        pass
    (output / 'meta.json').write_text(json.dumps({'commit': commit, 'source_root': source_root.as_posix()}, indent=2) + '\n',
                                      encoding='utf-8')

    files = read_summary(summary, source_root)
    baseline = baseline_commit = None
    if args.baseline:
        meta_path = args.baseline / 'meta.json'
        meta = json.loads(meta_path.read_text(encoding='utf-8')) if meta_path.is_file() else {}
        baseline_root = Path(meta.get('source_root') or source_root)
        baseline = read_summary(args.baseline / 'summary.json', baseline_root)
        baseline_commit = meta.get('commit')

    changed = None
    diff_base = args.diff_base or baseline_commit
    if diff_base:
        try:
            # Compares against the working tree, so local uncommitted changes count too.
            diff = git(source_root, 'diff', '--unified=0', '--no-color', '--no-ext-diff',
                       '--src-prefix=a/', '--dst-prefix=b/', diff_base, '--', 'engine')
        except (OSError, subprocess.CalledProcessError) as error:
            print('warning: cannot diff against %s, skipping changed-line coverage: %s'
                  % (diff_base, getattr(error, 'stderr', error)), file=sys.stderr)
        else:
            changed = changed_line_coverage(read_lcov(output / 'lcov.info', source_root), parse_changed_lines(diff))
            (output / 'changed_lines.json').write_text(json.dumps(dict(changed, base=diff_base), indent=2) + '\n',
                                                       encoding='utf-8')

    (output / 'summary.md').write_text(markdown_summary(files, baseline, baseline_commit, changed, diff_base),
                                       encoding='utf-8')
    print((output / 'summary.md').read_text(encoding='utf-8'))
    print('Coverage report: %s' % (output / 'html' / 'index.html'))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
