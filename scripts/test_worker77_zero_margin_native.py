"""Worker77 isolated zero-margin GUI material repeat native C++ helper regression.

Compiles exact production C++ helpers extracted from comp_gui.cpp. This does not
compile/link the full engine renderer or exercise GPU output. Original source
also checked for allocator/emitter budget parity and zero-margin opt-in routing.
Run: python3 scripts/test_worker77_zero_margin_native.py
"""
import pathlib, subprocess, tempfile, shutil, re, unittest

ROOT=pathlib.Path(__file__).resolve().parents[1]
SOURCE=ROOT/"engine/gamesys/src/gamesys/components/comp_gui.cpp"
STUB=r"""
#include <cassert>
#include <cmath>
#include <cfloat>
#include <cstdint>
#include <cstdio>
#include <cstring>
typedef uint64_t dmhash_t;
struct Vector4 {float x,y,z,w;
float getX() const{return x;} float getY() const{return y;}
float getZ() const{return z;} float getW() const{return w;}};
float sum(const Vector4& v){return v.x+v.y+v.z+v.w;}
struct Point3 {float x,y,z;float getX()const{return x;}float getY()const{return y;}};
namespace dmMath {template<class T>T Min(T a,T b){return a<b?a:b;}template<class T>T Max(T a,T b){return a>b?a:b;}}
uint64_t dmHashString64(const char* x){return strcmp(x,"slice9_tiled")==0?44:0;}
struct Fixture{bool material=true;bool uv=true;bool flip_u=false;bool flip_v=false;uint64_t id=44;
float tc[6]={0.1f,0.2f,0.1f,0.4f,0.3f,0.4f};
Vector4 margins={0,0,0,0};Point3 size={45,34,0};
Vector4 zoom={1,1,0,0},offset={0,0,0,0},mode={0,0,0,0};};
namespace dmGui {
using HScene=void*;using HNode=Fixture*;
enum Property {PROPERTY_REPEAT_ZOOM=12,PROPERTY_REPEAT_OFFSET=13,PROPERTY_REPEAT_MODE=14};
void* GetNodeMaterial(HScene,HNode n){return n->material?n:nullptr;}
uint64_t GetNodeMaterialId(HScene,HNode n){return n->id;}
const float* GetNodeFlipbookAnimUV(HScene,HNode n){return n->uv?n->tc:nullptr;}
Vector4 GetNodeSlice9(HScene,HNode n){return n->margins;}
Point3 GetNodeSize(HScene,HNode n){return n->size;}
Vector4 GetNodeProperty(HScene,HNode n,Property p){return p==PROPERTY_REPEAT_ZOOM?n->zoom:(p==PROPERTY_REPEAT_OFFSET?n->offset:n->mode);}
}
void GetNodeFlipbookAnimUVFlip(dmGui::HScene,dmGui::HNode n,bool& u,bool& v){u=n->flip_u;v=n->flip_v;}
"""
MAIN=r"""
static bool eq(float a,float b){return fabsf(a-b)<0.0001f;}
int main(){
Fixture n; GuiSlice9TileInfo t={};
// Atlas frame 20x20, target 45x34: ceil = 3x2 = 36 vertices, not 6 or 150.
assert(GetGuiSlice9TileInfo(nullptr,&n,100,100,&t));
assert(t.m_NoMargins);assert(t.m_XCount==3&&t.m_YCount==2);
assert(eq(t.m_SourceCenterX,20)&&eq(t.m_SourceCenterY,20));
assert(eq(t.m_RenderCenterX,45)&&eq(t.m_RenderCenterY,34));
assert(GetGuiSlice9TileVertexCount(t)==36);
// The new phase helper is extracted with the original helper under -Werror.
// At default offset and zoom it must agree with an ordinary full first tile.
float pp0,pp1,uu0,uu1;
GetGuiRepeatTileSegment(0,t.m_RenderCenterX,t.m_TileStepX,t.m_FirstSpanX,false,&pp0,&pp1,&uu0,&uu1);
assert(eq(pp0,0)&&eq(pp1,20)&&eq(uu0,0)&&eq(uu1,1));
float last=0; for(uint32_t i=0;i<t.m_XCount;i++){
float p0,p1,u0,u1;GetGuiSlice9TileSegment(i,t.m_XCount,t.m_RenderCenterX,t.m_SourceCenterX,false,&p0,&p1,&u0,&u1);
assert(eq(p0,last)&&p1>p0&&p1<=45&&u0==0&&u1>0&&u1<=1);last=p1;
} assert(eq(last,45));
last=0;for(uint32_t i=0;i<t.m_XCount;i++){
float p0,p1,u0,u1;GetGuiSlice9TileSegment(i,t.m_XCount,t.m_RenderCenterX,t.m_SourceCenterX,true,&p0,&p1,&u0,&u1);
assert(eq(p0,last)&&p1>p0&&p1<=45&&u0>0&&u0<=1&&u1==0);last=p1;
}assert(eq(last,45));
// Rotated frame, 30x20 source extent, fallback and vertex budget.
n.tc[0]=0.1f;n.tc[1]=0.2f;n.tc[2]=0.3f;n.tc[3]=0.2f;n.tc[4]=0.3f;n.tc[5]=0.5f;
assert(GetGuiSlice9TileInfo(nullptr,&n,100,100,&t));
assert(t.m_NoMargins&&eq(t.m_SourceCenterX,30)&&eq(t.m_SourceCenterY,20));
n.margins={3,2,5,4};assert(GetGuiSlice9TileInfo(nullptr,&n,100,100,&t));
assert(!t.m_NoMargins&&GetGuiSlice9TileVertexCount(t)==(t.m_XCount+2)*(t.m_YCount+2)*6);
n.margins={0,0,0,0};
n.material=false;assert(!GetGuiSlice9TileInfo(nullptr,&n,100,100,&t));n.material=true;
n.id=123;assert(!GetGuiSlice9TileInfo(nullptr,&n,100,100,&t));n.id=44;
n.uv=false;assert(GetGuiSlice9TileInfo(nullptr,&n,100,100,&t));
assert(t.m_NoMargins&&GetGuiSlice9TileVertexCount(t)==6);
n.margins={1,1,1,1};assert(!GetGuiSlice9TileInfo(nullptr,&n,100,100,&t));
n.margins={0,0,0,0};n.size={250,150,0};
assert(GetGuiSlice9TileInfo(nullptr,&n,100,100,&t));
assert(t.m_XCount==3&&t.m_YCount==2&&GetGuiSlice9TileVertexCount(t)==36);
n.uv=true;n.size={45,34,0};
n.size={0,34,0};assert(!GetGuiSlice9TileInfo(nullptr,&n,100,100,&t));
n.size={45,34,0};assert(!GetGuiSlice9TileInfo(nullptr,&n,0,100,&t));
n.size={100000,100000,0};assert(!GetGuiSlice9TileInfo(nullptr,&n,100,100,&t));
n.size={45,34,0};assert(GetGuiSlice9TileInfo(nullptr,&n,100,100,&t));
puts("PASS native exact extracted helper: zero margins, UV clipping, flip, rotation, legacy margins, fallbacks and cap");
}
"""
class ZeroMarginNative(unittest.TestCase):
 def test_exact_native_cpp_before_after(self):
  cxx=shutil.which("g++") or shutil.which("clang++")
  if not cxx:self.skipTest("No native compiler available")
  source=SOURCE.read_text()
  start=source.index("    static bool IsTiledSlice9Material(")
  end=source.index("    static uint32_t CalcVertexCount(",start)
  # This legacy harness exercises the pure Slice-9 helper functions. The
  # separate static-frame regression compiles CanTileGuiZeroMarginFrame with
  # its TextureSet DDF fixture; exclude that unrelated dependency here.
  frame_gate_start=source.index("    static bool CanTileGuiZeroMarginFrame(",start,end)
  frame_gate_end=source.index("    static bool GetGuiRepeatAxis(",frame_gate_start,end)
  helpers=source[start:frame_gate_start]+source[frame_gate_end:end]
  with tempfile.TemporaryDirectory(prefix="worker77_native_") as d:
   p=pathlib.Path(d)/"native.cpp";exe=pathlib.Path(d)/"native"
   p.write_text(STUB+"\n"+helpers+"\n"+MAIN)
   a=subprocess.run([cxx,"-std=c++11","-O1","-Wall","-Wextra","-Werror","-o",str(exe),str(p)],text=True,capture_output=True)
   self.assertEqual(0,a.returncode,a.stderr)
   b=subprocess.run([str(exe)],capture_output=True,text=True)
   self.assertEqual(0,b.returncode,b.stdout+b.stderr)
   self.assertIn("PASS native exact extracted helper",b.stdout)
 def test_budget_and_entrypoints_production(self):
  source=SOURCE.read_text()
  self.assertIn("GetGuiSlice9TileVertexCount(tile_info)",source)
  self.assertIn("if ((!use_slice_nine && manually_set_texture && !zero_margin_tiled) || !texture)",source)
  self.assertIn("if (!use_slice_nine && use_geometries && !zero_margin_tiled)",source)
  self.assertIn("if (zero_margin_tiled || GetGuiSlice9TileInfo(",source)
  self.assertIn("for (int y = zero_margin_tiled ? 1 : 0; y < (zero_margin_tiled ? 2 : 3); ++y)",source)
  self.assertIn("for (int x = zero_margin_tiled ? 1 : 0; x < (zero_margin_tiled ? 2 : 3); ++x)",source)
  self.assertGreaterEqual(source.count("GetGuiSlice9TileVertexCount(tile_info)"),3)
if __name__=="__main__":unittest.main(verbosity=2)
