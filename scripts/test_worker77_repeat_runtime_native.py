"""Exact extracted production C++ regression for GUI Repeat zoom/offset on fa74298.

This compiles the renderer helper source without the entire Defold/GL SDK. It
checks allocation counts, phase clamping, flips, rotation, safe fallbacks.
Run: python3 scripts/test_worker77_repeat_runtime_native.py
"""
from pathlib import Path
import subprocess, tempfile, shutil, unittest
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/"engine/gamesys/src/gamesys/components/comp_gui.cpp"
GUI_HEADER=ROOT/"engine/gui/src/dmsdk/gui/gui.h"
GUI_SOURCE=ROOT/"engine/gui/src/gui.cpp"
LUA=ROOT/"engine/gui/src/gui_script.cpp"
DDF=ROOT/"engine/gamesys/proto/gamesys/gui_ddf.proto"
STUB=r"""
#include <cassert>
#include <cmath>
#include <cfloat>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
typedef uint64_t dmhash_t;
struct Vector4{float x,y,z,w; float getX()const{return x;}float getY()const{return y;}float getZ()const{return z;}float getW()const{return w;}};
float sum(const Vector4& v){return v.x+v.y+v.z+v.w;}
struct Point3{float x,y,z;float getX()const{return x;}float getY()const{return y;}};
namespace dmMath{template<class T>T Min(T a,T b){return a<b?a:b;}template<class T>T Max(T a,T b){return a>b?a:b;}}
namespace dmGameSystemDDF{
enum SpriteTrimmingMode{SPRITE_TRIM_MODE_OFF=0,SPRITE_TRIM_MODE_4=1};
template<class T>struct Array{T* m_Data;uint32_t m_Count;
const T& operator[](uint32_t i)const{return m_Data[i];}};
struct SpriteGeometry{SpriteTrimmingMode m_TrimMode;float m_PivotX,m_PivotY;};
struct TextureSet{Array<SpriteGeometry> m_Geometries;Array<uint32_t> m_FrameIndices;
uint32_t m_UseGeometries;};
}
uint64_t dmHashString64(const char* s){if(!strcmp(s,"slice9_tiled"))return 44;if(!strcmp(s,"slice9_tiled_paged"))return 45;return 0;}
struct Fixture{bool material=true;bool uv=true;bool flip_u=false;bool flip_v=false;uint64_t id=44;
float tc[6]={0.1f,0.2f,0.1f,0.4f,0.3f,0.4f};
Vector4 margins={0,0,0,0};Point3 size={45,34,0};
Vector4 zoom={1,1,0,0},offset={0,0,0,0},mode={0,0,0,0};};
namespace dmGui{using HScene=void*;using HNode=Fixture*;
enum Property{PROPERTY_REPEAT_ZOOM=12,PROPERTY_REPEAT_OFFSET=13,PROPERTY_REPEAT_MODE=14};
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
static void check_segments(float dst,float tile,float phase,bool flipped,uint32_t expected){
float first;uint32_t count;float step;
assert(GetGuiRepeatAxis(dst,tile,1.0f,phase,flipped,&step,&first,&count));
assert(count==expected);float previous=0;
for(uint32_t i=0;i<count;i++){float p0,p1,u0,u1;
GetGuiRepeatTileSegment(i,dst,step,first,flipped,&p0,&p1,&u0,&u1);
assert(eq(p0,previous)&&p1>p0&&p1<=dst+0.0001f);
assert(u0>=-0.0001f&&u0<=1.0001f&&u1>=-0.0001f&&u1<=1.0001f);
assert(flipped ? u0>=u1 : u1>=u0);previous=p1;
}assert(eq(previous,dst));}
int main(){Fixture n;GuiSlice9TileInfo info={};
float p0,p1,t0,t1;
GetGuiSlice9TileSegment(0,1,18,20,false,&p0,&p1,&t0,&t1);assert(eq(p0,0)&&eq(p1,18));
// Source/phase float rounding must never reverse the first mirrored tile.
float tiny_step,tiny_first;uint32_t tiny_count;
assert(GetGuiRepeatAxis(1.01f,0.00100001f,1.0f,0.0f,true,&tiny_step,&tiny_first,&tiny_count));
assert(tiny_count==1010);
GetGuiSlice9TileSegment(0,tiny_count,1.01f,0.00100001f,true,&p0,&p1,&t0,&t1);
assert(p0==0.0f&&p1>p0&&t0>=0.0f&&t0<=1.0f&&t1==0.0f);
assert(GetGuiRepeatAxis(1.01f,0.00100001f,1.0f,0.0f,false,&tiny_step,&tiny_first,&tiny_count));
assert(tiny_count==1010);
GetGuiSlice9TileSegment(18,tiny_count,1.01f,0.00100001f,false,&p0,&p1,&t0,&t1);
assert(p1>p0&&t0>=0.0f&&t1<=1.0f);
assert(GetGuiSlice9TileInfo(nullptr,&n,100,100,&info));
assert(info.m_NoMargins&&info.m_XCount==3&&info.m_YCount==2);
assert(GetGuiSlice9TileVertexCount(info)==36);
assert(!info.m_AnimatedPhase);
n.zoom={2,2,0,0};
assert(GetGuiSlice9TileInfo(nullptr,&n,100,100,&info));
assert(info.m_XCount==2&&info.m_YCount==1&&GetGuiSlice9TileVertexCount(info)==12);
assert(eq(info.m_TileStepX,40)&&eq(info.m_TileStepY,40));
assert(info.m_AnimatedPhase);
n.offset={38,-5,0,0};assert(GetGuiSlice9TileInfo(nullptr,&n,100,100,&info));
assert(info.m_XCount==3&&info.m_YCount==2);
check_segments(45,40,38,false,3);check_segments(34,40,-5,false,2);
check_segments(45,40,0,false,2);check_segments(45,40,0,true,2);
check_segments(45,40,17,true,2);check_segments(15,40,-37,true,1);
check_segments(85,40,17,false,3);check_segments(85,40,-17,false,3);
n.flip_u=true;n.flip_v=true;
assert(GetGuiSlice9TileInfo(nullptr,&n,100,100,&info));
assert(info.m_XCount>=1&&info.m_YCount>=1);
for(uint32_t i=0;i<info.m_XCount;i++){float p0,p1,u0,u1;
GetGuiRepeatTileSegment(i,info.m_RenderCenterX,info.m_TileStepX,info.m_FirstSpanX,true,&p0,&p1,&u0,&u1);
assert(u0>=u1&&u1>=-0.0001f);}
n.margins={3,2,5,4};assert(GetGuiSlice9TileInfo(nullptr,&n,100,100,&info));
assert(!info.m_NoMargins&&GetGuiSlice9TileVertexCount(info)==(info.m_XCount+2)*(info.m_YCount+2)*6);
n.margins={0,0,0,0};n.zoom={1,1,0,0};n.offset={0,0,0,0};
n.tc[0]=0.1f;n.tc[1]=0.2f;n.tc[2]=0.3f;n.tc[3]=0.2f;n.tc[4]=0.3f;n.tc[5]=0.5f;
assert(GetGuiSlice9TileInfo(nullptr,&n,100,100,&info));
assert(info.m_NoMargins&&eq(info.m_SourceCenterX,30)&&eq(info.m_SourceCenterY,20));
n.tc[0]=0.1f;n.tc[1]=0.2f;n.tc[2]=0.1f;n.tc[3]=0.4f;n.tc[4]=0.3f;n.tc[5]=0.4f;
n.material=false;assert(!GetGuiSlice9TileInfo(nullptr,&n,100,100,&info));
n.mode={1,0,0,0};assert(GetGuiSlice9TileInfo(nullptr,&n,100,100,&info));
n.mode={0,0,0,0};n.material=true;
n.zoom={0,1,0,0};assert(!GetGuiSlice9TileInfo(nullptr,&n,100,100,&info));
n.zoom={1,1,0,0};n.offset={2000000,0,0,0};assert(!GetGuiSlice9TileInfo(nullptr,&n,100,100,&info));
n.offset={0,0,0,0};n.size={100000,100000,0};assert(!GetGuiSlice9TileInfo(nullptr,&n,100,100,&info));
n.size={0,34,0};assert(!GetGuiSlice9TileInfo(nullptr,&n,100,100,&info));
n.size={45,34,0};n.uv=false;assert(GetGuiSlice9TileInfo(nullptr,&n,100,100,&info));
// Exact serialized GUI scene regression: ordinary material and a 32x32 frame
// inside a 64x64 atlas, with 2px Slice9 borders, 64x48 box, zoom (2,1.5),
// and offset (8,-4). New Repeat mode must not require slice9_tiled material.
n.uv=true;n.material=false;n.mode={1,0,0,0};n.flip_u=false;n.flip_v=false;
n.tc[0]=0;n.tc[1]=0.5f;n.tc[2]=0;n.tc[3]=1;n.tc[4]=0.5f;n.tc[5]=1;
n.margins={2,2,2,2};n.size={64,48,0};n.zoom={2,1.5f,0,0};n.offset={8,-4,0,0};
assert(GetGuiSlice9TileInfo(nullptr,&n,64,64,&info));
assert(info.m_XCount==2&&info.m_YCount==2);
assert(GetGuiSlice9TileVertexCount(info)==96);
GetGuiRepeatTileSegment(0,info.m_RenderCenterX,info.m_TileStepX,info.m_FirstSpanX,false,&p0,&p1,&t0,&t1);
assert(eq(p0,0)&&eq(p1,48)&&eq(t0,8.0f/56.0f)&&eq(t1,1));
// Regression: Bob emits an untrimmed SpriteGeometry even when use_geometries
// is zero. That static frame must tile despite having geometry array entries.
dmGameSystemDDF::SpriteGeometry frames[]={
 {dmGameSystemDDF::SPRITE_TRIM_MODE_OFF,0,0},
 {dmGameSystemDDF::SPRITE_TRIM_MODE_4,0,0},
 {dmGameSystemDDF::SPRITE_TRIM_MODE_OFF,0.25f,0},
 {dmGameSystemDDF::SPRITE_TRIM_MODE_OFF,0,-0.5f}};
uint32_t frame_indices[]={0,1,2,3,0,99};
dmGameSystemDDF::TextureSet static_set={{frames,4},{frame_indices,6},0};
assert(static_set.m_Geometries.m_Count>0);
assert(CanTileGuiZeroMarginFrame(&static_set,0));
static_set.m_UseGeometries=1;
assert(CanTileGuiZeroMarginFrame(&static_set,4)); // Modern Bob can set flag 1 too.
assert(!CanTileGuiZeroMarginFrame(&static_set,1)); // Active polygon trim.
assert(!CanTileGuiZeroMarginFrame(&static_set,2)); // Horizontal pivot offset.
assert(!CanTileGuiZeroMarginFrame(&static_set,3)); // Vertical pivot offset.
assert(!CanTileGuiZeroMarginFrame(&static_set,5)); // Bad source frame index.
assert(!CanTileGuiZeroMarginFrame(&static_set,6)); // Bad animation frame index.
static_set.m_Geometries.m_Count=0;
assert(CanTileGuiZeroMarginFrame(&static_set,0)); // Legacy no-geometry texture set.
assert(CanTileGuiZeroMarginFrame(nullptr,0)); // Direct manual texture.
printf("PASS exact production C++ Repeat zoom offset size flip clipping rotation caps manual texture and material fallback\\n");
return 0;
}
"""
class RepeatNative(unittest.TestCase):
 def test_exact_cpp_helper(self):
  cxx=shutil.which("g++") or shutil.which("clang++")
  if not cxx:self.skipTest("No native compiler")
  src=SOURCE.read_text()
  a=src.index("    static bool IsTiledSlice9Material(");b=src.index("    static uint32_t CalcVertexCount(",a)
  with tempfile.TemporaryDirectory(prefix="worker77_repeat_native_") as td:
   cpp=Path(td)/"repeat.cpp";exe=Path(td)/"repeat"
   cpp.write_text(STUB+"\n"+src[a:b]+"\n"+MAIN)
   p=subprocess.run([cxx,"-std=c++11","-O1","-Wall","-Wextra","-Werror","-o",str(exe),str(cpp)],capture_output=True,text=True)
   self.assertEqual(p.returncode,0,p.stderr[:5000])
   q=subprocess.run([str(exe)],capture_output=True,text=True)
   self.assertEqual(q.returncode,0,(q.stderr+q.stdout)[:5000])
   self.assertIn("PASS exact production C++ Repeat",q.stdout)
 def test_property_and_emitter_source_contract(self):
  gui=GUI_HEADER.read_text();src=GUI_SOURCE.read_text();lua=LUA.read_text();render=SOURCE.read_text();proto=DDF.read_text()
  for symbol in ["PROPERTY_REPEAT_ZOOM","PROPERTY_REPEAT_OFFSET","PROPERTY_REPEAT_MODE"]:
   self.assertIn(symbol,gui);self.assertIn(symbol,src);self.assertIn(symbol,lua);self.assertIn(symbol,render)
  for identifier in ["LuaSetRepeatMode","LuaGetRepeatMode","LuaSetRepeatZoom","LuaGetRepeatZoom","LuaSetRepeatOffset","LuaGetRepeatOffset","SETPROP(repeat_zoom, REPEAT_ZOOM)","SETPROP(repeat_offset, REPEAT_OFFSET)"]:
   self.assertIn(identifier,lua)
  for name in ["PROP(repeat_zoom, PROPERTY_REPEAT_ZOOM )","PROP(repeat_offset, PROPERTY_REPEAT_OFFSET )"]:
   self.assertIn(name,src)
  for fragment in ["GetGuiRepeatAxis(", "GetGuiRepeatTileSegment(", "GetGuiSlice9TileVertexCount(tile_info)"]:
   self.assertIn(fragment,render)
  self.assertGreaterEqual(render.count("GetGuiSlice9TileVertexCount(tile_info)"),3)
  self.assertIn("GuiSlice9TileInfo tile_info",render)
  for field in ["optional TextureMode texture_mode     = 52", "optional dmMath.Vector4 repeat_zoom    = 53", "optional dmMath.Vector4 repeat_offset  = 54"]:
   self.assertIn(field,proto)
  for member in ["node_desc->m_TextureMode","node_desc->m_RepeatZoom","node_desc->m_RepeatOffset"]:
   self.assertIn(member,render)
  self.assertIn("gui.PROP_SIZE",lua)
  self.assertIn("PROPERTY_REPEAT_ZOOM] = Vector4(1,1,0,0)",src)
  self.assertIn("PROPERTY_REPEAT_OFFSET] = Vector4(0,0,0,0)",src)
if __name__=="__main__":unittest.main(verbosity=2)
