"""Run native recording tests while an unrelated process plays a competing tone."""

import argparse
import array
import math
import pathlib
import subprocess
import tempfile
import wave
import winsound


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--test", required=True)
    args, remaining = parser.parse_known_args()
    # The recorder runs in a child process. Its process-loopback capture must
    # exclude this parent's audio; the native silence tests verify that exclusion.
    with tempfile.TemporaryDirectory(prefix="record-audio-isolation-") as directory:
        path = pathlib.Path(directory) / "other-process.wav"
        samples = array.array("h")
        for i in range(48000):
            sample = int(6000 * math.sin(i * 880 * math.tau / 48000))
            samples.extend((sample, sample))
        with wave.open(str(path), "wb") as output:
            output.setparams((2, 2, 48000, 0, "NONE", "not compressed"))
            output.writeframes(samples.tobytes())
        winsound.PlaySound(str(path), winsound.SND_FILENAME | winsound.SND_ASYNC |
                           winsound.SND_LOOP | winsound.SND_NODEFAULT)
        try:
            return subprocess.run([args.test, *remaining], timeout=90).returncode
        finally:
            winsound.PlaySound(None, 0)


if __name__ == "__main__":
    raise SystemExit(main())
