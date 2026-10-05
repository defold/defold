"""Run a macOS benchmark with temporary sleep/screen-saver inhibition."""
import argparse
import json
import os
import signal
import subprocess
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--settings', type=Path, required=True)
    parser.add_argument('command', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    command = args.command[1:] if args.command[:1] == ['--'] else args.command
    if not command:
        parser.error('Missing command')
    previous = subprocess.run(['defaults', '-currentHost', 'read', 'com.apple.screensaver', 'idleTime'],
                              text=True, capture_output=True)
    saved = {'existed': previous.returncode == 0,
             'idleTime': int(previous.stdout.strip()) if previous.returncode == 0 else None}
    # Never overwrite the recovery record of a previous interrupted collection.
    with args.settings.open('x') as f:
        json.dump(saved, f, indent=2)
    def interrupted(signum, frame):
        raise KeyboardInterrupt
    signal.signal(signal.SIGTERM, interrupted)
    child = None
    try:
        subprocess.run(['defaults', '-currentHost', 'write', 'com.apple.screensaver', 'idleTime', '-int', '0'], check=True)
        subprocess.run(['killall', 'ScreenSaverEngine'], capture_output=True)
        child = subprocess.Popen(['caffeinate', '-diu', '-t', '14400', *command], start_new_session=True)
        return child.wait()
    finally:
        if child is not None and child.poll() is None:
            os.killpg(child.pid, signal.SIGTERM)
            child.wait()
        restore = ['defaults', '-currentHost']
        restore += (['write', 'com.apple.screensaver', 'idleTime', '-int', str(saved['idleTime'])]
                    if saved['existed'] else ['delete', 'com.apple.screensaver', 'idleTime'])
        subprocess.run(restore, check=True)
        print('Original screen-saver timeout restored.', flush=True)


if __name__ == '__main__':
    raise SystemExit(main())
