"""Planar object tracking with SIFT, optical flow and RANSAC homography."""

from __future__ import annotations

import argparse
import csv
import json
import math
import time
from collections.abc import Sequence
from dataclasses import dataclass
from pathlib import Path

import cv2
import numpy as np
from numpy.typing import NDArray

type ROI = tuple[int, int, int, int]
type FloatArray = NDArray[np.float32]
type Frame = cv2.typing.MatLike
type DetectionState = tuple[Detection, Frame, FloatArray, FloatArray, NDArray[np.bool_] | None]


@dataclass
class Detection:
    corners: FloatArray | None = None
    matches: int = 0
    inliers: int = 0
    flow_points: int = 0
    error: float | None = None
    reason: str = "few_matches"


def validate_roi(roi: Sequence[int], shape: Sequence[int]) -> ROI:
    x, y, w, h = roi
    height, width = shape[:2]
    if x < 0 or y < 0 or w < 2 or h < 2 or x + w > width or y + h > height:
        raise ValueError(
            f"ROI {roi} is outside {width}x{height} or smaller than 2x2")
    return x, y, w, h


class PlanarTracker:
    """Reference matching plus validated short-term optical flow of keypoints."""

    def __init__(self, reference: Frame, roi: Sequence[int] | None = None, *,
                 ratio: float = 0.75, min_inliers: int = 8,
                 min_inlier_ratio: float = 0.45, ransac_threshold: float = 3.0,
                 features: int = 2500) -> None:
        self.roi = validate_roi(roi or (0, 0, reference.shape[1], reference.shape[0]),
                                reference.shape)
        x, y, w, h = self.roi
        template = reference[y:y + h, x:x + w]
        self.sift = cv2.SIFT.create(nfeatures=features, contrastThreshold=0.02)
        # Small copies of the first frame model loss of fine texture when the
        # camera moves away. All coordinates are mapped to the original ROI.
        gray = cv2.cvtColor(template, cv2.COLOR_BGR2GRAY)
        self.keypoints: list[cv2.KeyPoint] = []
        descriptor_blocks: list[Frame] = []
        for scale in (1.0, 0.5, 0.25):
            if min(w, h) * scale < 32:
                continue
            reduced = cv2.resize(gray, None, fx=scale,
                                 fy=scale, interpolation=cv2.INTER_AREA)
            points, descriptors = self.sift.detectAndCompute(reduced, None)
            if descriptors is None:
                continue
            sx, sy = reduced.shape[1] / w, reduced.shape[0] / h
            for point in points:
                point.pt = (point.pt[0] / sx, point.pt[1] / sy)
                self.keypoints.append(point)
            descriptor_blocks.append(descriptors)
        reference_descriptors = np.vstack(
            descriptor_blocks) if descriptor_blocks else None
        if reference_descriptors is None or len(self.keypoints) < min_inliers:
            raise ValueError(
                "Too few reference features. Use a textured object or a larger ROI.")
        self.descriptors = reference_descriptors
        self.matcher = cv2.BFMatcher(cv2.NORM_L2)
        self.template_corners = np.asarray([[0, 0], [w - 1, 0],
                                            [w - 1, h - 1], [0, h - 1]], dtype=np.float32)
        self.template_area = float((w - 1) * (h - 1))
        self.ratio = ratio
        self.min_inliers = min_inliers
        self.min_inlier_ratio = min_inlier_ratio
        self.ransac_threshold = ransac_threshold
        self.previous_gray: Frame | None = None
        self.previous_src: FloatArray | None = None
        self.previous_dst: FloatArray | None = None

    def detect(self, frame: Frame) -> Detection:
        result, gray, src, dst, inside = self._detect(frame)
        self.previous_gray = gray
        if result.corners is None:
            self.previous_src = self.previous_dst = None
        else:
            assert inside is not None
            indices = np.flatnonzero(inside)
            # Bound optical flow cost while keeping points across the object.
            if len(indices) > 500:
                indices = indices[np.linspace(
                    0, len(indices) - 1, 500).astype(int)]
            self.previous_src = src[indices]
            self.previous_dst = dst[indices]
        return result

    def _detect(self, frame: Frame) -> DetectionState:
        result = Detection()
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        kp, descriptors = self.sift.detectAndCompute(gray, None)
        pairs = (self.matcher.knnMatch(self.descriptors, descriptors, k=2)
                 if descriptors is not None and len(kp) >= 2 else [])
        good = [p[0] for p in pairs if len(p) == 2 and
                p[0].distance < self.ratio * p[1].distance]
        # Several reference descriptors must not vote for the same frame feature.
        unique: dict[int, cv2.DMatch] = {}
        for match in sorted(good, key=lambda m: m.distance):
            unique.setdefault(match.trainIdx, match)
        good = list(unique.values())
        result.matches = len(good)
        src = np.asarray(
            [self.keypoints[m.queryIdx].pt for m in good], dtype=np.float32).reshape(-1, 2)
        dst = np.asarray([kp[m.trainIdx].pt for m in good], dtype=np.float32).reshape(-1, 2)
        if (self.previous_dst is not None and self.previous_src is not None and
                self.previous_gray is not None and self.previous_gray.shape == gray.shape):
            points = self.previous_dst[:, None, :]
            criteria = (cv2.TERM_CRITERIA_EPS | cv2.TERM_CRITERIA_COUNT, 30, 0.01)
            forward, status, error = cv2.calcOpticalFlowPyrLK(
                self.previous_gray, gray, points, points.copy(),
                winSize=(21, 21), maxLevel=3, criteria=criteria)
            if forward is not None:
                backward, reverse_status, _ = cv2.calcOpticalFlowPyrLK(
                    gray, self.previous_gray, forward, forward.copy(),
                    winSize=(21, 21), maxLevel=3, criteria=criteria)
                if backward is not None:
                    valid = ((status.ravel() == 1) & (reverse_status.ravel() == 1) &
                             (error.ravel() < 30) &
                             (np.linalg.norm(backward[:, 0] - points[:, 0], axis=1) < 1.5))
                    valid &= ((forward[:, 0, 0] >= 0) & (forward[:, 0, 0] < gray.shape[1]) &
                              (forward[:, 0, 1] >= 0) & (forward[:, 0, 1] < gray.shape[0]))
                    result.flow_points = int(valid.sum())
                    src = np.vstack((src, self.previous_src[valid]))
                    dst = np.vstack((dst, forward[valid, 0]))
        # Repeated SIFT detections and old flow tracks can represent the same
        # location. Count it once, giving fresh descriptor matches priority.
        kept, used_reference, used_frame = [], set(), set()
        for i, (a, b) in enumerate(zip(src, dst)):
            reference_cell = tuple(np.rint(a / 2).astype(int))
            frame_cell = tuple(np.rint(b).astype(int))
            if reference_cell in used_reference or frame_cell in used_frame:
                continue
            kept.append(i)
            used_reference.add(reference_cell)
            used_frame.add(frame_cell)
        result.flow_points = sum(i >= len(good) for i in kept)
        src, dst = src[kept], dst[kept]

        def finish() -> DetectionState:
            return result, gray, src, dst, None
        if len(src) < self.min_inliers:
            result.reason = "no_features" if descriptors is None else "few_matches"
            return finish()
        homography, mask = cv2.findHomography(src, dst, cv2.RANSAC,
                                              self.ransac_threshold,
                                              maxIters=2000, confidence=0.995)
        if homography is None or mask is None or not np.isfinite(homography).all():
            result.reason = "no_homography"
            return finish()
        inside = mask.ravel().astype(bool)
        result.inliers = int(inside.sum())
        if result.inliers < self.min_inliers or result.inliers / len(src) < self.min_inlier_ratio:
            result.reason = "few_inliers"
            return finish()
        projected = cv2.perspectiveTransform(
            src[inside, None, :], homography)[:, 0]
        result.error = float(
            np.median(np.linalg.norm(projected - dst[inside], axis=1)))
        # Prevent extrapolating the entire object from a tiny patch or a line.
        coverage = abs(cv2.contourArea(
            cv2.convexHull(src[inside]))) / self.template_area
        if coverage < 0.01 or result.error > self.ransac_threshold:
            result.reason = "poor_geometry"
            return finish()
        homogeneous = np.column_stack(
            (self.template_corners, np.ones(4))) @ homography.T
        denominators = homogeneous[:, 2]
        if not (np.all(denominators > 1e-8) or np.all(denominators < -1e-8)):
            result.reason = "invalid_projection"
            return finish()
        corners = (homogeneous[:, :2] /
                   denominators[:, None]).astype(np.float32)
        height, width = frame.shape[:2]
        area = abs(cv2.contourArea(corners))
        if (not np.isfinite(corners).all() or not cv2.isContourConvex(corners) or
                area < 64 or area > 4 * width * height or
                np.max(np.abs(corners)) > 4 * max(width, height)):
            result.reason = "invalid_polygon"
            return finish()
        visible_area, _ = cv2.intersectConvexConvex(
            corners, np.asarray([[0, 0], [width - 1, 0],
                                 [width - 1, height - 1], [0, height - 1]], dtype=np.float32))
        if visible_area < 64:
            result.reason = "outside_frame"
            return finish()
        result.corners = corners
        result.reason = "found"
        return result, gray, src, dst, inside


def annotate(frame: Frame, detection: Detection, label: str, index: int) -> Frame:
    image = frame.copy()
    if detection.corners is not None:
        polygon = np.rint(detection.corners).astype(np.int32)
        cv2.polylines(image, [polygon], True, (0, 255, 0), 2, cv2.LINE_AA)
        anchor = polygon[np.argmin(polygon[:, 1])]
        x = int(np.clip(anchor[0], 0, max(0, image.shape[1] - 160)))
        y = int(np.clip(anchor[1] - 8, 20, image.shape[0] - 8))
        cv2.putText(image, label, (x, y), cv2.FONT_HERSHEY_SIMPLEX, 0.6,
                    (0, 255, 0), 2, cv2.LINE_AA)
    status = (f"{index}: {detection.reason}  matches={detection.matches} "
              f"flow={detection.flow_points} inliers={detection.inliers}")
    cv2.rectangle(image, (0, image.shape[0] - 25), (image.shape[1], image.shape[0]),
                  (0, 0, 0), -1)
    cv2.putText(image, status, (6, image.shape[0] - 8), cv2.FONT_HERSHEY_SIMPLEX,
                0.42, (255, 255, 255), 1, cv2.LINE_AA)
    return image


def run_video(video: str | Path, output_dir: str | Path, *,
              roi: Sequence[int] | None = None, select_roi: bool = False, show: bool = False,
              label: str = "Object", ratio: float = 0.75, min_inliers: int = 8,
              min_inlier_ratio: float = 0.45, ransac_threshold: float = 3.0,
              features: int = 2500):
    video, output_dir = Path(video), Path(output_dir)
    if not video.is_file():
        raise ValueError(f"Input video does not exist: {video}")
    if video.resolve() == (output_dir / "tracked.mp4").resolve():
        raise ValueError("Output must not overwrite the input video")
    capture = cv2.VideoCapture(str(video))
    writer = None
    try:
        ok, first = capture.read()
        if not ok:
            raise ValueError(f"Cannot read first frame: {video}")
        if select_roi:
            roi = tuple(map(int, cv2.selectROI("Select object: Enter to accept, Esc to cancel",
                                               first, showCrosshair=True, fromCenter=False)))
            cv2.destroyAllWindows()
            if roi[2] == 0 or roi[3] == 0:
                raise ValueError("ROI selection cancelled")
        cv2.setRNGSeed(0)
        tracker = PlanarTracker(first, roi, ratio=ratio, min_inliers=min_inliers,
                                min_inlier_ratio=min_inlier_ratio,
                                ransac_threshold=ransac_threshold, features=features)
        height, width = first.shape[:2]
        fps = capture.get(cv2.CAP_PROP_FPS)
        if not math.isfinite(fps) or fps <= 0:
            fps = 30.0
        expected = int(capture.get(cv2.CAP_PROP_FRAME_COUNT))
        output_dir.mkdir(parents=True, exist_ok=True)
        writer = cv2.VideoWriter(str(output_dir / "tracked.mp4"),
                                 cv2.VideoWriter.fourcc(*"mp4v"), fps, (width, height))
        if not writer.isOpened():
            raise ValueError("Cannot create MP4 video with mp4v codec")
        cv2.imwrite(str(output_dir / "reference.jpg"), first)
        sample_indices = {0, max(0, expected - 1)} | {int(expected * v)
                                                      for v in (0.25, 0.5, 0.75)}
        samples, timings, errors, rows = [], [], [], []
        frame, index, first_lost_saved = first, 0, False
        started = time.perf_counter()
        with (output_dir / "frames.csv").open("w", newline="", encoding="utf-8") as file:
            fields = ["frame", "time_s", "found", "reason", "matches", "flow_points", "inliers",
                      "median_error_px", "processing_ms", "corners"]
            csv_writer = csv.DictWriter(file, fieldnames=fields)
            csv_writer.writeheader()
            while True:
                tick = time.perf_counter()
                detection = tracker.detect(frame)
                elapsed = (time.perf_counter() - tick) * 1000
                timings.append(elapsed)
                found = detection.corners is not None
                if detection.error is not None and found:
                    errors.append(detection.error)
                row = dict(frame=index, time_s=index / fps, found=int(found),
                           reason=detection.reason, matches=detection.matches,
                           flow_points=detection.flow_points,
                           inliers=detection.inliers, median_error_px=detection.error,
                           processing_ms=elapsed,
                           corners=json.dumps(detection.corners.tolist())
                           if detection.corners is not None else "")
                csv_writer.writerow(row)
                rows.append(row)
                annotated = annotate(frame, detection, label, index)
                writer.write(annotated)
                if index in sample_indices:
                    name = f"frame_{index:05d}.jpg"
                    cv2.imwrite(str(output_dir / name), annotated)
                    samples.append(name)
                if not found and not first_lost_saved:
                    cv2.imwrite(str(output_dir / "first_lost.jpg"), annotated)
                    first_lost_saved = True
                index += 1
                if show:
                    cv2.imshow("Tracking (Esc / Q to stop)", annotated)
                    if cv2.waitKey(1) & 0xFF in (27, ord("q")):
                        break
                ok, frame = capture.read()
                if not ok:
                    break
                if frame.shape[:2] != (height, width):
                    raise ValueError("Variable frame size is unsupported")
        summary = dict(video=str(video), frames=index, expected_frames=expected,
                       complete=(expected <= 0 or index == expected), width=width,
                       height=height, source_fps=fps, roi=list(tracker.roi),
                       reference_features=len(tracker.keypoints),
                       detected_frames=sum(r["found"] for r in rows),
                       detection_rate=sum(r["found"] for r in rows) / index,
                       median_reprojection_error_px=float(
                           np.median(errors)) if errors else None,
                       mean_processing_ms=float(np.mean(timings)),
                       processing_fps=1000 / float(np.mean(timings)),
                       elapsed_s=time.perf_counter() - started,
                       parameters=dict(ratio=ratio, min_inliers=min_inliers,
                                       min_inlier_ratio=min_inlier_ratio,
                                       ransac_threshold=ransac_threshold, features=features),
                       samples=samples,
                       versions=dict(python=__import__("platform").python_version(),
                                     opencv=cv2.__version__, numpy=np.__version__))
        (output_dir / "summary.json").write_text(
            json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        return summary
    finally:
        capture.release()
        if writer is not None:
            writer.release()
        if show or select_roi:
            cv2.destroyAllWindows()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("video", type=Path,
                        help="input video; first frame is the reference")
    parser.add_argument("--output-dir", type=Path)
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument("--roi", type=int, nargs=4,
                       metavar=("X", "Y", "W", "H"))
    modes.add_argument("--select-roi", action="store_true",
                       help="select first-frame ROI with mouse")
    parser.add_argument("--show", action="store_true",
                        help="display tracking while processing")
    parser.add_argument("--label", default="Object", help="ASCII object label")
    parser.add_argument("--ratio", type=float, default=0.75)
    parser.add_argument("--min-inliers", type=int, default=8)
    parser.add_argument("--min-inlier-ratio", type=float, default=0.45)
    parser.add_argument("--ransac-threshold", type=float, default=3.0)
    parser.add_argument("--features", type=int, default=2500)
    args = parser.parse_args()
    if not 0 < args.ratio < 1 or not 0 < args.min_inlier_ratio <= 1:
        parser.error("ratio must be in (0, 1); min-inlier-ratio in (0, 1]")
    if args.min_inliers < 4 or args.features < args.min_inliers or args.ransac_threshold <= 0:
        parser.error(
            "need >=4 inliers, features >= min-inliers, and a positive RANSAC threshold")
    try:
        summary = run_video(args.video, args.output_dir or Path("results") / args.video.stem,
                            roi=args.roi, select_roi=args.select_roi, show=args.show,
                            label=args.label, ratio=args.ratio, min_inliers=args.min_inliers,
                            min_inlier_ratio=args.min_inlier_ratio,
                            ransac_threshold=args.ransac_threshold, features=args.features)
    except (ValueError, cv2.error) as error:
        parser.exit(1, f"Error: {error}\n")
    print(json.dumps(summary, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
