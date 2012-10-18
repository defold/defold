#include <gtest/gtest.h>
#include <stdio.h>
#include <algorithm>
#include <map>

#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/math.h>

#include <ddf/ddf.h>

#include "../particle.h"
#include "../particle_private.h"

struct ParticleVertex;

using namespace Vectormath::Aos;

class ParticleTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        m_Context = dmParticle::CreateContext(64, 1024);
        assert(m_Context != 0);
        m_VertexBufferSize = 1024 * 4 * 6 * sizeof(float);
        m_VertexBuffer = new float[m_VertexBufferSize];
        m_Prototype = 0x0;
    }

    virtual void TearDown()
    {
        if (m_Prototype != 0x0)
        {
            dmParticle::Particle_DeletePrototype(m_Prototype);
        }
        dmParticle::DestroyContext(m_Context);
        delete [] m_VertexBuffer;
    }

    void VerifyVertexTexCoords(ParticleVertex* vertex_buffer, float* tex_coords, uint32_t tile);
    void VerifyVertexDims(ParticleVertex* vertex_buffer, uint32_t particle_count, float size, uint32_t tile_width, uint32_t tile_height);

    dmParticle::HContext m_Context;
    dmParticle::HPrototype m_Prototype;
    float* m_VertexBuffer;
    uint32_t m_VertexBufferSize;
};

static const float EPSILON = 0.000001f;
struct ParticleVertex
{
    float m_U, m_V;
    float m_X, m_Y, m_Z;
    float m_Red, m_Green, m_Blue, m_Alpha;
};

void ParticleTest::VerifyVertexTexCoords(ParticleVertex* vertex_buffer, float* tex_coords, uint32_t tile)
{
    uint32_t u0 = 0;
    uint32_t v0 = 1;
    uint32_t u1 = 2;
    uint32_t v1 = 3;
    float* tc = &tex_coords[tile * 4];
    // The particle vertices are ordered like an N, where the first triangle is the lower left, second is upper right
    ASSERT_FLOAT_EQ(tc[u0], vertex_buffer[0].m_U);
    ASSERT_FLOAT_EQ(tc[v1], vertex_buffer[0].m_V);
    ASSERT_FLOAT_EQ(tc[u0], vertex_buffer[1].m_U);
    ASSERT_FLOAT_EQ(tc[v0], vertex_buffer[1].m_V);
    ASSERT_FLOAT_EQ(tc[u1], vertex_buffer[2].m_U);
    ASSERT_FLOAT_EQ(tc[v1], vertex_buffer[2].m_V);
    ASSERT_FLOAT_EQ(tc[u1], vertex_buffer[3].m_U);
    ASSERT_FLOAT_EQ(tc[v1], vertex_buffer[3].m_V);
    ASSERT_FLOAT_EQ(tc[u0], vertex_buffer[4].m_U);
    ASSERT_FLOAT_EQ(tc[v0], vertex_buffer[4].m_V);
    ASSERT_FLOAT_EQ(tc[u1], vertex_buffer[5].m_U);
    ASSERT_FLOAT_EQ(tc[v0], vertex_buffer[5].m_V);
}

void ParticleTest::VerifyVertexDims(ParticleVertex* vertex_buffer, uint32_t particle_count, float size, uint32_t tile_width, uint32_t tile_height)
{
    float width_factor = 1.0f;
    float height_factor = 1.0f;
    if (tile_width > tile_height)
    {
        height_factor = tile_height / (float)tile_width;
    }
    else
    {
        width_factor = tile_width / (float)tile_height;
    }
    for (uint32_t i = 0; i < particle_count; ++i)
    {
        ParticleVertex* v = &vertex_buffer[i*6];
        float x = v[0].m_X - v[2].m_X;
        float y = v[0].m_Y - v[2].m_Y;
        float w = sqrt(x * x + y * y);
        ASSERT_NEAR(size * width_factor, w, 0.000001f);
        x = v[0].m_X - v[1].m_X;
        y = v[0].m_Y - v[1].m_Y;
        float h = sqrt(x * x + y * y);
        ASSERT_NEAR(size * height_factor, h, 0.000001f);
    }
}

bool LoadPrototype(const char* filename, dmParticle::HPrototype* prototype)
{
    char path[64];
    DM_SNPRINTF(path, 64, "build/default/src/test/%s", filename);
    const uint32_t MAX_FILE_SIZE = 4 * 1024;
    unsigned char buffer[MAX_FILE_SIZE];
    uint32_t file_size = 0;

    FILE* f = fopen(path, "rb");
    if (f)
    {
        file_size = fread(buffer, 1, MAX_FILE_SIZE, f);
        *prototype = dmParticle::NewPrototype(buffer, file_size);
        fclose(f);
        return *prototype != 0x0;
    }
    else
    {
        dmLogWarning("Particle FX could not be loaded: %s.", path);
        return false;
    }
}

bool ReloadPrototype(const char* filename, dmParticle::HPrototype prototype)
{
    char path[64];
    DM_SNPRINTF(path, 64, "build/default/src/test/%s", filename);
    const uint32_t MAX_FILE_SIZE = 4 * 1024;
    unsigned char buffer[MAX_FILE_SIZE];
    uint32_t file_size = 0;

    FILE* f = fopen(path, "rb");
    if (f)
    {
        file_size = fread(buffer, 1, MAX_FILE_SIZE, f);
        bool result = dmParticle::ReloadPrototype(prototype, buffer, file_size);
        fclose(f);
        return result;
    }
    else
    {
        dmLogWarning("Particle FX could not be reloaded: %s.", path);
        return false;
    }
}

struct RenderData
{
    void* m_Material;
    void* m_Texture;
    dmParticleDDF::BlendMode m_BlendMode;
    uint32_t m_VertexIndex;
    uint32_t m_VertexCount;
};

void RenderInstanceCallback(void* usercontext, void* material, void* texture, dmParticleDDF::BlendMode blendMode, uint32_t vertex_index, uint32_t vertex_count, dmParticle::RenderConstant* constants, uint32_t constant_count)
{
    RenderData* data = (RenderData*)usercontext;
    data->m_Material = material;
    data->m_Texture = texture;
    data->m_BlendMode = blendMode;
    data->m_VertexIndex = vertex_index;
    data->m_VertexCount = vertex_count;
}

TEST_F(ParticleTest, CreationSuccess)
{
    ASSERT_TRUE(LoadPrototype("once.particlefxc", &m_Prototype));
    dmParticle::HInstance instance = dmParticle::CreateInstance(m_Context, m_Prototype);
    ASSERT_NE(dmParticle::INVALID_INSTANCE, instance);
    ASSERT_EQ(1U, m_Context->m_InstanceIndexPool.Size());
    dmParticle::DestroyInstance(m_Context, instance);
    ASSERT_EQ(0U, m_Context->m_InstanceIndexPool.Size());
}

dmParticle::FetchAnimationResult EmptyFetchAnimationCallback(void* tile_source, dmhash_t animation, dmParticle::AnimationData* out_data)
{
    // Trash data to verify that this function is not called
    memset(out_data, 1, sizeof(*out_data));
    return dmParticle::FETCH_ANIMATION_UNKNOWN_ERROR;
}

dmParticle::FetchAnimationResult FailFetchAnimationCallback(void* tile_source, dmhash_t animation, dmParticle::AnimationData* out_data)
{
    return dmParticle::FETCH_ANIMATION_NOT_FOUND;
}

void EmptyRenderInstanceCallback(void* usercontext, void* material, void* texture, dmParticleDDF::BlendMode blendMode, uint32_t vertex_index, uint32_t vertex_count, dmParticle::RenderConstant* constants, uint32_t constant_count)
{
    // Trash data to verify that this function is not called
    RenderData* data = (RenderData*)usercontext;
    memset(data, 1, sizeof(*data));
}

float g_UnitTexCoords[] =
{
        0.0f, 0.0f, 1.0f, 1.0f
};

TEST_F(ParticleTest, IncompleteParticleFX)
{
    float dt = 1.0f / 60.0f;

    const char* files[] =
    {
            "empty.particlefxc",
            "empty_emitter.particlefxc",
            "empty_emitter.particlefxc"
    };
    bool has_emitter[] =
    {
            false,
            true,
            true
    };
    bool fetch_anim[] =
    {
            false,
            false,
            true
    };
    ParticleVertex vertex_buffer[6];
    for (uint32_t i = 0; i < 3; ++i)
    {
        if (m_Prototype != 0x0)
            dmParticle::Particle_DeletePrototype(m_Prototype);
        ASSERT_TRUE(LoadPrototype(files[i], &m_Prototype));

        dmParticle::HInstance instance = dmParticle::CreateInstance(m_Context, m_Prototype);

        ASSERT_FALSE(dmParticle::IsSpawning(m_Context, instance));

        dmParticle::StartInstance(m_Context, instance);

        if (has_emitter[i])
            ASSERT_TRUE(dmParticle::IsSpawning(m_Context, instance));
        else
            ASSERT_FALSE(dmParticle::IsSpawning(m_Context, instance));

        uint32_t out_vertex_buffer_size;
        if (fetch_anim[i])
        {
            dmParticle::SetTileSource(m_Prototype, 0, (void*)0xBAADF00D);
            dmParticle::Update(m_Context, dt, (float*)vertex_buffer, sizeof(vertex_buffer), &out_vertex_buffer_size, FailFetchAnimationCallback);
        }
        else
        {
            dmParticle::Update(m_Context, dt, (float*)vertex_buffer, sizeof(vertex_buffer), &out_vertex_buffer_size, EmptyFetchAnimationCallback);
        }

        RenderData render_data;

        if (has_emitter[i])
        {
            dmParticle::Render(m_Context, &render_data, RenderInstanceCallback);
            ASSERT_EQ(sizeof(vertex_buffer), out_vertex_buffer_size);
            ASSERT_EQ((void*)0x0, render_data.m_Material);
            ASSERT_EQ((void*)0x0, render_data.m_Texture);
            VerifyVertexTexCoords((ParticleVertex*)&((float*)vertex_buffer)[render_data.m_VertexIndex], g_UnitTexCoords, 0);
            ASSERT_EQ(6u, render_data.m_VertexCount);
            ASSERT_EQ((void*)0x0, render_data.m_Texture);
        }
        else
        {
            memset(&render_data, 0, sizeof(RenderData));
            dmParticle::Render(m_Context, &render_data, EmptyRenderInstanceCallback);
            ASSERT_EQ((void*)0x0, render_data.m_Material);
            ASSERT_EQ((void*)0x0, render_data.m_Texture);
            ASSERT_EQ(0u, render_data.m_VertexCount);
            ASSERT_EQ(0u, out_vertex_buffer_size);
        }

        dmParticle::DestroyInstance(m_Context, instance);
    }
}

TEST_F(ParticleTest, StartInstance)
{
    float dt = 1.0f / 60.0f;

    ASSERT_TRUE(LoadPrototype("once.particlefxc", &m_Prototype));
    dmParticle::HInstance instance = dmParticle::CreateInstance(m_Context, m_Prototype);
    uint16_t index = instance & 0xffff;

    dmParticle::Instance* i = m_Context->m_Instances[index];
    ASSERT_NE((void*)0, (void*)i);
    ASSERT_FALSE(dmParticle::IsSpawning(m_Context, instance));
    ASSERT_EQ(0U, i->m_Emitters[0].m_Particles.Size());

    dmParticle::StartInstance(m_Context, instance);

    ASSERT_TRUE(dmParticle::IsSpawning(m_Context, instance));

    uint32_t out_vertex_buffer_size;
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, &out_vertex_buffer_size, 0x0);

    i = m_Context->m_Instances[index];
    dmParticle::Emitter* e = i->m_Emitters.Begin();
    dmParticle::Particle* p = &e->m_Particles[0];
    ASSERT_NE((void*)0, (void*)i);
    ASSERT_EQ(1U, i->m_Emitters[0].m_Particles.Size());
    ASSERT_LT(0.0f, p->GetTimeLeft());
    ASSERT_EQ(e->m_ParticleTimeLeft, p->GetTimeLeft());
    ASSERT_TRUE(dmParticle::IsSpawning(m_Context, instance));
    ASSERT_GT(m_Prototype->m_DDF->m_Emitters[0].m_Duration, e->m_Timer);
    ASSERT_EQ(6 * sizeof(ParticleVertex), out_vertex_buffer_size); // 6 vertices

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);

    i = m_Context->m_Instances[index];
    e = &i->m_Emitters[0];
    ASSERT_NE((void*)0, (void*)i);
    ASSERT_EQ(0U, i->m_Emitters[0].m_Particles.Size());
    ASSERT_FALSE(dmParticle::IsSpawning(m_Context, instance));
    ASSERT_LT(m_Prototype->m_DDF->m_Emitters[0].m_Duration, e->m_Timer);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);

    ASSERT_LT(m_Prototype->m_DDF->m_Emitters[0].m_Duration, e->m_Timer);
    ASSERT_FALSE(dmParticle::IsSpawning(m_Context, instance));

    dmParticle::DestroyInstance(m_Context, instance);
}

TEST_F(ParticleTest, StartOnceInstance)
{
    float dt = 1.0f / 60.0f;

    ASSERT_TRUE(LoadPrototype("once.particlefxc", &m_Prototype));
    dmParticle::HInstance instance = dmParticle::CreateInstance(m_Context, m_Prototype);
    uint16_t index = instance & 0xffff;
    ASSERT_TRUE(dmParticle::IsSleeping(m_Context, instance));

    dmParticle::StartInstance(m_Context, instance);
    ASSERT_FALSE(dmParticle::IsSleeping(m_Context, instance));

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);

    dmParticle::Emitter* e = &m_Context->m_Instances[index]->m_Emitters[0];
    ASSERT_LT(m_Prototype->m_DDF->m_Emitters[0].m_Duration, e->m_Timer);
    ASSERT_FALSE(dmParticle::IsSpawning(m_Context, instance));
    ASSERT_TRUE(dmParticle::IsSleeping(m_Context, instance));

    dmParticle::DestroyInstance(m_Context, instance);
}

TEST_F(ParticleTest, StartLoopInstance)
{
    const uint32_t loop_count = 4;
    const uint32_t sample_count = 4;
    float dt = 1.0f / sample_count;

    ASSERT_TRUE(LoadPrototype("loop.particlefxc", &m_Prototype));
    dmParticle::HInstance instance = dmParticle::CreateInstance(m_Context, m_Prototype);
    ASSERT_TRUE(dmParticle::IsSleeping(m_Context, instance));

    dmParticle::Emitter* e = &m_Context->m_Instances[instance & 0xffff]->m_Emitters[0];

    dmParticle::StartInstance(m_Context, instance);
    ASSERT_FALSE(dmParticle::IsSleeping(m_Context, instance));

    for (uint32_t i = 0; i < loop_count; ++i)
    {
        for (uint32_t j = 0; j < sample_count; ++j)
        {
            dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
            float v = ((j+1)%sample_count)/(float)sample_count;
            ASSERT_EQ(v, e->m_Particles[0].GetSourceSize());
        }
    }

    ASSERT_TRUE(dmParticle::IsSpawning(m_Context, instance));
    ASSERT_FALSE(dmParticle::IsSleeping(m_Context, instance));

    dmParticle::StopInstance(m_Context, instance);
    ASSERT_TRUE(dmParticle::IsSleeping(m_Context, instance));

    dmParticle::DestroyInstance(m_Context, instance);
}

TEST_F(ParticleTest, FireAndForget)
{
    float dt = 1.0f / 60.0f;

    ASSERT_TRUE(LoadPrototype("once.particlefxc", &m_Prototype));
    dmParticle::FireAndForget(m_Context, m_Prototype, Vectormath::Aos::Point3(0.0f, 0.0f, 0.0f), Vectormath::Aos::Quat::identity());

    // counting on 0
    uint16_t index = 0;

    dmParticle::Instance* i = m_Context->m_Instances[index];

    ASSERT_EQ(1U, m_Context->m_InstanceIndexPool.Size());
    ASSERT_TRUE(dmParticle::IsSpawning(m_Context, i->m_VersionNumber << 16));

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);

    ASSERT_NE((void*)0, (void*)m_Context->m_Instances[index]);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);

    ASSERT_NE((void*)0, (void*)m_Context->m_Instances[index]);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);

    ASSERT_EQ((void*)0, (void*)m_Context->m_Instances[index]);
    ASSERT_EQ(0U, m_Context->m_InstanceIndexPool.Size());
}

TEST_F(ParticleTest, ResetInstance)
{
    float dt = 1.0f / 60.0f;

    ASSERT_TRUE(LoadPrototype("once.particlefxc", &m_Prototype));
    dmParticle::HInstance instance = dmParticle::CreateInstance(m_Context, m_Prototype);
    uint16_t index = instance & 0xffff;

    dmParticle::Instance* i = m_Context->m_Instances[index];
    ASSERT_NE((void*)0, (void*)i);
    ASSERT_FALSE(dmParticle::IsSpawning(m_Context, instance));
    ASSERT_EQ(0U, i->m_Emitters[0].m_Particles.Size());

    dmParticle::StartInstance(m_Context, instance);

    ASSERT_TRUE(dmParticle::IsSpawning(m_Context, instance));

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    ASSERT_EQ(1U, i->m_Emitters[0].m_Particles.Size());

    dmParticle::ResetInstance(m_Context, instance);
    ASSERT_FALSE(dmParticle::IsSpawning(m_Context, instance));
    ASSERT_EQ(0U, i->m_Emitters[0].m_Particles.Size());

    // Start again
    dmParticle::StartInstance(m_Context, instance);
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    ASSERT_TRUE(dmParticle::IsSpawning(m_Context, instance));
    ASSERT_EQ(1U, i->m_Emitters[0].m_Particles.Size());

    dmParticle::DestroyInstance(m_Context, instance);
}

TEST_F(ParticleTest, EmissionSpace)
{
    float dt = 1.0f / 60.0f;

    // Test world space

    ASSERT_TRUE(LoadPrototype("once.particlefxc", &m_Prototype));
    dmParticle::HInstance instance = dmParticle::CreateInstance(m_Context, m_Prototype);
    uint16_t index = instance & 0xffff;

    dmParticle::SetPosition(m_Context, instance, Vectormath::Aos::Point3(10.0f, 0.0f, 0.0f));
    dmParticle::StartInstance(m_Context, instance);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);

    dmParticle::Emitter* e = &m_Context->m_Instances[index]->m_Emitters[0];
    dmParticle::Particle* p = &e->m_Particles[0];
    ASSERT_LT(0.0f, p->GetTimeLeft());
    ASSERT_EQ(10.0f, p->GetPosition().getX());

    dmParticle::DestroyInstance(m_Context, instance);

    // Test emitter space

    dmParticle::Particle_DeletePrototype(m_Prototype);
    ASSERT_TRUE(LoadPrototype("emitter_space.particlefxc", &m_Prototype));
    instance = dmParticle::CreateInstance(m_Context, m_Prototype);
    index = instance & 0xffff;

    dmParticle::SetPosition(m_Context, instance, Vectormath::Aos::Point3(10.0f, 0.0f, 0.0f));
    dmParticle::StartInstance(m_Context, instance);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);

    e = &m_Context->m_Instances[index]->m_Emitters[0];
    p = &e->m_Particles[0];
    ASSERT_LT(0.0f, p->GetTimeLeft());
    ASSERT_EQ(0.0f, p->GetPosition().getX());

    dmParticle::DestroyInstance(m_Context, instance);
}

TEST_F(ParticleTest, RestartInstance)
{
    float dt = 1.0f / 60.0f;

    ASSERT_TRUE(LoadPrototype("restart.particlefxc", &m_Prototype));
    dmParticle::HInstance instance = dmParticle::CreateInstance(m_Context, m_Prototype);
    uint16_t index = instance & 0xffff;

    dmParticle::StartInstance(m_Context, instance);

    ASSERT_TRUE(dmParticle::IsSpawning(m_Context, instance));

    dmParticle::Emitter* e = &m_Context->m_Instances[index]->m_Emitters[0];
    ASSERT_EQ(0u, e->m_Particles.Size());

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);

    ASSERT_EQ(1u, e->m_Particles.Size());

    dmParticle::RestartInstance(m_Context, instance);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);

    ASSERT_EQ(1u, e->m_Particles.Size());

    dmParticle::DestroyInstance(m_Context, instance);
}

/**
 * The emitter has a spline for particle size, which has the points and tangents:
 * (0.00, 0), (1,0)
 * (0.25, 0), (1,1)
 * (0.50, 1), (1,0)
 * (0.75, 0), (1,-1)
 * (1.00, 0), (1,0)
 *
 * Test evaluation of the size at t = [0, 0.125, 0.25, 0.375, 0.5, 0.625, 0.75, 0.875, 1]
 */
TEST_F(ParticleTest, EvaluateEmitterProperty)
{
    float dt = 1.0f / 8.0f;

    ASSERT_TRUE(LoadPrototype("emitter_spline.particlefxc", &m_Prototype));
    dmParticle::HInstance instance = dmParticle::CreateInstance(m_Context, m_Prototype);
    uint16_t index = instance & 0xffff;
    dmParticle::Instance* i = m_Context->m_Instances[index];

    dmParticle::StartInstance(m_Context, instance);

    // t = 0.125, size < 0
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    dmParticle::Particle* particle = &i->m_Emitters[0].m_Particles[0];
    ASSERT_GT(0.0f, particle->GetSize());

    // t = 0.25, size = 0
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    ASSERT_DOUBLE_EQ(0.0f, particle->GetSize());

    // t = 0.375, size > 0
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    ASSERT_LT(0.0f, particle->GetSize());

    // t = 0.5, size = 1
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    ASSERT_DOUBLE_EQ(1.0f, particle->GetSize());

    // t = 0.625, size > 0
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    ASSERT_LT(0.0f, particle->GetSize());

    // t = 0.75, size = 0
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    ASSERT_DOUBLE_EQ(0.0f, particle->GetSize());

    // t = 0.875, size < 0
    // Updating with a full dt here will make the emitter reach its duration
    dmParticle::Update(m_Context, dt - EPSILON, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    ASSERT_GT(0.0f, particle->GetSize());

    // t = 1, size = 0
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    ASSERT_NEAR(0.0f, particle->GetSize(), EPSILON);

    dmParticle::DestroyInstance(m_Context, instance);
}

/**
 * The emitter has a constant for particle size = 0 and spread +- 1.0
 */
TEST_F(ParticleTest, EvaluateEmitterPropertySpread)
{
    float dt = 1.0f / 8.0f;

    ASSERT_TRUE(LoadPrototype("emitter_spline_spread.particlefxc", &m_Prototype));
    dmParticle::HInstance instance = dmParticle::CreateInstance(m_Context, m_Prototype);
    uint16_t index = instance & 0xffff;
    dmParticle::Instance* i = m_Context->m_Instances[index];

    dmParticle::StartInstance(m_Context, instance);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    dmParticle::Particle* particle = &i->m_Emitters[0].m_Particles[0];
    // NOTE size could potentially be 0, but not likely
    ASSERT_NE(0.0f, particle->GetSize());
    ASSERT_GT(1.0f, dmMath::Abs(particle->GetSize()));

    dmParticle::DestroyInstance(m_Context, instance);
}

/**
 * The emitter has a spline for particle scale (size is always 1), which has the points and tangents:
 * (0.00, 0), (1,0)
 * (0.25, 0), (1,1)
 * (0.50, 1), (1,0)
 * (0.75, 0), (1,-1)
 * (1.00, 0), (1,0)
 *
 * Test evaluation of the size at t = [0, 0.125, 0.25, 0.375, 0.5, 0.625, 0.75, 0.875, 1]
 */
TEST_F(ParticleTest, EvaluateParticleProperty)
{
    float dt = 1.0f / 8.0f;

    ASSERT_TRUE(LoadPrototype("particle_spline.particlefxc", &m_Prototype));
    dmParticle::HInstance instance = dmParticle::CreateInstance(m_Context, m_Prototype);
    uint16_t index = instance & 0xffff;
    dmParticle::Instance* i = m_Context->m_Instances[index];

    dmParticle::StartInstance(m_Context, instance);

    // t = 0.125, size < 0
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    dmParticle::Particle* particle = &i->m_Emitters[0].m_Particles[0];
    ASSERT_GT(0.0f, particle->GetSize());

    // t = 0.25, size = 0
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    ASSERT_DOUBLE_EQ(0.0f, particle->GetSize());

    // t = 0.375, size > 0
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    ASSERT_LT(0.0f, particle->GetSize());

    // t = 0.5, size = 1
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    ASSERT_DOUBLE_EQ(1.0f, particle->GetSize());

    // t = 0.625, size > 0
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    ASSERT_LT(0.0f, particle->GetSize());

    // t = 0.75, size = 0
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    ASSERT_DOUBLE_EQ(0.0f, particle->GetSize());

    // t = 0.875, size < 0
    // Updating with a full dt here will make the emitter reach its duration
    dmParticle::Update(m_Context, dt - EPSILON, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    ASSERT_GT(0.0f, particle->GetSize());

    // t = 1, size = 0
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    ASSERT_NEAR(0.0f, particle->GetSize(), EPSILON);

    dmParticle::DestroyInstance(m_Context, instance);
}

/**
 * Test that flip book animations are updated correctly
 */

float g_TexCoords[] =
{
        // 2 x 4 tiles
        0.0f, 0.0f, 0.25f, 0.5f,
        0.25f, 0.0f, 0.5f, 0.5f,
        0.5f, 0.0f, 0.75f, 0.5f,
        0.75f, 0.0f, 1.0f, 0.5f,
        0.0f, 0.5f, 0.25f, 1.0f,
        0.25f, 0.5f, 0.5f, 1.0f,
        0.5f, 0.5f, 0.75f, 1.0f,
        0.75f, 0.5f, 1.0f, 1.0f,
};

struct TileSource
{
    TileSource()
    : m_Texture((void*)0xBAADF00D)
    , m_TexCoords(g_TexCoords)
    {

    }

    void* m_Texture;
    float* m_TexCoords;
};

dmParticle::FetchAnimationResult FetchAnimationCallback(void* tile_source, dmhash_t animation, dmParticle::AnimationData* out_data)
{
    if (tile_source == 0x0)
    {
        return dmParticle::FETCH_ANIMATION_UNKNOWN_ERROR;
    }
    TileSource* ts = (TileSource*)tile_source;
    out_data->m_Texture = ts->m_Texture;
    out_data->m_TexCoords = ts->m_TexCoords;
    if (animation == dmHashString64("once"))
    {
        out_data->m_Playback = dmParticle::ANIM_PLAYBACK_ONCE_FORWARD;
        out_data->m_TileWidth = 2;
        out_data->m_TileHeight = 3;
        out_data->m_StartTile = 1;
        out_data->m_EndTile = 5;
        out_data->m_FPS = 30;
    }
    else
    {
        return dmParticle::FETCH_ANIMATION_NOT_FOUND;
    }
    out_data->m_Texture = (void*)0xBAADF00D;
    out_data->m_StructSize = sizeof(dmParticle::AnimationData);
    return dmParticle::FETCH_ANIMATION_OK;
}

TEST_F(ParticleTest, Animation)
{
    float dt = 0.2f;

    ASSERT_TRUE(LoadPrototype("anim.particlefxc", &m_Prototype));

    dmParticle::HInstance instance = dmParticle::CreateInstance(m_Context, m_Prototype);
    uint16_t index = instance & 0xffff;
    dmParticle::Instance* i = m_Context->m_Instances[index];
    dmParticle::Emitter* emitter = &i->m_Emitters[0];

    TileSource tile_source;
    dmParticle::SetTileSource(m_Prototype, 0, &tile_source);

    dmParticle::StartInstance(m_Context, instance);

    ParticleVertex vertex_buffer[6];
    uint32_t vertex_buffer_size;

    // Test once anim
    m_Prototype->m_Emitters[0].m_Animation = dmHashString64("once");

    // 5 tiles
    for (uint32_t i = 0; i < 5; ++i)
    {
        dmParticle::Update(m_Context, dt, (float*)vertex_buffer, sizeof(vertex_buffer), &vertex_buffer_size, FetchAnimationCallback);
        VerifyVertexTexCoords(vertex_buffer, g_TexCoords, i);
        VerifyVertexDims(vertex_buffer, 1, 1.0f, 2, 3);
    }

    ASSERT_EQ(sizeof(vertex_buffer), vertex_buffer_size);
    dmParticle::Particle* particle = emitter->m_Particles.Begin();
    ASSERT_LT(0.0f, particle->GetTimeLeft());

    // Test rendering of last frame
    RenderData data;
    dmParticle::Render(m_Context, &data, RenderInstanceCallback);
    ASSERT_EQ(m_Prototype->m_Emitters[0].m_Material, data.m_Material);
    ASSERT_EQ(tile_source.m_Texture, data.m_Texture);
    ASSERT_EQ(0u, data.m_VertexIndex);
    ASSERT_EQ(6u, data.m_VertexCount);

    // Particle dead
    dmParticle::Update(m_Context, dt, (float*)vertex_buffer, sizeof(vertex_buffer), &vertex_buffer_size, FetchAnimationCallback);
    ASSERT_EQ(0u, vertex_buffer_size);
    ASSERT_GT(0.0f, particle->GetTimeLeft());

    dmParticle::DestroyInstance(m_Context, instance);
}

TEST_F(ParticleTest, InvalidKeys)
{
    ASSERT_TRUE(LoadPrototype("invalid_keys.particlefxc", &m_Prototype));
}

TEST_F(ParticleTest, StableSort)
{
    float dt = 1.0f / 60.0f;

    ASSERT_TRUE(LoadPrototype("sort.particlefxc", &m_Prototype));
    dmParticle::HInstance instance = dmParticle::CreateInstance(m_Context, m_Prototype);
    uint16_t index = instance & 0xffff;

    dmParticle::Instance* i = m_Context->m_Instances[index];

    dmParticle::StartInstance(m_Context, instance);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);

    const uint32_t particle_count = 20;
    ASSERT_EQ(particle_count, i->m_Emitters[0].m_Particles.Size());

    float x[particle_count];
    dmParticle::Particle* p = &i->m_Emitters[0].m_Particles[0];
    // Store x-positions
    for (uint32_t pi = 0; pi < particle_count; ++pi)
    {
        float f = (float)pi + 1;
        x[pi] = f;
        Point3 pos = p[pi].GetPosition();
        pos.setX(f);
        p[pi].SetPosition(pos);
    }
    // Disturb order by altering a few particles
    const uint32_t disturb_count = particle_count / 2;
    for (uint32_t d = 0; d < disturb_count; ++d)
    {
        p[d].SetTimeLeft(p[d].GetTimeLeft() - dt);
        x[d] += particle_count;
        Point3 pos = p[d].GetPosition();
        pos.setX(x[d]);
        p[d].SetPosition(pos);
    }
    // Sort
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    // Sort verification
    std::sort(x, x+particle_count);
    // Verify order of undisturbed
    for (uint32_t pi = 0; pi < particle_count; ++pi)
    {
        ASSERT_EQ(x[pi], p[pi].GetPosition().getX());
    }

    dmParticle::DestroyInstance(m_Context, instance);
}

TEST_F(ParticleTest, ReloadPrototype)
{
    ASSERT_TRUE(LoadPrototype("reload1.particlefxc", &m_Prototype));
    ASSERT_EQ(1u, m_Prototype->m_Emitters.Size());

    ASSERT_TRUE(ReloadPrototype("reload2.particlefxc", m_Prototype));
    ASSERT_EQ(2u, m_Prototype->m_Emitters.Size());

    ASSERT_TRUE(ReloadPrototype("reload1.particlefxc", m_Prototype));
    ASSERT_EQ(1u, m_Prototype->m_Emitters.Size());
}

TEST_F(ParticleTest, ReloadInstance)
{
    float dt = 1.0f / 60.0f;

    ASSERT_TRUE(LoadPrototype("reload1.particlefxc", &m_Prototype));

    dmParticle::HInstance instance = dmParticle::CreateInstance(m_Context, m_Prototype);
    uint16_t index = instance & 0xffff;
    dmParticle::Instance* i = m_Context->m_Instances[index];
    dmParticle::StartInstance(m_Context, instance);
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);

    ASSERT_EQ(1u, i->m_Emitters.Size());
    ASSERT_EQ(1u, i->m_Emitters[0].m_Particles.Size());
    ASSERT_TRUE(dmParticle::IsSpawning(m_Context, instance));

    dmParticle::Particle original_particle;
    memcpy(&original_particle, &i->m_Emitters[0].m_Particles[0], sizeof(dmParticle::Particle));

    ASSERT_TRUE(ReloadPrototype("reload2.particlefxc", m_Prototype));
    dmParticle::ReloadInstance(m_Context, instance, false);

    ASSERT_EQ(2u, i->m_Emitters.Size());
    ASSERT_EQ(1u, i->m_Emitters[0].m_Particles.Size());
    dmParticle::Particle* particle = &i->m_Emitters[0].m_Particles[0];
    ASSERT_EQ(0, memcmp(&original_particle, particle, sizeof(dmParticle::Particle)));
    ASSERT_EQ(1u, i->m_Emitters[1].m_Particles.Size());
    ASSERT_TRUE(dmParticle::IsSpawning(m_Context, instance));

    ASSERT_TRUE(ReloadPrototype("reload1.particlefxc", m_Prototype));
    dmParticle::ReloadInstance(m_Context, instance, false);
    ASSERT_EQ(1u, i->m_Emitters.Size());
    ASSERT_EQ(1u, i->m_Emitters[0].m_Particles.Size());
    particle = &i->m_Emitters[0].m_Particles[0];
    ASSERT_EQ(0, memcmp(&original_particle, particle, sizeof(dmParticle::Particle)));

    ASSERT_EQ(1u, i->m_Emitters.Size());
    ASSERT_EQ(1u, i->m_Emitters[0].m_Particles.Size());
    ASSERT_TRUE(dmParticle::IsSpawning(m_Context, instance));

    // Test reload with max_particle_count changed
    ASSERT_TRUE(ReloadPrototype("reload3.particlefxc", m_Prototype));
    dmParticle::ReloadInstance(m_Context, instance, false);

    ASSERT_EQ(1u, i->m_Emitters.Size());
    ASSERT_EQ(2u, i->m_Emitters[0].m_Particles.Size());
    particle = &i->m_Emitters[0].m_Particles[0];
    ASSERT_EQ(0, memcmp(&original_particle, particle, sizeof(dmParticle::Particle)));
    ASSERT_TRUE(dmParticle::IsSpawning(m_Context, instance));

    dmParticle::DestroyInstance(m_Context, instance);
}

TEST_F(ParticleTest, ReloadInstanceLoop)
{
    float dt = 1.0f / 60.0f;

    ASSERT_TRUE(LoadPrototype("reload_loop.particlefxc", &m_Prototype));

    dmParticle::HInstance instance = dmParticle::CreateInstance(m_Context, m_Prototype);
    uint16_t index = instance & 0xffff;
    dmParticle::Instance* i = m_Context->m_Instances[index];
    dmParticle::StartInstance(m_Context, instance);

    const float time = 0.4f;
    float timer = 0.0f;
    while (timer < time)
    {
        dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
        timer += dt;
    }

    ASSERT_EQ(1u, i->m_Emitters.Size());
    ASSERT_EQ(1u, i->m_Emitters[0].m_Particles.Size());
    float emitter_timer = i->m_Emitters[0].m_Timer;

    dmParticle::Particle original_particle;
    memcpy(&original_particle, &i->m_Emitters[0].m_Particles[0], sizeof(dmParticle::Particle));

    ASSERT_TRUE(ReloadPrototype("reload_loop.particlefxc", m_Prototype));
    dmParticle::ReloadInstance(m_Context, instance, true);

    ASSERT_EQ(1u, i->m_Emitters.Size());
    ASSERT_EQ(emitter_timer, i->m_Emitters[0].m_Timer);
    ASSERT_EQ(1u, i->m_Emitters[0].m_Particles.Size());
    dmParticle::Particle* particle = &i->m_Emitters[0].m_Particles[0];
    ASSERT_EQ(0, memcmp(&original_particle, particle, sizeof(dmParticle::Particle)));

    dmParticle::DestroyInstance(m_Context, instance);
}

TEST_F(ParticleTest, Acceleration)
{
    float dt = 1.0f;

    ASSERT_TRUE(LoadPrototype("mod_acc.particlefxc", &m_Prototype));
    dmParticle::HInstance instance = dmParticle::CreateInstance(m_Context, m_Prototype);
    uint16_t index = instance & 0xffff;
    dmParticle::Instance* i = m_Context->m_Instances[index];

    dmParticle::StartInstance(m_Context, instance);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    dmParticle::Particle* particle = &i->m_Emitters[0].m_Particles[0];
    ASSERT_EQ(1.0f, lengthSqr(particle->GetVelocity()));

    dmParticle::DestroyInstance(m_Context, instance);
}

TEST_F(ParticleTest, DragNoDir)
{
    float dt = 1.0f;

    ASSERT_TRUE(LoadPrototype("mod_drag_nodir.particlefxc", &m_Prototype));
    dmParticle::HInstance instance = dmParticle::CreateInstance(m_Context, m_Prototype);
    uint16_t index = instance & 0xffff;
    dmParticle::Instance* i = m_Context->m_Instances[index];

    dmParticle::StartInstance(m_Context, instance);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    dmParticle::Particle* particle = &i->m_Emitters[0].m_Particles[0];
    ASSERT_EQ(0.0f, lengthSqr(particle->GetVelocity()));

    dmParticle::DestroyInstance(m_Context, instance);
}

TEST_F(ParticleTest, DragDir)
{
    float dt = 1.0f;

    ASSERT_TRUE(LoadPrototype("mod_drag_dir.particlefxc", &m_Prototype));
    dmParticle::HInstance instance = dmParticle::CreateInstance(m_Context, m_Prototype);
    uint16_t index = instance & 0xffff;
    dmParticle::Instance* i = m_Context->m_Instances[index];

    dmParticle::StartInstance(m_Context, instance);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    dmParticle::Particle* particle = &i->m_Emitters[0].m_Particles[0];
    Vector3 velocity = particle->GetVelocity();
    ASSERT_NEAR(0.0f, velocity.getX(), EPSILON);
    ASSERT_LT(0.0f, velocity.getY());
    ASSERT_EQ(0.0f, velocity.getZ());

    dmParticle::DestroyInstance(m_Context, instance);
}

TEST_F(ParticleTest, DragBigMagnitude)
{
    float dt = 1.0f / 4.0f;

    ASSERT_TRUE(LoadPrototype("mod_drag_bigmag.particlefxc", &m_Prototype));
    dmParticle::HInstance instance = dmParticle::CreateInstance(m_Context, m_Prototype);
    uint16_t index = instance & 0xffff;
    dmParticle::Instance* i = m_Context->m_Instances[index];

    dmParticle::StartInstance(m_Context, instance);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    dmParticle::Particle* particle = &i->m_Emitters[0].m_Particles[0];
    ASSERT_EQ(0u, lengthSqr(particle->GetVelocity()));

    dmParticle::DestroyInstance(m_Context, instance);
}

TEST_F(ParticleTest, Radial)
{
    float dt = 1.0f;

    ASSERT_TRUE(LoadPrototype("mod_radial.particlefxc", &m_Prototype));
    dmParticle::HInstance instance = dmParticle::CreateInstance(m_Context, m_Prototype);
    uint16_t index = instance & 0xffff;
    dmParticle::Instance* i = m_Context->m_Instances[index];

    dmParticle::StartInstance(m_Context, instance);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    dmParticle::Particle* particle = &i->m_Emitters[0].m_Particles[0];
    ASSERT_EQ(1.0f, lengthSqr(particle->GetVelocity()));
    ASSERT_EQ(-1.0f, particle->GetVelocity().getX());

    dmParticle::DestroyInstance(m_Context, instance);
}

TEST_F(ParticleTest, RadialMaxDistance)
{
    float dt = 1.0f;

    ASSERT_TRUE(LoadPrototype("mod_radial_maxdist.particlefxc", &m_Prototype));
    dmParticle::HInstance instance = dmParticle::CreateInstance(m_Context, m_Prototype);
    uint16_t index = instance & 0xffff;
    dmParticle::Instance* i = m_Context->m_Instances[index];

    dmParticle::StartInstance(m_Context, instance);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    dmParticle::Particle* particle = &i->m_Emitters[0].m_Particles[0];
    ASSERT_EQ(0.0f, lengthSqr(particle->GetVelocity()));

    dmParticle::DestroyInstance(m_Context, instance);
}

// Velocity-less particle at the same position as the radial
TEST_F(ParticleTest, RadialEdgeCase)
{
    float dt = 1.0f;

    ASSERT_TRUE(LoadPrototype("mod_radial_edgecase.particlefxc", &m_Prototype));
    dmParticle::HInstance instance = dmParticle::CreateInstance(m_Context, m_Prototype);
    uint16_t index = instance & 0xffff;
    dmParticle::Instance* i = m_Context->m_Instances[index];

    dmParticle::StartInstance(m_Context, instance);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    dmParticle::Particle* particle = &i->m_Emitters[0].m_Particles[0];
    ASSERT_EQ(1.0f, lengthSqr(particle->GetVelocity()));

    dmParticle::DestroyInstance(m_Context, instance);
}

TEST_F(ParticleTest, Vortex)
{
    float dt = 1.0f;

    ASSERT_TRUE(LoadPrototype("mod_vortex.particlefxc", &m_Prototype));
    dmParticle::HInstance instance = dmParticle::CreateInstance(m_Context, m_Prototype);
    uint16_t index = instance & 0xffff;
    dmParticle::Instance* i = m_Context->m_Instances[index];

    dmParticle::StartInstance(m_Context, instance);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    dmParticle::Particle* particle = &i->m_Emitters[0].m_Particles[0];
    ASSERT_EQ(0.0f, particle->GetVelocity().getX());
    ASSERT_EQ(-1.0f, particle->GetVelocity().getY());
    ASSERT_EQ(0.0f, particle->GetVelocity().getZ());

    dmParticle::DestroyInstance(m_Context, instance);
}

TEST_F(ParticleTest, VortexMaxDistance)
{
    float dt = 1.0f;

    ASSERT_TRUE(LoadPrototype("mod_vortex_maxdist.particlefxc", &m_Prototype));
    dmParticle::HInstance instance = dmParticle::CreateInstance(m_Context, m_Prototype);
    uint16_t index = instance & 0xffff;
    dmParticle::Instance* i = m_Context->m_Instances[index];

    dmParticle::StartInstance(m_Context, instance);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    dmParticle::Particle* particle = &i->m_Emitters[0].m_Particles[0];
    ASSERT_EQ(0.0f, particle->GetVelocity().getX());
    ASSERT_EQ(0.0f, particle->GetVelocity().getY());
    ASSERT_EQ(0.0f, particle->GetVelocity().getZ());

    dmParticle::DestroyInstance(m_Context, instance);
}

TEST_F(ParticleTest, VortexEdgeCase)
{
    float dt = 1.0f;

    ASSERT_TRUE(LoadPrototype("mod_vortex_edgecase.particlefxc", &m_Prototype));
    dmParticle::HInstance instance = dmParticle::CreateInstance(m_Context, m_Prototype);
    uint16_t index = instance & 0xffff;
    dmParticle::Instance* i = m_Context->m_Instances[index];

    dmParticle::StartInstance(m_Context, instance);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    dmParticle::Particle* particle = &i->m_Emitters[0].m_Particles[0];
    ASSERT_EQ(-1.0f, particle->GetVelocity().getX());
    ASSERT_EQ(0.0f, particle->GetVelocity().getY());
    ASSERT_EQ(0.0f, particle->GetVelocity().getZ());

    dmParticle::DestroyInstance(m_Context, instance);
}

TEST_F(ParticleTest, case1544)
{
    ASSERT_TRUE(LoadPrototype("modifier_crash.particlefxc", &m_Prototype));
}

void RenderConstantRenderInstanceCallback(void* usercontext, void* material, void* texture, dmParticleDDF::BlendMode blendMode, uint32_t vertex_index, uint32_t vertex_count, dmParticle::RenderConstant* constants, uint32_t constant_count)
{
    std::map<dmhash_t, Vector4>* render_constants = (std::map<dmhash_t, Vector4>*)usercontext;
    for (uint32_t i = 0; i < constant_count; ++i)
    {
        dmParticle::RenderConstant& c = constants[i];
        (*render_constants)[c.m_NameHash] = c.m_Value;
    }
}

TEST_F(ParticleTest, RenderConstants)
{
    dmhash_t emitter_id = dmHashString64("emitter");
    dmhash_t constant_id = dmHashString64("tint");

    float dt = 1.0f / 60.0f;

    ASSERT_TRUE(LoadPrototype("render_constant.particlefxc", &m_Prototype));
    dmParticle::HInstance instance = dmParticle::CreateInstance(m_Context, m_Prototype);

    dmParticle::StartInstance(m_Context, instance);

    std::map<dmhash_t, Vector4> constants;

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);

    dmParticle::Render(m_Context, (void*)&constants, RenderConstantRenderInstanceCallback);

    ASSERT_TRUE(constants.empty());

    dmParticle::SetRenderConstant(m_Context, instance, emitter_id, constant_id, Vector4(1, 2, 3, 4));
    dmParticle::Render(m_Context, (void*)&constants, RenderConstantRenderInstanceCallback);

    ASSERT_TRUE(constants.end() != constants.find(constant_id));
    Vector4 v = constants.at(constant_id);
    ASSERT_EQ(1, v.getX());
    ASSERT_EQ(2, v.getY());
    ASSERT_EQ(3, v.getZ());
    ASSERT_EQ(4, v.getW());

    constants.clear();

    dmParticle::ResetRenderConstant(m_Context, instance, emitter_id, constant_id);
    dmParticle::Render(m_Context, (void*)&constants, RenderConstantRenderInstanceCallback);

    ASSERT_TRUE(constants.empty());

    dmParticle::DestroyInstance(m_Context, instance);
}

TEST_F(ParticleTest, InheritVelocity)
{
    float dt = 1.0f / 60.0f;

    ASSERT_TRUE(LoadPrototype("inherit_velocity.particlefxc", &m_Prototype));
    dmParticle::HInstance instance = dmParticle::CreateInstance(m_Context, m_Prototype);
    uint16_t index = instance & 0xffff;
    dmParticle::Instance* i = m_Context->m_Instances[index];
    dmParticle::Emitter* e = &i->m_Emitters[0];

    dmParticle::StartInstance(m_Context, instance);
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);

    dmParticle::Particle* p = &e->m_Particles[0];
    ASSERT_EQ(0u, lengthSqr(p->GetVelocity()));

    dmParticle::ResetInstance(m_Context, instance);
    dmParticle::StartInstance(m_Context, instance);
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    ASSERT_EQ(1u, e->m_Particles.Size());
    dmParticle::SetPosition(m_Context, instance, Point3(10, 0, 0));
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    ASSERT_NE(0.0f, lengthSqr(e->m_Velocity));
    ASSERT_EQ(2u, e->m_Particles.Size());

    // New particle is at index 0 because of sorting
    p = &i->m_Emitters[0].m_Particles[0];
    ASSERT_NE(0u, lengthSqr(p->GetVelocity()));
    p = &i->m_Emitters[0].m_Particles[1];
    ASSERT_EQ(0u, lengthSqr(p->GetVelocity()));

    dmParticle::DestroyInstance(m_Context, instance);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
