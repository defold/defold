"""Minimal numeric golden fixture for a future Defold GUI tiled-Slice9 test.

Run (stdlib only):
  python3 -m unittest discover -s scripts -p 'test_slice9_tiling_rotated.py' -v

This file tests a standalone geometry oracle, NOT native C++ or a chosen GUI
API. It supplies an exact, renderer-independent pixel-sampling requirement:
when tiled Slice9 is enabled, a clipped repeat in a 90-degree rotated atlas
must sample its own sub-tile rather than the stretched center.

Future GUI C++ golden-test setup (engine/gamesys/.../comp_gui.cpp):
  Atlas: 16x16 texels, source frame 4x4 packed at x=4..8, y=2..6;
  rotated UV rectangle: (1/4, 1/8, 1/2, 3/8).
  Rotated tc corners: TL=(1/4,3/8), TR=(1/2,3/8),
                      BR=(1/2,1/8), BL=(1/4,1/8).
  GUI node 5x5; Slice9 (L,T,R,B)=(1,1,1,1); no flips; local origin BL.

Smallest non-degenerate integer example with source-center period 2 and a
single 1-pixel partial repeat in both center axes. A shader implementation
need not generate the geometry oracle's 16 quads; a native golden can check
the final pixel sample instead of committing to a vertex-count contract.
"""

import unittest
from fractions import Fraction

from slice9_tiling_reference import tiled_slice9_quads


class RotatedClippedGolden(unittest.TestCase):
    SOURCE = (4, 4)
    NODE = (5, 5)
    SLICE9 = (1, 1, 1, 1)
    ATLAS = (1 / 4, 1 / 8, 1 / 2, 3 / 8)

    def reference(self):
        return tiled_slice9_quads(
            self.NODE, self.SOURCE, self.SLICE9, self.ATLAS, rotated=True
        )

    def clipped_center(self):
        return next(
            q for q in self.reference()
            if q.cell == (1, 1) and (q.x0, q.y0, q.x1, q.y1) == (3, 3, 4, 4)
        )

    def test_cpp_six_vertex_golden_for_rotated_clipped_quad(self):
        q = self.clipped_center()
        # comp_gui.cpp RenderBoxNodes emits [v00, v10, v11, v00, v11, v01].
        # Every value is an exact power-of-two fraction, safe for a float32
        # native golden comparison without a rounding-ambiguity tolerance.
        vertices = (
            (q.x0, q.y0, *q.uv_bl),
            (q.x1, q.y0, *q.uv_br),
            (q.x1, q.y1, *q.uv_tr),
            (q.x0, q.y0, *q.uv_bl),
            (q.x1, q.y1, *q.uv_tr),
            (q.x0, q.y1, *q.uv_tl),
        )
        self.assertEqual(vertices, (
            (3, 3, 5 / 16, 5 / 16),
            (4, 3, 5 / 16, 1 / 4),
            (4, 4, 3 / 8, 1 / 4),
            (3, 3, 5 / 16, 5 / 16),
            (4, 4, 3 / 8, 1 / 4),
            (3, 4, 3 / 8, 5 / 16),
        ))

    def test_partial_tile_not_stretched_or_outside_atlas(self):
        quads = self.reference()
        center = [q for q in quads if q.cell == (1, 1)]
        self.assertEqual(len(center), 4)
        self.assertEqual(len(quads), 16)  # Oracle only, not native API contract.
        self.assertEqual(sum(q.area for q in quads), 25)
        self.assertEqual((self.clipped_center().x1 - self.clipped_center().x0,
                          self.clipped_center().y1 - self.clipped_center().y0), (1, 1))
        # Source interval is [1,2] on each axis; rotated atlas swaps them.
        q = self.clipped_center()
        self.assertEqual(q.uv_tl.u - q.uv_bl.u, 1 / 16)
        self.assertEqual(q.uv_bl.v - q.uv_br.v, 1 / 16)
        for q in quads:
            for uv in (q.uv_bl, q.uv_tl, q.uv_tr, q.uv_br):
                self.assertGreaterEqual(uv.u, self.ATLAS[0])
                self.assertLessEqual(uv.u, self.ATLAS[2])
                self.assertGreaterEqual(uv.v, self.ATLAS[1])
                self.assertLessEqual(uv.v, self.ATLAS[3])

    def test_interior_pixel_distinguishes_repeat_from_legacy_stretch(self):
        # Sample at (3.5,3.5), strictly INSIDE last center repeat (no seam).
        q = self.clipped_center()
        half = Fraction(1, 2)
        sample_x = 3 + half
        sample_y = 3 + half
        s = (float(sample_x - Fraction(q.x0)) / (q.x1 - q.x0))
        t = (float(sample_y - Fraction(q.y0)) / (q.y1 - q.y0))
        tiled_u = q.uv_bl.u + t * (q.uv_tl.u - q.uv_bl.u)
        tiled_v = q.uv_bl.v + s * (q.uv_br.v - q.uv_bl.v)
        self.assertEqual((tiled_u, tiled_v), (11 / 32, 9 / 32))

        # Current Defold GUI STRETCH mapping for the same 4x4 atlas frame:
        # comp_gui.cpp rotated center UV (u,v) boundaries are
        # bottom-left=(5/16,5/16), top-right=(7/16,3/16).
        # Center destination covers [1,4]x[1,4]; sample fraction = 5/6.
        stretch_fraction = Fraction(5, 6)
        stretched_u = Fraction(5, 16) + stretch_fraction * Fraction(1, 8)
        stretched_v = Fraction(5, 16) - stretch_fraction * Fraction(1, 8)
        self.assertEqual((stretched_u, stretched_v),
                         (Fraction(5, 12), Fraction(5, 24)))
        self.assertEqual(Fraction(tiled_u).limit_denominator(), Fraction(11, 32))
        self.assertEqual(Fraction(tiled_v).limit_denominator(), Fraction(9, 32))
        # A golden asserting (11/32, 9/32) at this interior sample MUST fail
        # on legacy stretch (5/12, 5/24) once a tiled native path is enabled.
        self.assertEqual(Fraction(tiled_u).limit_denominator() - stretched_u,
                         -Fraction(7, 96))
        self.assertEqual(Fraction(tiled_v).limit_denominator() - stretched_v,
                         Fraction(7, 96))


if __name__ == '__main__':
    unittest.main()
