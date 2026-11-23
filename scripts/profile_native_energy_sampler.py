"""Run only powermetrics with administrator privileges and own its lifetime."""

import argparse
from contextlib import ExitStack, closing
import os
import select
import signal
import subprocess
import sys


def collect(sample_count, control, output):
    signals = (signal.SIGHUP, signal.SIGINT, signal.SIGTERM)
    previous_mask = signal.pthread_sigmask(signal.SIG_BLOCK, signals)
    sampler = None
    cancelled = False
    try:
        # Do not let a signal arrive between spawning and taking ownership of the
        # child. The supervisor handles cancellation and uses SIGKILL on it.
        sampler = subprocess.Popen([
            "/usr/bin/powermetrics", "--samplers", "cpu_power,gpu_power,ane_power,tasks",
            "--show-process-energy", "--format", "plist", "--buffer-size", "0",
            "--sample-rate", "500", "--sample-count", str(sample_count or -1),
        ], stdin=subprocess.DEVNULL, stdout=output)
        signal.pthread_sigmask(signal.SIG_SETMASK, previous_mask)
        # EOF on the runner's pipe cancels even if sudo authorization has expired.
        # The process event also lets a normally completed sampler exit promptly.
        with closing(select.kqueue()) as queue:
            events = [select.kevent(control.fileno(), filter=select.KQ_FILTER_READ,
                                    flags=select.KQ_EV_ADD),
                      select.kevent(sampler.pid, filter=select.KQ_FILTER_PROC,
                                    flags=select.KQ_EV_ADD, fflags=select.KQ_NOTE_EXIT)]
            try:
                received = queue.control(events, 1, sample_count * 0.5 + 10 if sample_count else None)
                cancelled = any(event.filter == select.KQ_FILTER_READ for event in received)
            except ProcessLookupError:
                # The sampler can exit before its process event is registered.
                pass
    finally:
        signal.pthread_sigmask(signal.SIG_BLOCK, signals)
        try:
            if sampler:
                if sampler.poll() is None:
                    sampler.kill()
                result = sampler.wait()
        finally:
            signal.pthread_sigmask(signal.SIG_SETMASK, previous_mask)
    return 0 if cancelled else result


def interrupted(signum, frame):
    # Run collect's cleanup when sudo forwards a terminal or termination signal.
    raise SystemExit(128 + signum)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("sample_count", type=int)
    parser.add_argument("--control", help="Engine-owned lifetime FIFO; EOF stops the sampler")
    parser.add_argument("--output", help="Engine-owned sample FIFO")
    args = parser.parse_args()
    if args.sample_count < 0:
        parser.error("sample_count must be nonnegative; zero samples until control closes")
    if bool(args.control) != bool(args.output):
        parser.error("--control and --output must be supplied together")
    for sig in (signal.SIGHUP, signal.SIGINT, signal.SIGTERM):
        signal.signal(sig, interrupted)
    with ExitStack() as resources:
        control = sys.stdin
        output = sys.stdout
        if args.control:
            # Nonblocking opens also handle the engine exiting while the user
            # is answering the authorization dialog: no reader means no launch,
            # and EOF on control is observable even before the sampler starts.
            control = resources.enter_context(os.fdopen(os.open(args.control, os.O_RDONLY | os.O_NONBLOCK), "rb", buffering=0))
            output = resources.enter_context(os.fdopen(os.open(args.output, os.O_WRONLY | os.O_NONBLOCK), "wb", buffering=0))
            os.set_blocking(output.fileno(), True)
        sys.exit(collect(args.sample_count, control, output))
