# Copyright 2020-2026 The Defold Foundation
# Copyright 2014-2020 King
# Copyright 2009-2014 Ragnar Svensson, Christian Murray
# Licensed under the Defold License version 1.0 (the "License"); you may not use
# this file except in compliance with the License.
#
# You may obtain a copy of the License, together with FAQs at
# https://www.defold.com/license
#
# Unless required by applicable law or agreed to in writing, software distributed
# under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied. See the License for the
# specific language governing permissions and limitations under the License.

"""Compile the real GUI property hash lookup and check the Repeat zoom range."""

from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
GUI_CPP = ROOT / "engine/gui/src/gui.cpp"
SCRIPT_CPP = ROOT / "engine/gui/src/gui_script.cpp"
RENDER_CPP = ROOT / "engine/gamesys/src/gamesys/components/comp_gui.cpp"


class RepeatPropertyContract(unittest.TestCase):
    # Regression: new enum members must resolve native hashes without indexing
    # an unrelated enum entry or accidentally making Repeat mode animatable.
    def test_native_enum_hash_lookup(self):
        cxx = shutil.which("g++") or shutil.which("clang++")
        if not cxx:
            self.skipTest("No C++ compiler installed")

        source = GUI_CPP.read_text()
        table_begin = source.index("    PropDesc g_PropTable[] = {")
        table_end = source.index("    PropDesc* GetPropertyDesc(", table_begin)
        func_begin = source.index("    dmhash_t GetPropertyHash(Property property)")
        func_end = source.index("    void CancelAnimationHash(", func_begin)
        fixture = r"""
#include <cassert>
#include <cstdint>
#include <cstring>
typedef uint64_t dmhash_t;
enum Property {
    PROPERTY_POSITION=0, PROPERTY_ROTATION=1, PROPERTY_SCALE=2,
    PROPERTY_COLOR=3, PROPERTY_SIZE=4, PROPERTY_OUTLINE=5,
    PROPERTY_SHADOW=6, PROPERTY_SLICE9=7, PROPERTY_PIE_PARAMS=8,
    PROPERTY_TEXT_PARAMS=9, PROPERTY_EULER=10, PROPERTY_PREV_EULER=11,
    PROPERTY_REPEAT_ZOOM=12, PROPERTY_REPEAT_OFFSET=13,
    PROPERTY_REPEAT_MODE=14, PROPERTY_COUNT=15
};
struct PropDesc { dmhash_t m_Hash; Property m_Property; uint8_t m_Component; };
dmhash_t dmHashString64(const char* name) {
    dmhash_t result = 14695981039346656037ULL;
    for (const unsigned char* ptr = (const unsigned char*)name; *ptr; ++ptr)
        result = (result ^ *ptr) * 1099511628211ULL;
    return result;
}
"""
        tests = r"""
int main() {
    assert(GetPropertyHash(PROPERTY_POSITION)==dmHashString64("position"));
    assert(GetPropertyHash(PROPERTY_SHADOW)==dmHashString64("shadow"));
    assert(GetPropertyHash(PROPERTY_SLICE9)==dmHashString64("slice"));
    assert(GetPropertyHash(PROPERTY_EULER)==dmHashString64("euler"));
    assert(GetPropertyHash(PROPERTY_REPEAT_ZOOM)==dmHashString64("repeat_zoom"));
    assert(GetPropertyHash(PROPERTY_REPEAT_OFFSET)==dmHashString64("repeat_offset"));
    assert(GetPropertyHash(PROPERTY_PIE_PARAMS)==0);
    assert(GetPropertyHash(PROPERTY_TEXT_PARAMS)==0);
    assert(GetPropertyHash(PROPERTY_PREV_EULER)==0);
    assert(GetPropertyHash(PROPERTY_REPEAT_MODE)==0);
    assert(GetPropertyHash(PROPERTY_COUNT)==0);
    return 0;
}
"""
        with tempfile.TemporaryDirectory(prefix="worker92_repeat_hash_") as directory:
            cpp = Path(directory) / "hash.cpp"
            exe = Path(directory) / "hash"
            cpp.write_text(fixture + source[table_begin:table_end] + source[func_begin:func_end] + tests)
            compiled = subprocess.run(
                [cxx, "-std=c++11", "-Wall", "-Wextra", "-Werror", "-o", str(exe), str(cpp)],
                capture_output=True, text=True,
            )
            self.assertEqual(compiled.returncode, 0, compiled.stderr)
            executed = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(executed.returncode, 0, executed.stderr)

    # Regression: the Lua setter must reject zoom 0.005 because the actual
    # renderer only supports a repeated tile size at zoom >= 0.01.
    def test_lua_and_renderer_zoom_domains_match(self):
        lua = SCRIPT_CPP.read_text()
        renderer = RENDER_CPP.read_text()
        setter = lua[lua.index("    static int LuaSetRepeatZoom("):lua.index("    static int LuaGetRepeatZoom(")]
        helper = renderer[renderer.index("    static bool GetGuiRepeatAxis("):
                          renderer.index("    static void GetGuiRepeatTileSegment(")]
        self.assertIn("zoom.getX() >= 0.01f && zoom.getY() >= 0.01f", setter)
        self.assertIn("zoom.getX() <= 256.0f && zoom.getY() <= 256.0f", setter)
        self.assertIn("zoom >= 0.01f && zoom <= 256.0f", helper)


if __name__ == "__main__":
    unittest.main(verbosity=2)
