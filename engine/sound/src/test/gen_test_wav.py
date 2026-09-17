#!/usr/bin/env python3

import argparse
import math
import struct
import wave


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True)
    parser.add_argument("--tone", type=int)
    parser.add_argument("--rate", type=int, required=True)
    parser.add_argument("--frames", type=int, required=True)
    parser.add_argument("--channels", type=int, required=True)
    parser.add_argument("--ramp", action="store_true")
    parser.add_argument("--dc", action="store_true")
    args = parser.parse_args()

    with wave.open(args.output, "wb") as out:
        out.setnchannels(args.channels)
        out.setsampwidth(2)
        out.setframerate(args.rate)
        frames = bytearray()
        for i in range(args.frames):
            if args.dc:
                sample = 0.8 * 32768
            else:
                sample = 0.8 * 32768 * math.sin((i * 2.0 * math.pi * args.tone) / args.rate)
            if args.ramp:
                sample *= ((args.frames - 1) - i) / float(args.frames)
            packed = struct.pack("h", int(sample))
            for _ in range(args.channels):
                frames.extend(packed)
        out.writeframes(frames)


if __name__ == "__main__":
    main()
