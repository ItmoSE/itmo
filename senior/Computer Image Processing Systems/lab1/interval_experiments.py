"""Create chroma-key examples for exact, narrow, medium and broad RGB ranges."""

from __future__ import annotations

import csv
from pathlib import Path

import cv2
import numpy as np

from chroma_key import chroma_key_opencv, read_rgb, write_rgb

BACKGROUND = Path("assets/background.png")
OUTPUT = Path("results/intervals")
BROAD = ((0, 70, 0), (170, 255, 180))
SAMPLES = {
    Path("pic1.jpg"): (70, 129, 71),
    Path("pic2.jpg"): (105, 187, 59),
}


def around(color: tuple[int, int, int], radius: int) -> tuple[tuple[int, int, int], tuple[int, int, int]]:
    lower = tuple(max(0, channel - radius) for channel in color)
    upper = tuple(min(255, channel + radius) for channel in color)
    return lower, upper  # type: ignore[return-value]


def preview(image: np.ndarray, max_width: int = 1400) -> np.ndarray:
    if image.shape[1] <= max_width:
        return image
    scale = max_width / image.shape[1]
    return cv2.resize(image, (max_width, round(image.shape[0] * scale)), interpolation=cv2.INTER_AREA)


def main() -> None:
    background_source = read_rgb(BACKGROUND)
    rows: list[dict[str, str]] = []

    for path, sample in SAMPLES.items():
        foreground = read_rgb(path)
        height, width = foreground.shape[:2]
        background = cv2.resize(background_source, (width, height), interpolation=cv2.INTER_LINEAR)
        ranges = {
            "exact": (sample, sample),
            "narrow_5": around(sample, 5),
            "medium_20": around(sample, 20),
            "broad": BROAD,
        }
        target = OUTPUT / path.stem
        target.mkdir(parents=True, exist_ok=True)

        for name, (lower, upper) in ranges.items():
            result = chroma_key_opencv(foreground, background, lower, upper)
            mask = cv2.inRange(foreground, np.array(lower, np.uint8), np.array(upper, np.uint8))
            selected = int(np.count_nonzero(mask))
            write_rgb(target / f"{name}.jpg", preview(result))
            rows.append({
                "image": path.name,
                "variant": name,
                "lower_rgb": ",".join(map(str, lower)),
                "upper_rgb": ",".join(map(str, upper)),
                "selected_pixels": str(selected),
                "selected_percent": f"{selected / (width * height) * 100:.3f}",
            })

    OUTPUT.mkdir(parents=True, exist_ok=True)
    with (OUTPUT / "intervals.csv").open("w", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(output, fieldnames=rows[0].keys())
        writer.writeheader()
        writer.writerows(rows)


if __name__ == "__main__":
    main()
