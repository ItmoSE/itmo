"""Benchmark both chroma-key implementations on pic1.jpg and pic2.jpg."""

from __future__ import annotations

import csv
import time
from pathlib import Path

import cv2
import matplotlib.pyplot as plt
import numpy as np

from chroma_key import chroma_key_native, chroma_key_opencv, read_rgb, write_rgb

INPUTS = (Path("pic1.jpg"), Path("pic2.jpg"))
BACKGROUND = Path("assets/background.png")
RESULTS = Path("results")
LOWER = (0, 70, 0)
UPPER = (170, 255, 180)


def timed(function, foreground: np.ndarray, background: np.ndarray) -> tuple[np.ndarray, float]:
    started = time.perf_counter()
    result = function(foreground, background, LOWER, UPPER)
    return result, (time.perf_counter() - started) * 1000


def main() -> None:
    background_source = read_rgb(BACKGROUND)
    (RESULTS / "native").mkdir(parents=True, exist_ok=True)
    (RESULTS / "opencv").mkdir(parents=True, exist_ok=True)
    rows: list[dict[str, str]] = []

    for path in INPUTS:
        foreground = read_rgb(path)
        height, width = foreground.shape[:2]
        background = cv2.resize(background_source, (width, height), interpolation=cv2.INTER_LINEAR)
        native_result, native_ms = timed(chroma_key_native, foreground, background)
        opencv_result, opencv_ms = timed(chroma_key_opencv, foreground, background)
        if not np.array_equal(native_result, opencv_result):
            raise AssertionError(f"implementations produced different pixels for {path}")
        write_rgb(RESULTS / "native" / f"{path.stem}.jpg", native_result)
        write_rgb(RESULTS / "opencv" / f"{path.stem}.jpg", opencv_result)
        rows.append({
            "image": path.name,
            "size": f"{width}x{height}",
            "pixels": str(width * height),
            "native_ms": f"{native_ms:.3f}",
            "opencv_ms": f"{opencv_ms:.3f}",
            "speedup": f"{native_ms / opencv_ms:.1f}",
        })
        print(f"{path.name}: native={native_ms:.3f} ms, opencv={opencv_ms:.3f} ms, speedup={native_ms / opencv_ms:.1f}x")

    with (RESULTS / "benchmark.csv").open("w", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(output, fieldnames=rows[0].keys())
        writer.writeheader()
        writer.writerows(rows)

    labels = [row["image"] for row in rows]
    native_times = [float(row["native_ms"]) for row in rows]
    opencv_times = [float(row["opencv_ms"]) for row in rows]
    x = np.arange(len(labels))
    _, axis = plt.subplots(figsize=(7, 4.2))
    axis.bar(x - 0.18, native_times, 0.36, label="Native Python")
    axis.bar(x + 0.18, opencv_times, 0.36, label="OpenCV")
    axis.set_yscale("log")
    axis.set_ylabel("Time, ms (log scale)")
    axis.set_xticks(x, labels)
    axis.grid(axis="y", alpha=0.25)
    axis.legend()
    plt.tight_layout()
    plt.savefig(RESULTS / "benchmark.svg")
    plt.close()


if __name__ == "__main__":
    main()
