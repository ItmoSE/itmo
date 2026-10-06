import numpy as np
import pytest

from chroma_key import chroma_key_native, chroma_key_opencv, parse_color


@pytest.mark.parametrize("implementation", [chroma_key_native, chroma_key_opencv])
def test_replaces_only_pixels_inside_inclusive_range(implementation):
    foreground = np.array([[[0, 200, 0], [10, 10, 10]], [[120, 255, 120], [121, 200, 20]]], dtype=np.uint8)
    background = np.full_like(foreground, [7, 8, 9])
    actual = implementation(foreground, background, (0, 160, 0), (120, 255, 120))
    expected = np.array([[[7, 8, 9], [10, 10, 10]], [[7, 8, 9], [121, 200, 20]]], dtype=np.uint8)
    np.testing.assert_array_equal(actual, expected)


def test_implementations_are_equivalent_on_random_images():
    generator = np.random.default_rng(42)
    foreground = generator.integers(0, 256, (50, 80, 3), dtype=np.uint8)
    background = generator.integers(0, 256, (50, 80, 3), dtype=np.uint8)
    native = chroma_key_native(foreground, background, (20, 100, 30), (180, 240, 200))
    library = chroma_key_opencv(foreground, background, (20, 100, 30), (180, 240, 200))
    np.testing.assert_array_equal(native, library)


def test_rejects_mismatched_shapes():
    with pytest.raises(ValueError, match="equal shapes"):
        chroma_key_native(np.zeros((2, 2, 3), np.uint8), np.zeros((1, 2, 3), np.uint8), (0, 0, 0), (1, 1, 1))


def test_parse_color():
    assert parse_color("10, 20,255") == (10, 20, 255)
