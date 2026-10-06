"""Chroma key in native Python and with OpenCV."""

from __future__ import annotations

import argparse
import time
from pathlib import Path
from typing import Callable

import cv2
import numpy as np

Image = np.ndarray
Color = tuple[int, int, int]


def _validate_inputs(foreground: Image, background: Image, lower: Color, upper: Color) -> None:
    if foreground.ndim != 3 or foreground.shape[2] != 3:
        raise ValueError("foreground must be an H x W x 3 RGB image")
    if background.shape != foreground.shape:
        raise ValueError("background and foreground must have equal shapes")
    if foreground.dtype != np.uint8 or background.dtype != np.uint8:
        raise ValueError("images must use uint8 channels")
    if len(lower) != 3 or len(upper) != 3:
        raise ValueError("color boundaries must contain three channels")
    if any(not 0 <= value <= 255 for value in (*lower, *upper)):
        raise ValueError("color channels must be between 0 and 255")
    if any(lo > hi for lo, hi in zip(lower, upper)):
        raise ValueError("each lower boundary must not exceed the upper boundary")


def chroma_key_native(foreground: Image, background: Image, lower: Color, upper: Color) -> Image:
    """Replace matching RGB pixels using explicit Python loops."""
    _validate_inputs(foreground, background, lower, upper)
    result = foreground.copy()
    height, width, _ = foreground.shape
    for y in range(height):
        for x in range(width):
            red, green, blue = (int(channel) for channel in foreground[y, x])
            if (
                lower[0] <= red <= upper[0]
                and lower[1] <= green <= upper[1]
                and lower[2] <= blue <= upper[2]
            ):
                result[y, x] = background[y, x]
    return result


def chroma_key_opencv(foreground: Image, background: Image, lower: Color, upper: Color) -> Image:
    """Replace matching RGB pixels using OpenCV's vectorised mask."""
    _validate_inputs(foreground, background, lower, upper)
    mask = cv2.inRange(foreground, np.array(lower, np.uint8), np.array(upper, np.uint8))
    result = foreground.copy()
    cv2.copyTo(background, mask, result)
    return result


def read_rgb(path: Path) -> Image:
    image = cv2.imread(str(path), cv2.IMREAD_COLOR)
    if image is None:
        raise ValueError(f"cannot read image: {path}")
    return cv2.cvtColor(image, cv2.COLOR_BGR2RGB)


def write_rgb(path: Path, image: Image) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if not cv2.imwrite(str(path), cv2.cvtColor(image, cv2.COLOR_RGB2BGR)):
        raise OSError(f"cannot write image: {path}")


def parse_color(value: str) -> Color:
    try:
        channels = tuple(int(part.strip()) for part in value.split(","))
    except ValueError as exc:
        raise argparse.ArgumentTypeError("expected R,G,B integers") from exc
    if len(channels) != 3 or any(channel < 0 or channel > 255 for channel in channels):
        raise argparse.ArgumentTypeError("expected three channel values from 0 to 255")
    return channels  # type: ignore[return-value]


def _timed(function: Callable[..., Image], *args: object) -> tuple[Image, float]:
    started = time.perf_counter()
    result = function(*args)
    return result, (time.perf_counter() - started) * 1000


def main() -> None:
    parser = argparse.ArgumentParser(description="Replace a selected RGB range with another image")
    parser.add_argument("foreground", type=Path, help="image containing the chroma-key color")
    parser.add_argument("background", type=Path, help="replacement image")
    parser.add_argument("output", type=Path, help="result image")
    parser.add_argument("--lower", type=parse_color, default=(0, 160, 0), help="lower RGB boundary")
    parser.add_argument("--upper", type=parse_color, default=(120, 255, 120), help="upper RGB boundary")
    parser.add_argument("--method", choices=("opencv", "native"), default="opencv")
    args = parser.parse_args()

    foreground = read_rgb(args.foreground)
    background = read_rgb(args.background)
    if background.shape[:2] != foreground.shape[:2]:
        background = cv2.resize(background, (foreground.shape[1], foreground.shape[0]))
    function = chroma_key_opencv if args.method == "opencv" else chroma_key_native
    result, elapsed = _timed(function, foreground, background, args.lower, args.upper)
    write_rgb(args.output, result)
    print(f"method={args.method}; size={foreground.shape[1]}x{foreground.shape[0]}; time={elapsed:.3f} ms")


if __name__ == "__main__":
    main()
