"""Regression tests for the isolated tiled Slice-9 geometry reference mesh.

These deliberately do not claim Defold engine / graphics tests have passed.
Run with stdlib only:
  python3 -m unittest discover -s scripts -p 'test_slice9_tiling_reference.py' -v
"""

import unittest

from slice9_tiling_reference import UV, tiled_slice9_quads


class TiledSlice9GeometryTest(unittest.TestCase):
    # Different left/right and bottom/top margins expose flip ordering bugs.
    NODE = (29, 22)
    FRAME = (18, 14)
    MARGINS = (3, 2, 5, 4)
    ATLAS = (0.1, 0.2, 0.3, 0.4)

    def mesh(self, **options):
        return tiled_slice9_quads(
            self.NODE, self.FRAME, self.MARGINS, self.ATLAS, **options
        )

    def test_corner_is_fixed_and_untiled(self):
        quads = self.mesh()
        bottom_left = [q for q in quads if q.cell == (0, 0)]
        self.assertEqual(len(bottom_left), 1)
        corner = bottom_left[0]
        self.assertEqual((corner.x0, corner.y0, corner.x1, corner.y1), (0, 0, 3, 4))
        self.assertEqual(corner.uv_bl, UV(0.1, 0.2))
        self.assertAlmostEqual(corner.uv_tr.u, 0.1 + 0.2 * 3 / 18)
        self.assertAlmostEqual(corner.uv_tr.v, 0.2 + 0.2 * 4 / 14)

    def test_edges_repeat_on_one_axis_only(self):
        quads = self.mesh()
        top = sorted((q for q in quads if q.cell == (1, 2)), key=lambda q: q.x0)
        left = sorted((q for q in quads if q.cell == (0, 1)), key=lambda q: q.y0)
        self.assertEqual([(q.x1 - q.x0) for q in top], [10, 10, 1])
        self.assertEqual([(q.y1 - q.y0) for q in left], [8, 8])
        self.assertEqual(top[0].uv_bl, top[1].uv_bl)
        self.assertEqual(top[0].uv_tl, top[1].uv_tl)
        self.assertEqual(left[0].uv_bl, left[1].uv_bl)
        self.assertEqual(left[0].uv_br, left[1].uv_br)
        # Vertical coordinates cover the entire top margin, not a repeat.
        self.assertAlmostEqual(top[0].uv_tl.v - top[0].uv_bl.v, 0.2 * 2 / 14)

    def test_center_tiles_on_both_axes_and_clips_last_tile_uv(self):
        center = [q for q in self.mesh() if q.cell == (1, 1)]
        self.assertEqual(len(center), 6)
        self.assertEqual(sorted({q.x1 - q.x0 for q in center}), [1, 10])
        self.assertEqual(sorted({q.y1 - q.y0 for q in center}), [8])
        first = next(q for q in center if q.x0 == 3 and q.y0 == 4)
        second = next(q for q in center if q.x0 == 13 and q.y0 == 4)
        self.assertEqual(first.uv_bl, second.uv_bl)
        last = next(q for q in center if q.x0 == 23 and q.y0 == 4)
        self.assertEqual(last.uv_bl, first.uv_bl)
        # A 1-pixel partial final repeat must sample only ONE source pixel.
        self.assertAlmostEqual(last.uv_br.u - last.uv_bl.u, 0.2 / 18)
        self.assertAlmostEqual(last.uv_tl.v - last.uv_bl.v, 0.2 * 8 / 14)

    def test_tiled_quads_cover_node_exactly_once_without_overlaps(self):
        quads = self.mesh()
        self.assertEqual(len(quads), 20)
        self.assertEqual(sum(q.area for q in quads), 29 * 22)
        self.assertTrue(all(q.area > 0 for q in quads))
        for y in range(22):
            for x in range(29):
                hits = sum(
                    q.x0 <= x + 0.5 < q.x1 and q.y0 <= y + 0.5 < q.y1
                    for q in quads
                )
                self.assertEqual(hits, 1, (x, y))

    def test_atlas_subregion_uvs_do_not_reach_neighbouring_frames(self):
        # Tiny packed sprite inside a larger atlas. Sampler WRAP is not used.
        rect = (0.24, 0.36, 0.31, 0.41)
        for rotated in (False, True):
            for flip_x, flip_y in ((False, False), (True, False), (False, True), (True, True)):
                with self.subTest(rotated=rotated, flip_x=flip_x, flip_y=flip_y):
                    quads = tiled_slice9_quads(
                        self.NODE, self.FRAME, self.MARGINS, rect,
                        rotated=rotated, flip_x=flip_x, flip_y=flip_y,
                    )
                    for q in quads:
                        for uv in (q.uv_bl, q.uv_tl, q.uv_tr, q.uv_br):
                            self.assertGreaterEqual(uv.u, rect[0] - 1e-12)
                            self.assertLessEqual(uv.u, rect[2] + 1e-12)
                            self.assertGreaterEqual(uv.v, rect[1] - 1e-12)
                            self.assertLessEqual(uv.v, rect[3] + 1e-12)

    def test_rotated_atlas_maps_local_axes_into_swapped_uv_axes(self):
        normal = self.mesh()
        rotated = self.mesh(rotated=True)
        self.assertEqual([(q.cell, q.x0, q.y0, q.x1, q.y1) for q in normal],
                         [(q.cell, q.x0, q.y0, q.x1, q.y1) for q in rotated])
        center = next(q for q in rotated if q.cell == (1, 1) and q.x0 == 3 and q.y0 == 4)
        # Source (x=3, y=4) -> packed 90 CW: (u=t_y, v=1-t_x).
        self.assertAlmostEqual(center.uv_bl.u, 0.1 + 0.2 * 4 / 14)
        self.assertAlmostEqual(center.uv_bl.v, 0.2 + 0.2 * (1 - 3 / 18))

    def test_asymmetric_flip_mirrors_physical_margins_and_uvs(self):
        plain = next(q for q in self.mesh() if q.cell == (0, 0))
        mirrored = next(q for q in self.mesh(flip_x=True, flip_y=True) if q.cell == (0, 0))
        self.assertEqual((mirrored.x0, mirrored.y0, mirrored.x1, mirrored.y1),
                         (26, 18, 29, 22))
        self.assertEqual(mirrored.uv_bl, plain.uv_tr)
        self.assertEqual(mirrored.uv_tl, plain.uv_br)
        self.assertEqual(mirrored.uv_tr, plain.uv_bl)
        self.assertEqual(mirrored.uv_br, plain.uv_tl)
        self.assertEqual(sum(q.area for q in self.mesh(flip_x=True, flip_y=True)), 29 * 22)
        source_right = next(q for q in self.mesh(flip_x=True) if q.cell == (2, 0))
        self.assertEqual((source_right.x0, source_right.x1), (0, 5))

    def test_node_smaller_than_one_repeat_period_uses_one_clipped_tile(self):
        quads = tiled_slice9_quads((9, 9), self.FRAME, self.MARGINS, self.ATLAS)
        center = [q for q in quads if q.cell == (1, 1)]
        self.assertEqual(len(center), 1)
        self.assertEqual((center[0].x1 - center[0].x0, center[0].y1 - center[0].y0),
                         (1, 3))
        self.assertAlmostEqual(center[0].uv_br.u - center[0].uv_bl.u, 0.2 / 18)
        self.assertAlmostEqual(center[0].uv_tl.v - center[0].uv_bl.v, 0.2 * 3 / 14)
        self.assertEqual(sum(q.area for q in quads), 81)

    def test_no_margins_only_center_and_no_degenerate_quads(self):
        quads = tiled_slice9_quads((17, 18), (8, 8), (0, 0, 0, 0), self.ATLAS)
        self.assertEqual(len(quads), 9)
        self.assertTrue(all(q.cell == (1, 1) and q.area > 0 for q in quads))
        self.assertEqual(sum(q.area for q in quads), 306)

    def test_geometry_budget_prevents_quad_explosion(self):
        with self.assertRaisesRegex(ValueError, "max_quads"):
            tiled_slice9_quads(
                (500, 500), (12, 12), (5, 5, 5, 5), self.ATLAS, max_quads=32
            )

    def test_rejects_invalid_or_undefined_geometry(self):
        cases = (
            ((0, 22), self.FRAME, self.MARGINS, self.ATLAS),
            ((29, 22), (8, 8), self.MARGINS, self.ATLAS),
            ((7, 22), self.FRAME, self.MARGINS, self.ATLAS),
            ((29, 22), self.FRAME, (-1, 2, 5, 4), self.ATLAS),
            ((29, 22), self.FRAME, self.MARGINS, (0.4, 0.2, 0.3, 0.4)),
            ((float("nan"), 22), self.FRAME, self.MARGINS, self.ATLAS),
        )
        for args in cases:
            with self.subTest(args=args), self.assertRaises(ValueError):
                tiled_slice9_quads(*args)


if __name__ == "__main__":
    unittest.main()
