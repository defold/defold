// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#ifndef DM_TEST_GAMESYS_PRIVATE_H
#define DM_TEST_GAMESYS_PRIVATE_H

#include "test_gamesys.h"
#include "../gamesys_private.h"

#include "../../../../graphics/src/graphics_private.h"
#include "../../../../graphics/src/null/graphics_null_private.h"
#include "../../../../particle/src/particle_private.h"
#include "../../../../render/src/render/render_private.h"
#include "../../../../render/src/render/font/fontmap_private.h"
#include "../../../../resource/src/resource_private.h"
#include "../../../../gui/src/gui_private.h"

#include <render/font/fontmap.h>
#include <platform/window.hpp>

#include "gamesys/resources/res_compute.h"
#include "gamesys/resources/res_font.h"
#include "gamesys/resources/res_font_private.h"
#include "gamesys/resources/res_glyph_bank.h"
#include "gamesys/resources/res_label.h"
#include "gamesys/resources/res_material.h"
#include "gamesys/resources/res_render_target.h"
#include "gamesys/resources/res_ttf.h"
#include "gamesys/resources/res_textureset.h"

#include <stdio.h>
#include <string.h>

#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/memory.h>
#include <dlib/time.h>
#include <dlib/path.h>
#include <dlib/sys.h>
#include <dlib/testutil.h>
#include <dlib/utf8.h>
#include <testmain/testmain.h>

#include <font/fontcollection.h>
#include <font/text_layout.h>

#include <ddf/ddf.h>
#include <gameobject/gameobject.h>
#include <gameobject/gameobject_ddf.h>
#include <gameobject/lua_ddf.h>
#include <gameobject/script.h>
#include <gameobject/gameobject_props.h>

#include <gamesys/gamesys_ddf.h>
#include <gamesys/label_ddf.h>
#include <gamesys/sprite_ddf.h>
#include "../components/comp_label.h"
#include "../components/comp_model.h"
#include "../components/comp_private.h"
#include "../components/comp_collection_proxy.h"
#include "../scripts/script_sys_gamesys.h"
#include "../scripts/script_resource.h"

#include "resource/layer_guitar_a.ogg.embed.h" // LAYER_GUITAR_A_OGG / LAYER_GUITAR_A_OGG_SIZE
#include "resource/booster_on_sfx.wav.embed.h" // BOOSTER_ON_SFX_WAV / BOOSTER_ON_SFX_WAV_SIZE

#include <dmsdk/gamesys/render_constants.h>
#include <dmsdk/gamesys/components/comp_gui.h>
#include <dmsdk/gamesys/resources/res_data.h>
#include <dmsdk/gamesys/resources/res_light.h>
#include <dmsdk/gamesys/resources/res_model.h>

#include <sound/sound.h>

static inline float ReadUnalignedFloat(const void* ptr)
{
    float value;
    memcpy(&value, ptr, sizeof(value));
    return value;
}

static inline int16_t ReadUnalignedInt16(const void* ptr)
{
    int16_t value;
    memcpy(&value, ptr, sizeof(value));
    return value;
}

namespace dmGameObject
{
    HCollection GetCollectionByHash(HContext regist, dmhash_t socket_name);
}

namespace dmGameSystem
{
    dmGameObject::Result CompCollectionProxyUnloadAsync(HCollectionProxyWorld world, HCollectionProxyComponent proxy, ProxyLoadCallback cbk, void* cbk_ctx);
}

namespace dmGameSystem
{
    void DumpResourceRefs(dmGameObject::HCollection collection);
    extern void GetSpriteWorldRenderBuffers(void* world, dmRender::HBufferedRenderBuffer* vx_buffer, dmRender::HBufferedRenderBuffer* ix_buffer);
    extern uint32_t GetSpriteWorldVertexBufferCapacity(void* sprite_world);
    extern void GetSpriteWorldDynamicAttributePool(void* sprite_world, DynamicAttributePool** pool_out);
    extern void GetSpriteComponentScale(void* sprite_component, dmVMath::Vector3* scale_out);
    extern uint16_t GetSpriteComponentAnimationIndex(void* sprite_component);
    extern void GetModelWorldRenderBuffers(void* world, dmRender::HBufferedRenderBuffer** vx_buffers, uint32_t* vx_buffers_count);
    extern void GetModelWorldInstanceRenderBuffer(void* model_world, dmRender::HBufferedRenderBuffer* instance_buffer);
    extern void GetModelWorldRenderBatchStats(void* model_world, uint8_t* world_batch_count, uint8_t* local_batch_count, uint8_t* local_instanced_batch_count);
    extern void GetModelWorldScratchConstantBuffers(void* model_world, dmGameSystem::HComponentRenderConstants** constant_buffers, uint32_t* count);
    extern void GetModelComponentRenderConstants(void* model_component, int render_item_ix, dmGameSystem::HComponentRenderConstants* render_constants);
    extern void GetModelComponentAttributeRenderData(void* model_component, int render_item_ix, dmGraphics::HVertexBuffer* vx_buffer, dmGraphics::HVertexDeclaration* vx_decl, dmGraphics::HVertexDeclaration* inst_decl);
    extern void GetParticleFXWorldRenderBuffers(void* world, dmRender::HBufferedRenderBuffer* vx_buffer);
    extern void GetTileGridWorldRenderBuffers(void* world, dmRender::HBufferedRenderBuffer* vx_buffer);
}

#define EPSILON 0.0001f
#define ASSERT_VEC4(exp, act)\
    ASSERT_NEAR(exp.getX(), act.getX(), EPSILON);\
    ASSERT_NEAR(exp.getY(), act.getY(), EPSILON);\
    ASSERT_NEAR(exp.getZ(), act.getZ(), EPSILON);\
    ASSERT_NEAR(exp.getW(), act.getW(), EPSILON);

#define ASSERT_VEC3(exp, act)\
    ASSERT_NEAR(exp.getX(), act.getX(), EPSILON);\
    ASSERT_NEAR(exp.getY(), act.getY(), EPSILON);\
    ASSERT_NEAR(exp.getZ(), act.getZ(), EPSILON);

struct CollectionProxyComponentRef
{
    dmGameSystem::HCollectionProxyWorld m_World;
    dmGameSystem::HCollectionProxyComponent m_Component;
};

class TextureSetResourceTest : public ResourceTest { public: TextureSetResourceTest() { SetContentFolder("textureset"); } };
class RenderResourceTest : public ResourceTest { public: RenderResourceTest() { SetContentFolder("render"); } };
class DataResourceTest : public ResourceTest { public: DataResourceTest() { SetContentFolder("data"); } };
class LightResourceTest : public ResourceTest { public: LightResourceTest() { SetContentFolder("light"); } };
class ResourceFolderTest : public ResourceTest { public: ResourceFolderTest() { SetContentFolder("resource"); } };
class GuiResourceTest : public ResourceTest { public: GuiResourceTest() { SetContentFolder("gui"); } };
class MaterialResourceTest : public ResourceTest { public: MaterialResourceTest() { SetContentFolder("material"); } };

class CameraComponentTest : public ComponentTest { public: CameraComponentTest() { SetContentFolder("camera"); } };
class MaterialComponentTest : public ComponentTest { public: MaterialComponentTest() { SetContentFolder("material"); } };
class CollectionProxyComponentTest : public ComponentTest { public: CollectionProxyComponentTest() { SetContentFolder("collection_proxy"); } };
class ResourceComponentTest : public ComponentTest { public: ResourceComponentTest() { SetContentFolder("resource"); } };
class GuiComponentTest : public ComponentTest { public: GuiComponentTest() { SetContentFolder("gui"); } };
class LabelComponentTest : public ComponentTest { public: LabelComponentTest() { SetContentFolder("label"); } };
class MiscComponentTest : public ComponentTest { public: MiscComponentTest() { SetContentFolder("misc"); } };
class ParticleFxComponentTest : public ComponentTest { public: ParticleFxComponentTest() { SetContentFolder("particlefx"); } };

bool RunString(lua_State* L, const char* script);
void DeleteInstance(dmGameObject::HCollection collection, dmGameObject::HInstance instance);
CollectionProxyComponentRef GetCollectionProxyComponentRef(dmGameObject::HInstance instance, dmhash_t component_id);
dmGameObject::HCollection GetCollectionByName(dmGameObject::HContext regist, const char* name);
void ConfigureCollectionProxy(CollectionProxyComponentRef proxy, const char* collection_path, dmGameObject::Result expected_load_result = dmGameObject::RESULT_OK);
void UpdateAndPostUpdateCollection(dmGameObject::HCollection collection, dmGameObject::UpdateContext* update_context, dmGameObject::HContext regist);
bool UpdateAndWaitUntilDone(dmGameSystem::ScriptLibContext& scriptlibcontext, dmGameObject::HCollection collection,
                           dmGameObject::UpdateContext* update_context, bool ignore_script_update_fail,
                           const char* tests_done_key, uint32_t timeout_seconds = 1);
void GetResourceProperty(dmGameObject::HInstance instance, dmhash_t comp_name, dmhash_t prop_name, dmhash_t* out_val);
dmGameObject::PropertyResult SetResourceProperty(dmGameObject::HInstance instance, dmhash_t comp_name, dmhash_t prop_name, dmhash_t in_val);
dmhash_t GetHashProperty(dmGameObject::HInstance instance, dmhash_t comp_name, dmhash_t prop_name, const dmGameObject::PropertyOptions* options = 0);
dmGameObject::PropertyResult SetHashProperty(dmGameObject::HInstance instance, dmhash_t comp_name, dmhash_t prop_name, dmhash_t in_val, const dmGameObject::PropertyOptions* options = 0);
void RenderCollection(dmRender::HRenderContext render_context, dmGameObject::HCollection collection);

#endif // DM_TEST_GAMESYS_PRIVATE_H
