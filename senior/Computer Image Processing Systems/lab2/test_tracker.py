"""Checks with known geometry; no manual ground truth is needed for these tests."""

import csv
import tempfile
import unittest
from pathlib import Path

import cv2
import numpy as np

from tracker import PlanarTracker, run_video, validate_roi


class TrackingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cv2.setNumThreads(1)
        rng = np.random.default_rng(42)
        cls.reference = rng.integers(0, 256, (300, 420, 3), dtype=np.uint8)
        cls.reference = cv2.GaussianBlur(cls.reference, (3, 3), 0.7)
        for i in range(40):
            x, y = map(int, rng.integers([10, 10], [400, 280]))
            cv2.putText(cls.reference, str(i), (x, y), cv2.FONT_HERSHEY_SIMPLEX,
                        0.6, (255, 255, 255), 1, cv2.LINE_AA)

    def test_known_perspective_and_recovery(self):
        tracker = PlanarTracker(self.reference)
        target = np.asarray([[40, 25], [370, 10], [390, 260], [20, 275]], dtype=np.float32)
        matrix = cv2.getPerspectiveTransform(tracker.template_corners, target)
        warped = cv2.warpPerspective(self.reference, matrix, (420, 300))
        detection = tracker.detect(warped)
        if detection.corners is None:
            self.fail(detection.reason)
        self.assertLess(float(np.max(np.linalg.norm(detection.corners - target, axis=1))), 3)
        lost = tracker.detect(np.zeros_like(warped))
        self.assertIsNone(lost.corners)
        recovered = tracker.detect(warped)
        self.assertIsNotNone(recovered.corners, recovered.reason)

    def test_roi_uses_first_frame_coordinates(self):
        roi = (70, 40, 250, 200)
        tracker = PlanarTracker(self.reference, roi)
        result = tracker.detect(self.reference)
        expected = np.asarray([[70, 40], [319, 40], [319, 239], [70, 239]], dtype=np.float32)
        if result.corners is None:
            self.fail(result.reason)
        np.testing.assert_allclose(result.corners, expected, atol=1)

    def test_bad_input(self):
        with self.assertRaises(ValueError):
            PlanarTracker(np.zeros_like(self.reference))
        for roi in ((-1, 0, 50, 50), (400, 0, 30, 30), (0, 0, 0, 10)):
            with self.assertRaises(ValueError):
                validate_roi(roi, self.reference.shape)

    def test_unrelated_textured_frame_is_rejected(self):
        tracker = PlanarTracker(self.reference)
        self.assertIsNotNone(tracker.detect(self.reference).corners)
        unrelated = np.random.default_rng(101).integers(0, 256, self.reference.shape,
                                                       dtype=np.uint8)
        self.assertIsNone(tracker.detect(unrelated).corners)

    def test_flow_sequence_with_known_geometry(self):
        tracker = PlanarTracker(self.reference)
        self.assertIsNotNone(tracker.detect(self.reference).corners)
        for step in range(1, 11):
            target = np.asarray([[step * 3, step * 2], [419 - step * 5, step],
                                 [419 - step * 3, 299 - step * 2], [step * 2, 299 - step]],
                                dtype=np.float32)
            matrix = cv2.getPerspectiveTransform(tracker.template_corners, target)
            frame = cv2.warpPerspective(self.reference, matrix, (420, 300))
            result = tracker.detect(frame)
            if result.corners is None:
                self.fail(result.reason)
            self.assertGreater(result.flow_points, 0)
            self.assertLess(float(np.max(np.linalg.norm(result.corners - target, axis=1))), 3)

    def test_complete_video_output(self):
        with tempfile.TemporaryDirectory(prefix="lab2-test-") as directory:
            folder = Path(directory)
            source = folder / "input.avi"
            writer = cv2.VideoWriter(str(source), cv2.VideoWriter.fourcc(*"MJPG"),
                                     15, (420, 300))
            self.assertTrue(writer.isOpened())
            for frame in (self.reference, np.zeros_like(self.reference), self.reference):
                writer.write(frame)
            writer.release()
            summary = run_video(source, folder / "result")
            self.assertEqual(summary["frames"], 3)
            self.assertTrue(summary["complete"])
            with (folder / "result" / "frames.csv").open() as file:
                rows = list(csv.DictReader(file))
            self.assertEqual([int(row["found"]) for row in rows], [1, 0, 1])
            output = cv2.VideoCapture(str(folder / "result" / "tracked.mp4"))
            count = 0
            while output.read()[0]:
                count += 1
            self.assertEqual(count, 3)
            self.assertEqual(output.get(cv2.CAP_PROP_FPS), 15)
            output.release()


if __name__ == "__main__":
    unittest.main()
