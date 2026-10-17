"""Private source-level regression for tiled Slice-9's atlas-page contract.

Run from the isolated Defold checkout:
  PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s scripts \
      -p test_worker86_paged_slice9_source_contract.py -v

Checks material/shader compatibility and runs the actual extracted C++ tile
segment helper. This cannot verify a renderer, GPU texture sampling or Bob.
"""

import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
MAT = ROOT / "engine/engine/content/builtins/materials"
CPP = ROOT / "engine/gamesys/src/gamesys/components/comp_gui.cpp"
CMAKE = ROOT / "engine/gamesys/src/gamesys/test/test_content.cmake"


def material_field(contents, field):
    match = re.search(rf'^{re.escape(field)}:\s*"?([^"\n]+)"?$', contents, re.M)
    if match is None:
        raise AssertionError(f"missing material field: {field}")
    return match.group(1)


class TiledSlice9PagedSourceContract(unittest.TestCase):
    def test_single_page_material_still_uses_2d_sampler(self):
        material = (MAT / "slice9_tiled.material").read_text()
        self.assertEqual("slice9_tiled", material_field(material, "name"))
        self.assertEqual("/builtins/materials/gui.vp", material_field(material, "vertex_program"))
        self.assertEqual("/builtins/materials/gui.fp", material_field(material, "fragment_program"))
        self.assertRegex(material, r'(?m)^max_page_count:\s*0\s*$')

    def test_paged_variant_samples_actual_atlas_layer(self):
        # Regression: PR 0d7f65f has only slice9_tiled.material with a
        # sampler2D and no page_index input. A page>0 must reach sampler2DArray.
        material = (MAT / "slice9_tiled_paged.material").read_text()
        stock = (MAT / "gui_paged_atlas.material").read_text()
        self.assertEqual("slice9_tiled_paged", material_field(material, "name"))
        for field in ("vertex_program", "fragment_program"):
            self.assertEqual(material_field(stock, field), material_field(material, field))
        self.assertEqual("4", material_field(material, "max_page_count"))
        vp = (MAT / "gui_paged_atlas.vp").read_text()
        fp = (MAT / "gui_paged_atlas.fp").read_text()
        self.assertRegex(vp, r'in\s+mediump\s+float\s+page_index\s*;')
        self.assertIn("var_page_index = page_index;", vp)
        self.assertIn("sampler2DArray texture_sampler", fp)
        self.assertIn("vec3(var_texcoord0.xy, var_page_index)", fp)

    def test_paged_material_optin_and_page_index_passthrough(self):
        cpp = CPP.read_text()
        optin = cpp.split("static bool IsTiledSlice9Material", 1)[1].split("struct GuiSlice9TileInfo", 1)[0]
        self.assertIn('dmHashString64("slice9_tiled")', optin)
        self.assertIn('dmHashString64("slice9_tiled_paged")', optin)
        body = cpp.split("static void RenderBoxNodes", 1)[1].split("ro.m_VertexCount = rendered_vert_count;", 1)[0]
        self.assertRegex(body, r'page_index\s*=\s*page_indices\[frame_index\];')
        self.assertEqual(4, len(re.findall(r'v(?:00|10|01|11)\.SetPageIndex\(page_index\);', body)))
        # Both the tiled path and legacy nine-quad fallback reuse these vertices.
        self.assertIn("GetGuiSlice9TileInfo(scene, node, org_width, org_height, &tile_info)", body)
        self.assertIn("rendered_vert_count += verts_per_slice9;", body)
        self.assertIn("rendered_vert_count += GetGuiSlice9TileVertexCount(tile_info);", body)
        self.assertIn("num_vertices += GetGuiSlice9TileVertexCount(tile_info);", cpp)
        self.assertIn("if (uv_rotated)", body)
        self.assertIn("v00.SetUV(src_y0, src_x0)", body)

    def test_test_content_stages_paged_material(self):
        cmake = CMAKE.read_text()
        self.assertIn('set(_GS_SLICE9_TILED_PAGED_MATERIAL "${GS_ENGINE_CONTENT_DIR}/builtins/materials/slice9_tiled_paged.material")', cmake)
        self.assertIn('"${_stage_root}/builtins/materials/slice9_tiled_paged.material"', cmake)
        self.assertRegex(cmake, r'DEPENDS [^\n]*"\$\{_GS_SLICE9_TILED_PAGED_MATERIAL\}"')

    @unittest.skipUnless(shutil.which("g++"), "g++ unavailable")
    def test_compiled_cpp_actual_tiler_eligibility_and_stretch_fallback(self):
        cpp = CPP.read_text()
        start = cpp.index("static bool IsTiledSlice9Material(")
        end = cpp.index("static void GetGuiSlice9TileSegment(", start)
        # Compile only the pure tiler helpers in this older source-extraction
        # test. CanTileGuiZeroMarginFrame has a TextureSet DDF dependency and
        # is source-compiled by test_worker77_repeat_runtime_native instead.
        frame_gate_start = cpp.index("static bool CanTileGuiZeroMarginFrame(", start, end)
        frame_gate_end = cpp.index("static bool GetGuiRepeatAxis(", frame_gate_start, end)
        actual_cpp = cpp[start:frame_gate_start] + cpp[frame_gate_end:end]
        header = r'''
#include <cmath>
#include <cfloat>
#include <cstdint>
#include <cstdio>
#include <cstring>
typedef uint64_t dmhash_t;
static dmhash_t dmHashString64(const char* str) {
    if (!std::strcmp(str, "slice9_tiled")) return 1;
    if (!std::strcmp(str, "slice9_tiled_paged")) return 2;
    return 3;
}
struct Vector4 {
    float x, y, z, w;
    float getX() const { return x; }
    float getY() const { return y; }
    float getZ() const { return z; }
    float getW() const { return w; }
};
static float sum(Vector4 v) { return v.x+v.y+v.z+v.w; }
struct Point3 {
    float x,y;
    float getX() const { return x; }
    float getY() const { return y; }
};
namespace dmMath {
    template<class T> T Min(T a,T b) { return a<b?a:b; }
    template<class T> T Max(T a,T b) { return a>b?a:b; }
}
static dmhash_t g_material = 1;
static float g_uv[6] = {0.25f,0.375f,0.25f,0.125f,0.5f,0.125f};
static const float* g_uvp = g_uv;
static Vector4 g_slice = {1,1,1,1};
static Point3 g_size = {5,5};
static Vector4 g_zoom = {1,1,0,0};
static Vector4 g_offset = {0,0,0,0};
static Vector4 g_mode = {0,0,0,0};
namespace dmGui {
    typedef int HScene;
    typedef int HNode;
    enum Property {PROPERTY_REPEAT_ZOOM=12,PROPERTY_REPEAT_OFFSET=13,PROPERTY_REPEAT_MODE=14};
    static void* GetNodeMaterial(HScene,HNode) { return (void*)1; }
    static dmhash_t GetNodeMaterialId(HScene,HNode) { return g_material; }
    static const float* GetNodeFlipbookAnimUV(HScene,HNode) { return g_uvp; }
    static Vector4 GetNodeSlice9(HScene,HNode) { return g_slice; }
    static Point3 GetNodeSize(HScene,HNode) { return g_size; }
    static Vector4 GetNodeProperty(HScene,HNode,Property p) {
        return p==PROPERTY_REPEAT_ZOOM ? g_zoom :
               (p==PROPERTY_REPEAT_OFFSET ? g_offset : g_mode);
    }
}
static void GetNodeFlipbookAnimUVFlip(dmGui::HScene,dmGui::HNode,bool& u,bool& v) {
    u=false;v=false;
}
'''
        main = r'''
int main() {
    GuiSlice9TileInfo info;
    // Nonrotated 4x4 source frame, 5x5 box and 1px margins on all sides.
    if (!GetGuiSlice9TileInfo(0,0,16,16,&info)) return 1;
    if (info.m_XCount!=2 || info.m_YCount!=2 || info.m_SourceCenterX!=2 || info.m_SourceCenterY!=2) return 2;
    if (GetGuiSlice9TileVertexCount(info) != 96) return 11;
    // Repeat tile phase helper is also present in this exact source extract.
    // Its default first segment must cover the original 2px center tile.
    float p0,p1,u0,u1;
    GetGuiRepeatTileSegment(0,3,info.m_TileStepX,info.m_FirstSpanX,false,&p0,&p1,&u0,&u1);
    if (p0!=0 || p1!=2 || u0!=0 || u1!=1) return 12;
    // Same frame packed rotated 90 degrees. Its atlas U/V axes swap.
    g_uv[2]=0.5f; g_uv[4]=0.5f; g_uv[3]=0.375f; g_uv[5]=0.125f;
    if (!GetGuiSlice9TileInfo(0,0,16,16,&info)) return 3;
    if (info.m_XCount!=2 || info.m_YCount!=2) return 4;
    // Paged opt-in must generate exactly the same tiled geometry.
    g_material=2;
    if (!GetGuiSlice9TileInfo(0,0,16,16,&info)) return 5;
    // Unknown material uses legacy nine-quad stretch.
    g_material=3;
    if (GetGuiSlice9TileInfo(0,0,16,16,&info)) return 6;
    g_material=2;
    // Degenerate source center and zero-length rendered center fall back.
    g_slice={2,1,2,1};
    if (GetGuiSlice9TileInfo(0,0,16,16,&info)) return 7;
    g_slice={1,1,1,1}; g_size={2,5};
    if (GetGuiSlice9TileInfo(0,0,16,16,&info)) return 8;
    // Pathological repetition above 2048 quads also falls back to stretch.
    g_size={5000,5000};
    if (GetGuiSlice9TileInfo(0,0,16,16,&info)) return 9;
    g_size={5,5}; g_uvp=nullptr;
    if (GetGuiSlice9TileInfo(0,0,16,16,&info)) return 10;
    std::puts("10 native C++ eligibility/fallback checks passed");
    return 0;
}
'''
        with tempfile.TemporaryDirectory(prefix="defold-worker86-") as work:
            source_file, binary = Path(work) / "eligibility.cpp", Path(work) / "eligibility"
            source_file.write_text(header + actual_cpp + main)
            compile_ = subprocess.run(["g++", "-std=c++11", "-Wall", "-Wextra", "-Werror",
                                       str(source_file), "-o", str(binary)], capture_output=True, text=True)
            self.assertEqual(compile_.returncode, 0, compile_.stderr)
            test = subprocess.run([str(binary)], capture_output=True, text=True)
            self.assertEqual(test.returncode, 0, test.stdout + test.stderr)
            self.assertIn("10 native C++ eligibility/fallback checks passed", test.stdout)

    @unittest.skipUnless(shutil.which("g++"), "g++ unavailable")
    def test_compiled_cpp_helper_partial_tiles_rotated_axis_and_flips(self):
        cpp = CPP.read_text()
        start = cpp.index("static void GetGuiSlice9TileSegment(")
        end = cpp.index("static uint32_t CalcVertexCount(", start)
        helper = cpp[start:end]
        source = r'''
#include <cmath>
#include <cstdint>
#include <cstdio>
namespace dmMath {
inline float Min(float a, float b) { return a < b ? a : b; }
inline float Max(float a, float b) { return a > b ? a : b; }
}
''' + helper + r'''
static bool segment(uint32_t i, uint32_t count, float destination,
                    float source, bool flip, float a, float b, float u, float v) {
    float p0, p1, t0, t1;
    GetGuiSlice9TileSegment(i, count, destination, source, flip, &p0, &p1, &t0, &t1);
    if (std::fabs(p0-a)>1e-6f || std::fabs(p1-b)>1e-6f ||
        std::fabs(t0-u)>1e-6f || std::fabs(t1-v)>1e-6f) {
        std::printf("mismatch i=%u flip=%d actual=(%g,%g,%g,%g)\n", i, flip, p0,p1,t0,t1);
        return false;
    }
    return true;
}
int main() {
    // Source center 2px, destination center 3px: one clipped 1px repeat.
    // Same helper is used for both atlas axes; rotated UV placement swaps
    // the axes after this calculation in RenderBoxNodes.
    for (int axis=0; axis<2; ++axis) {
        if (!segment(0,2,3,2,false,0,2,0,1)) return 1;
        if (!segment(1,2,3,2,false,2,3,0,0.5f)) return 2;
        if (!segment(0,2,3,2,true,0,1,0.5f,0)) return 3;
        if (!segment(1,2,3,2,true,1,3,1,0)) return 4;
    }
    // Too-small/zero-center node uses legacy stretch in GetGuiSlice9TileInfo.
    std::puts("8 C++ partial-tile/flip checks passed");
    return 0;
}
'''
        with tempfile.TemporaryDirectory(prefix="defold-worker86-") as work:
            source_file, binary = Path(work) / "segment.cpp", Path(work) / "segment"
            source_file.write_text(source)
            compile_ = subprocess.run(["g++", "-std=c++11", "-Wall", "-Wextra", "-Werror",
                                       str(source_file), "-o", str(binary)], capture_output=True, text=True)
            self.assertEqual(compile_.returncode, 0, compile_.stderr)
            test = subprocess.run([str(binary)], capture_output=True, text=True)
            self.assertEqual(test.returncode, 0, test.stdout + test.stderr)
            self.assertIn("8 C++ partial-tile/flip checks passed", test.stdout)


if __name__ == "__main__":
    unittest.main()
