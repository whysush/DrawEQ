#!/usr/bin/env python3
"""Turns a folder of DrawEQFrames PNGs into a GIF.

    ./Tools/make_gifs.py <frame-dir> <out.gif> [--width 820] [--frame MS]
                         [--hold-first MS] [--hold-last MS] [--pause INDEX:MS]

Two details matter for the result. Every frame is quantised against one shared
palette rather than its own, because per-frame palettes make consecutive frames
differ in pixels that did not actually change, which defeats the compressor and
roughly doubles the file. And the durations are per-frame, so a still opening
beat and a hold on the finished curve cost 2 frames rather than 20 duplicates.
"""

import argparse
import pathlib
import sys

from PIL import Image


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("frame_dir", type=pathlib.Path)
    ap.add_argument("output", type=pathlib.Path)
    ap.add_argument("--width", type=int, default=820)
    ap.add_argument("--frame", type=int, default=110, help="ms per frame")
    ap.add_argument("--hold-first", type=int, default=700)
    ap.add_argument("--hold-last", type=int, default=1600)
    ap.add_argument("--pause", action="append", default=[],
                    help="INDEX:MS - hold a specific frame longer")
    ap.add_argument("--colors", type=int, default=128)
    ap.add_argument("--stride", type=int, default=1,
                    help="keep every Nth frame - a smooth sweep does not need "
                         "every step, and dropping half the frames halves the "
                         "file for no visible cost")
    args = ap.parse_args()

    paths = sorted(args.frame_dir.glob("frame-*.png"))[::args.stride]
    if not paths:
        print(f"no frames in {args.frame_dir}", file=sys.stderr)
        return 1

    frames = []
    for p in paths:
        im = Image.open(p).convert("RGB")
        height = round(im.height * args.width / im.width)
        frames.append(im.resize((args.width, height), Image.LANCZOS))

    # One palette for the whole animation, built from a strip of evenly spaced
    # frames so it covers the colours the animation actually reaches rather
    # than only those in frame zero - the fitted line and the band markers do
    # not exist until near the end.
    step = max(1, len(frames) // 12)
    rows = list(range(0, len(frames), step))
    sample = Image.new("RGB", (args.width, len(rows) * 8))

    for row, i in enumerate(rows):
        sample.paste(frames[i].resize((args.width, 8), Image.LANCZOS), (0, row * 8))

    palette = sample.quantize(colors=args.colors, method=Image.MEDIANCUT)

    quantised = [f.quantize(palette=palette, dither=Image.FLOYDSTEINBERG)
                 for f in frames]

    durations = [args.frame] * len(quantised)
    durations[0] = args.hold_first
    durations[-1] = args.hold_last

    for spec in args.pause:
        index, _, ms = spec.partition(":")
        durations[int(index) % len(durations)] = int(ms)

    quantised[0].save(
        args.output,
        save_all=True,
        append_images=quantised[1:],
        duration=durations,
        loop=0,
        optimize=True,
        disposal=1,
    )

    size = args.output.stat().st_size
    print(f"{args.output}: {len(quantised)} frames, "
          f"{quantised[0].width}x{quantised[0].height}, {size / 1e6:.2f} MB")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
