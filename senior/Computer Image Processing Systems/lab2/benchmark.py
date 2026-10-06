"""Run the three supplied inputs and save reproducible metrics and illustrations."""

import csv
import json
from pathlib import Path

import cv2
import numpy as np

from tracker import run_video


def read_tile(path: Path) -> cv2.typing.MatLike:
    frame = cv2.imread(str(path))
    if frame is None:
        raise ValueError(f"Cannot read snapshot: {path}")
    return cv2.resize(frame, (480, 270))


def main():
    cv2.setNumThreads(1)
    results = Path("results")
    summaries = []
    for name in ("mona-lisa", "mona-lisa-blur", "mona-lisa-blur-extra-credit"):
        summary = run_video(Path("test-videos") / f"{name}.avi", results / name,
                            label="Mona Lisa")
        summaries.append(summary)
        print(f"{name}: {summary['detected_frames']}/{summary['frames']}, "
              f"{summary['processing_fps']:.1f} FPS", flush=True)
        tiles = [read_tile(results / name / sample)
                 for sample in summary["samples"]]
        if (results / name / "first_lost.jpg").exists():
            tiles.append(read_tile(results / name / "first_lost.jpg"))
        while len(tiles) < 6:
            tiles.append(np.zeros((270, 480, 3), np.uint8))
        sheet = np.vstack([np.hstack(tiles[i:i + 3]) for i in (0, 3)])
        cv2.imwrite(str(results / name / "samples.jpg"), sheet)
    (results / "benchmark.json").write_text(
        json.dumps(summaries, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    timeline = np.full((250, 1200, 3), 255, np.uint8)
    for row, summary in enumerate(summaries):
        y = 35 + row * 70
        cv2.putText(timeline, Path(summary["video"]).stem, (10, y),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.55, (0, 0, 0), 1, cv2.LINE_AA)
        with (results / Path(summary["video"]).stem / "frames.csv").open() as file:
            flags = [int(r["found"]) for r in csv.DictReader(file)]
        for x in range(1180):
            flag = flags[min(len(flags) - 1, int(x * len(flags) / 1180))]
            cv2.line(timeline, (10 + x, y + 8), (10 + x, y + 27),
                     (60, 170, 60) if flag else (70, 70, 210), 1)
    cv2.putText(timeline, "0%                 Green: detected; red: lost                 100% of video",
                (10, 240), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 0, 0), 1, cv2.LINE_AA)
    cv2.imwrite(str(results / "detection_timeline.png"), timeline)


if __name__ == "__main__":
    main()
