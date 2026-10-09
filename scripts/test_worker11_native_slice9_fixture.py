"""Cheap pre-build verification of Defold #5400 native GUI fixtures.

This suite cannot replace the new C++ TiledSlice9GuiTest, which must run
against a Defold engine assembled with the *new* builtins material resource.

Run: PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s scripts \
    -p 'test_worker11_native_slice9_fixture.py' -v
"""

import re
import unittest
from pathlib import Path

from slice9_tiling_reference import tiled_slice9_quads


ROOT = Path(__file__).resolve().parent.parent
GUI = ROOT / "engine/gamesys/src/gamesys/test/gui"


class NativeTiledGuiFixtureCheck(unittest.TestCase):
    def setUp(self):
        self.tiled = (GUI / "render_box_tiled_test.gui").read_text()
        self.stretch = (GUI / "render_box_tiled_stretch_test.gui").read_text()

    def test_static_tilesource_available_and_nonpaged(self):
        source = (GUI / "render_box_test1.tilesource").read_text()
        self.assertIn('tile_width: 32', source)
        self.assertIn('tile_height: 32', source)
        self.assertIn('image: "/gui/tile_tile_anim.png"', source)
        self.assertTrue((GUI / "tile_tile_anim.png").is_file())
        self.assertNotIn("max_page_count", source)
        for scene in (self.tiled, self.stretch):
            self.assertIn('texture: "/gui/render_box_test1.tilesource"', scene)
            self.assertIn('texture: "render_box/anim"', scene)
            meaningful = "\n".join(line for line in scene.splitlines()
                                  if not line.lstrip().startswith("#"))
            self.assertNotIn("gui.new_texture", meaningful)

    def test_tiled_registration_scene_label_not_just_material_internal_name(self):
        self.assertRegex(self.tiled, r'materials\s*\{\s*name:\s*"slice9_tiled"\s*material:\s*"/builtins/materials/slice9_tiled\.material"\s*\}')
        self.assertRegex(self.tiled, r'(?s)nodes\s*\{.*material:\s*"slice9_tiled"')
        self.assertNotIn('material: "slice9_tiled"', self.stretch)
        self.assertNotIn('materials {', self.stretch)
        self.assertTrue((ROOT / "engine/engine/content/builtins/materials/slice9_tiled.material").is_file())

    def test_same_size_manual_mode_and_slice_margins(self):
        for scene in (self.tiled, self.stretch):
            self.assertRegex(scene, r'size\s*\{\s*x:\s*64\.0\s*y:\s*48\.0')
            self.assertRegex(scene, r'slice9\s*\{\s*x:\s*2\.0\s*y:\s*2\.0\s*z:\s*2\.0\s*w:\s*2\.0\s*\}')
            self.assertIn("size_mode: SIZE_MODE_MANUAL", scene)
            self.assertIn("type: TYPE_BOX", scene)

    def test_go_paths_and_explicit_test_content_build_inputs(self):
        build_inputs = (GUI / "build.inputs").read_text().splitlines()
        for name in ("render_box_tiled_test", "render_box_tiled_stretch_test"):
            go = (GUI / (name + ".go")).read_text()
            self.assertIn(f'component: "/gui/{name}.gui"', go)
            self.assertIn(f"/gui/{name}.go", build_inputs)

    def test_independent_atlas_oracle_20_quads_120_verts_partial_edges(self):
        quads = tiled_slice9_quads((64, 48), (32, 32), (2, 2, 2, 2),
                                   (0, 0.5, 0.5, 1), max_quads=2048)
        self.assertEqual(20, len(quads))
        self.assertEqual(120, 6 * len(quads))
        self.assertEqual(3072, sum(q.area for q in quads))
        self.assertEqual(6, sum(q.cell == (1, 1) for q in quads))
        last_center = [q for q in quads if q.cell == (1, 1) and q.x1 == 62 and q.y1 == 46]
        self.assertEqual(1, len(last_center))
        q = last_center[0]
        self.assertEqual((58, 30, 62, 46), (q.x0, q.y0, q.x1, q.y1))
        self.assertAlmostEqual(0.09375, q.uv_tr.u)
        self.assertAlmostEqual(0.78125, q.uv_tr.v)

    def test_native_golden_harness_checks_both_modes_and_clipped_uvs(self):
        cpp = (ROOT / "engine/gamesys/src/gamesys/test/test_gamesys_gui.cpp").read_text()
        self.assertIn("TEST_P(TiledSlice9GuiTest, StaticFrameTiledVersusStretch)", cpp)
        self.assertIn('{ "/gui/render_box_tiled_test.goc", 120, true }', cpp)
        self.assertIn('{ "/gui/render_box_tiled_stretch_test.goc", 54, false }', cpp)
        self.assertIn("0.09375f", cpp)
        self.assertIn("m_ClientVertexBuffer.Size()", cpp)
        self.assertIn("m_PageIndex", cpp)


if __name__ == "__main__":
    unittest.main()
