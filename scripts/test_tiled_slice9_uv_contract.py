"""Standalone acceptance examples for a potential Defold tiled Slice-9 mode.

This is a mathematical reference, not an engine implementation or an approved
material/vertex contract. The upstream API and shader approach remain undecided.

Run: python3 -m unittest discover -s scripts -p 'test_tiled_slice9_uv_contract.py' -v
"""

import unittest


def atlas_uv(
    x, y, rendered_width, rendered_height, source_width, source_height,
    uv_bounds, *, repeat_x=False, repeat_y=False, rotated=False,
    flip_x=False, flip_y=False,
):
    """Map a pixel position within one Slice-9 cell into its atlas rectangle.

    Pixel coordinates are strictly inside the rendered cell. Restricting to
    interior samples avoids specifying texture-filter seam behavior, which
    must be resolved by an actual shader design and GPU render tests.
    """
    if min(rendered_width, rendered_height, source_width, source_height) <= 0:
        raise ValueError("All source and rendered dimensions must be positive")
    if not (0 < x < rendered_width and 0 < y < rendered_height):
        raise ValueError("Expected an interior pixel sample")

    u0, v0, u1, v1 = uv_bounds
    if not (0 <= u0 < u1 <= 1 and 0 <= v0 < v1 <= 1):
        raise ValueError("Expected a subrectangle of an atlas")

    tx = (x % source_width) / source_width if repeat_x else x / rendered_width
    ty = (y % source_height) / source_height if repeat_y else y / rendered_height
    if flip_x:
        tx = 1 - tx
    if flip_y:
        ty = 1 - ty

    # Rotation here is clockwise. The engine's atlas descriptor determines
    # actual rotation orientation, which future integration must respect.
    if rotated:
        tx, ty = ty, 1 - tx
    return u0 + tx * (u1 - u0), v0 + ty * (v1 - v0)


class TiledSlice9Contract(unittest.TestCase):
    RECT = (0.25, 0.125, 0.50, 0.25)

    def test_corner_is_not_tiled(self):
        args = (2, 3, 8, 6, 8, 6, self.RECT)
        self.assertEqual((0.3125, 0.1875), atlas_uv(*args))

    def test_top_and_bottom_edges_repeat_only_x(self):
        args = (25, 4, 8, 4, self.RECT)
        a = atlas_uv(1, 1, *args, repeat_x=True)
        b = atlas_uv(9, 1, *args, repeat_x=True)
        c = atlas_uv(17, 1, *args, repeat_x=True)
        self.assertEqual((0.28125, 0.15625), a)
        self.assertEqual(a, b)
        self.assertEqual(b, c)
        # The corresponding unstretched Y location is preserved.
        self.assertEqual((0.28125, 0.21875), atlas_uv(1, 3, *args, repeat_x=True))

    def test_left_and_right_edges_repeat_only_y(self):
        args = (4, 18, 4, 6, self.RECT)
        first = atlas_uv(1, 2, *args, repeat_y=True)
        self.assertEqual((0.3125, 0.16666666666666666), first)
        self.assertEqual(first, atlas_uv(1, 8, *args, repeat_y=True))
        self.assertEqual(first, atlas_uv(1, 14, *args, repeat_y=True))

    def test_center_repeats_both_axes_with_partial_last_tile(self):
        args = (14, 11, 6, 4, self.RECT)
        self.assertEqual(
            atlas_uv(1, 1, *args, repeat_x=True, repeat_y=True),
            atlas_uv(13, 9, *args, repeat_x=True, repeat_y=True),
        )
        self.assertEqual(
            (0.2916666666666667, 0.15625),
            atlas_uv(13, 9, *args, repeat_x=True, repeat_y=True),
        )

    def test_atlas_subrectangle_remains_isolated(self):
        rect = (0.26, 0.34, 0.33, 0.41)
        for x, y in ((0.5, 0.5), (5, 3), (11, 9), (15.5, 13.5)):
            u, v = atlas_uv(x, y, 16, 14, 3, 4, rect, repeat_x=True, repeat_y=True)
            self.assertLessEqual(rect[0], u)
            self.assertLessEqual(u, rect[2])
            self.assertLessEqual(rect[1], v)
            self.assertLessEqual(v, rect[3])

    def test_rotated_atlas_and_axis_flip(self):
        rect = (0.125, 0.25, 0.375, 0.50)
        args = (10, 11, 19, 17, 8, 4, rect)
        uv = atlas_uv(*args, repeat_x=True, repeat_y=True, rotated=True)
        self.assertEqual((0.3125, 0.4375), uv)
        flipped = atlas_uv(*args, repeat_x=True, repeat_y=True, rotated=True, flip_x=True)
        self.assertEqual((0.3125, 0.3125), flipped)

    def test_stretch_path_does_not_repeat(self):
        rect = self.RECT
        a = atlas_uv(1, 1, 20, 20, 4, 4, rect)
        b = atlas_uv(5, 1, 20, 20, 4, 4, rect)
        self.assertNotEqual(a, b)
        self.assertEqual((0.2625, 0.13125), a)

    def test_rejects_degenerate_geometry(self):
        with self.assertRaises(ValueError):
            atlas_uv(1, 1, 0, 8, 4, 4, self.RECT)
        with self.assertRaises(ValueError):
            atlas_uv(1, 1, 8, 8, 0, 4, self.RECT)


if __name__ == "__main__":
    unittest.main()
