# Copyright 2020-2026 The Defold Foundation
# Licensed under the Defold License version 1.0

import argparse
import json
import os
from pathlib import Path
import time


def cache_status(outcome, hit):
    if outcome == 'success':
        return 'hit' if hit == 'true' else 'miss'
    return outcome or 'unavailable'


def format_report(state, finished_ns, environment):
    restore = (state['setup_started_ns'] - state['started_ns']) / 1e9
    setup = (finished_ns - state['setup_started_ns']) / 1e9
    status = cache_status(environment.get('DOTNET_CACHE_OUTCOME'), environment.get('DOTNET_CACHE_HIT'))
    return '\n'.join([
        '### Windows .NET SDK timing',
        '',
        f"SDK: {environment.get('DOTNET_SDK_VERSION', 'unavailable')}",
        '',
        '| Measurement | Result |',
        '|---|---:|',
        f'| Cache result | {status} |',
        f"| Setup result | {environment.get('DOTNET_SETUP_OUTCOME', 'unavailable')} |",
        f'| Cache lookup / restore | {restore:.2f}s |',
        f'| SDK setup | {setup:.2f}s |',
        f'| Total before build | {restore + setup:.2f}s |',
        '',
        'Durations include time between steps. Post-job cache saving is excluded.',
        'Compare successful cache hits to measure warm-cache performance.',
        '',
    ])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('event', choices=('start', 'setup', 'report'))
    parser.add_argument('--state', type=Path, required=True)
    args = parser.parse_args()
    now = time.monotonic_ns()
    if args.event == 'start':
        state = {'started_ns': now}
    else:
        state = json.loads(args.state.read_text())
    if args.event == 'setup':
        state['setup_started_ns'] = now
    if args.event == 'report':
        report = format_report(state, now, os.environ)
        print(report)
        if os.environ.get('GITHUB_STEP_SUMMARY'):
            with open(os.environ['GITHUB_STEP_SUMMARY'], 'a', encoding='utf-8') as summary:
                summary.write(report)
    else:
        args.state.write_text(json.dumps(state))


if __name__ == '__main__':
    main()
