#include <gtest/gtest.h>

#include <dlib/dstrings.h>
#include <dlib/log.h>

#include <ddf/ddf.h>

#include "../particle.h"
#include "../particle_private.h"

class ParticleTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        m_Context = dmParticle::CreateContext(64, 1024);
        assert(m_Context != 0);
        m_Prototype.m_DDF = 0;
        m_Prototype.m_Material = 0;
        m_Prototype.m_Texture = 0;
        m_VertexBufferSize = 1024 * 4 * 6 * sizeof(float);
        m_VertexBuffer = new float[m_VertexBufferSize];
    }

    virtual void TearDown()
    {
        dmParticle::DestroyContext(m_Context);
        if (m_Prototype.m_DDF != 0)
            dmDDF::FreeMessage(m_Prototype.m_DDF);
        delete [] m_VertexBuffer;
    }

    dmParticle::HContext m_Context;
    dmParticle::Prototype m_Prototype;
    float* m_VertexBuffer;
    uint32_t m_VertexBufferSize;
};

bool LoadPrototype(const char* filename, dmParticle::Prototype* prototype)
{
    dmParticleDDF::Emitter* prototype_ddf;
    char path[64];
    DM_SNPRINTF(path, 64, "build/default/src/test/%s", filename);
    dmDDF::Result e = dmDDF::LoadMessageFromFile(path, &dmParticleDDF_Emitter_DESCRIPTOR, (void**) &prototype_ddf);
    if (e != dmDDF::RESULT_OK)
    {
        dmLogWarning("Emitter could not be loaded: %s.", path);
        return false;
    }
    if (prototype_ddf->m_Material == 0x0 || *prototype_ddf->m_Material == '\0'
            || prototype_ddf->m_Texture.m_Name == 0x0 || *prototype_ddf->m_Texture.m_Name == '\0')
    {
        dmDDF::FreeMessage(prototype_ddf);
        return false;
    }

    if (prototype->m_DDF != 0)
        dmDDF::FreeMessage(prototype->m_DDF);
    prototype->m_DDF = prototype_ddf;
    prototype->m_Material = 0;
    prototype->m_Texture = 0;
    return true;
}

TEST_F(ParticleTest, CreationSuccess)
{
    ASSERT_TRUE(LoadPrototype("once.particlefxc", &m_Prototype));
    dmParticle::HEmitter emitter = dmParticle::CreateEmitter(m_Context, &m_Prototype);
    ASSERT_NE(dmParticle::INVALID_EMITTER, emitter);
    ASSERT_EQ(1U, m_Context->m_EmitterIndexPool.Size());
    dmParticle::DestroyEmitter(m_Context, emitter);
    ASSERT_EQ(0U, m_Context->m_EmitterIndexPool.Size());
}

TEST_F(ParticleTest, CreationFailure)
{
    ASSERT_FALSE(LoadPrototype("null.particlefxc", &m_Prototype));
}

TEST_F(ParticleTest, StartEmitter)
{
    float dt = 1.0f / 60.0f;

    ASSERT_TRUE(LoadPrototype("once.particlefxc", &m_Prototype));
    dmParticle::HEmitter emitter = dmParticle::CreateEmitter(m_Context, &m_Prototype);
    uint16_t index = emitter & 0xffff;

    ASSERT_NE((void*)0, (void*)m_Context->m_Emitters[index]);
    ASSERT_EQ(0U, m_Context->m_Emitters[index]->m_IsSpawning);
    ASSERT_EQ(1U, m_Context->m_Emitters[index]->m_Particles.Size());

    dmParticle::StartEmitter(m_Context, emitter);

    ASSERT_EQ(1U, m_Context->m_Emitters[index]->m_IsSpawning);

    uint32_t out_vertex_buffer_size;
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, &out_vertex_buffer_size);

    ASSERT_NE((void*)0, (void*)m_Context->m_Emitters[index]);
    ASSERT_LT(0.0f, m_Context->m_Emitters[index]->m_Particles[0].m_TimeLeft);
    ASSERT_EQ(1U, m_Context->m_Emitters[index]->m_IsSpawning);
    ASSERT_GT(m_Context->m_Emitters[index]->m_Prototype->m_DDF->m_Duration, m_Context->m_Emitters[index]->m_Timer);
    ASSERT_EQ(6 * 6 * sizeof(float), out_vertex_buffer_size); // 6 vertices of 6 floats (pos, uv, alpha)

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0);

    ASSERT_NE((void*)0, (void*)m_Context->m_Emitters[index]);
    ASSERT_GT(0.0f, m_Context->m_Emitters[index]->m_Particles[0].m_TimeLeft);
    ASSERT_EQ(m_Context->m_Emitters[index]->m_ParticleTimeLeft, m_Context->m_Emitters[index]->m_Particles[0].m_TimeLeft);
    ASSERT_EQ(0U, m_Context->m_Emitters[index]->m_IsSpawning);
    ASSERT_LT(m_Context->m_Emitters[index]->m_Prototype->m_DDF->m_Duration, m_Context->m_Emitters[index]->m_Timer);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0);

    ASSERT_LT(m_Context->m_Emitters[index]->m_Prototype->m_DDF->m_Duration, m_Context->m_Emitters[index]->m_Timer);
    ASSERT_EQ(0U, m_Context->m_Emitters[index]->m_IsSpawning);

    dmParticle::DestroyEmitter(m_Context, emitter);
}

TEST_F(ParticleTest, StartOnceEmitter)
{
    float dt = 1.0f / 60.0f;

    ASSERT_TRUE(LoadPrototype("once.particlefxc", &m_Prototype));
    dmParticle::HEmitter emitter = dmParticle::CreateEmitter(m_Context, &m_Prototype);
    uint16_t index = emitter & 0xffff;

    dmParticle::StartEmitter(m_Context, emitter);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0);

    ASSERT_LT(m_Context->m_Emitters[index]->m_Prototype->m_DDF->m_Duration, m_Context->m_Emitters[index]->m_Timer);
    ASSERT_EQ(0U, m_Context->m_Emitters[index]->m_IsSpawning);

    dmParticle::DestroyEmitter(m_Context, emitter);
}

TEST_F(ParticleTest, StartLoopEmitter)
{
    float dt = 1.0f / 60.0f;

    ASSERT_TRUE(LoadPrototype("loop.particlefxc", &m_Prototype));
    dmParticle::HEmitter emitter = dmParticle::CreateEmitter(m_Context, &m_Prototype);
    uint16_t index = emitter & 0xffff;

    dmParticle::StartEmitter(m_Context, emitter);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0);

    ASSERT_EQ(1U, m_Context->m_Emitters[index]->m_IsSpawning);

    dmParticle::DestroyEmitter(m_Context, emitter);
}

TEST_F(ParticleTest, FireAndForget)
{
    float dt = 1.0f / 60.0f;

    ASSERT_TRUE(LoadPrototype("once.particlefxc", &m_Prototype));
    dmParticle::FireAndForget(m_Context, &m_Prototype, Vectormath::Aos::Point3(0.0f, 0.0f, 0.0f), Vectormath::Aos::Quat::identity());

    // counting on 0
    uint16_t index = 0;

    ASSERT_EQ(1U, m_Context->m_EmitterIndexPool.Size());
    ASSERT_EQ(1U, m_Context->m_Emitters[index]->m_IsSpawning);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0);

    ASSERT_NE((void*)0, (void*)m_Context->m_Emitters[index]);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0);

    ASSERT_NE((void*)0, (void*)m_Context->m_Emitters[index]);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0);

    ASSERT_EQ((void*)0, (void*)m_Context->m_Emitters[index]);
    ASSERT_EQ(0U, m_Context->m_EmitterIndexPool.Size());
}

TEST_F(ParticleTest, EmissionSpace)
{
    float dt = 1.0f / 60.0f;

    // Test world space

    ASSERT_TRUE(LoadPrototype("once.particlefxc", &m_Prototype));
    dmParticle::HEmitter emitter = dmParticle::CreateEmitter(m_Context, &m_Prototype);
    uint16_t index = emitter & 0xffff;

    dmParticle::SetPosition(m_Context, emitter, Vectormath::Aos::Point3(10.0f, 0.0f, 0.0f));
    dmParticle::StartEmitter(m_Context, emitter);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0);

    ASSERT_LT(0.0f, m_Context->m_Emitters[index]->m_Particles[0].m_TimeLeft);
    ASSERT_EQ(10.0f, m_Context->m_Emitters[index]->m_Particles[0].m_Position.getX());

    dmParticle::DestroyEmitter(m_Context, emitter);

    // Test emitter space

    ASSERT_TRUE(LoadPrototype("emitter_space.particlefxc", &m_Prototype));
    emitter = dmParticle::CreateEmitter(m_Context, &m_Prototype);
    index = emitter & 0xffff;

    dmParticle::SetPosition(m_Context, emitter, Vectormath::Aos::Point3(10.0f, 0.0f, 0.0f));
    dmParticle::StartEmitter(m_Context, emitter);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0);

    ASSERT_LT(0.0f, m_Context->m_Emitters[index]->m_Particles[0].m_TimeLeft);
    ASSERT_EQ(0.0f, m_Context->m_Emitters[index]->m_Particles[0].m_Position.getX());

    dmParticle::DestroyEmitter(m_Context, emitter);
}

TEST_F(ParticleTest, RestartEmitter)
{
    float dt = 1.0f / 60.0f;

    ASSERT_TRUE(LoadPrototype("restart.particlefxc", &m_Prototype));
    dmParticle::HEmitter emitter = dmParticle::CreateEmitter(m_Context, &m_Prototype);
    uint16_t index = emitter & 0xffff;

    dmParticle::StartEmitter(m_Context, emitter);

    ASSERT_EQ(1U, m_Context->m_Emitters[index]->m_IsSpawning);

    ASSERT_GE(0.0f, m_Context->m_Emitters[index]->m_Particles[0].m_TimeLeft);
    ASSERT_GE(0.0f, m_Context->m_Emitters[index]->m_Particles[1].m_TimeLeft);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0);

    ASSERT_LT(0.0f, m_Context->m_Emitters[index]->m_Particles[0].m_TimeLeft);
    ASSERT_GE(0.0f, m_Context->m_Emitters[index]->m_Particles[1].m_TimeLeft);

    dmParticle::RestartEmitter(m_Context, emitter);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0);

    ASSERT_GT(0.0f, m_Context->m_Emitters[index]->m_Particles[0].m_TimeLeft);
    ASSERT_LT(0.0f, m_Context->m_Emitters[index]->m_Particles[1].m_TimeLeft);

    dmParticle::DestroyEmitter(m_Context, emitter);
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
    dmParticle::HEmitter emitter = dmParticle::CreateEmitter(m_Context, &m_Prototype);
    uint16_t index = emitter & 0xffff;
    dmParticle::Emitter* e = m_Context->m_Emitters[index];

    dmParticle::StartEmitter(m_Context, emitter);
    dmParticle::Particle* particle = &e->m_Particles[0];

    ASSERT_GE(0.0f, particle->m_TimeLeft);

    // t = 0, size = 0
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0);
    ASSERT_DOUBLE_EQ(0.0f, particle->m_Size);

    // t = 0.125, size < 0
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0);
    ASSERT_GT(0.0f, particle->m_Size);

    // t = 0.25, size = 0
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0);
    ASSERT_DOUBLE_EQ(0.0f, particle->m_Size);

    // t = 0.375, size > 0
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0);
    ASSERT_LT(0.0f, particle->m_Size);

    // t = 0.5, size = 1
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0);
    ASSERT_DOUBLE_EQ(1.0f, particle->m_Size);

    // t = 0.625, size > 0
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0);
    ASSERT_LT(0.0f, particle->m_Size);

    // t = 0.75, size = 0
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0);
    ASSERT_DOUBLE_EQ(0.0f, particle->m_Size);

    // t = 0.875, size < 0
    // Updating with a full dt here will make the emitter reach its duration
    float epsilon = 0.000001f;
    dmParticle::Update(m_Context, dt - epsilon, m_VertexBuffer, m_VertexBufferSize, 0x0);
    ASSERT_GT(0.0f, particle->m_Size);

    // t = 1, size = 0
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0);
    ASSERT_NEAR(0.0f, particle->m_Size, epsilon);

    dmParticle::DestroyEmitter(m_Context, emitter);
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
    dmParticle::HEmitter emitter = dmParticle::CreateEmitter(m_Context, &m_Prototype);
    uint16_t index = emitter & 0xffff;
    dmParticle::Emitter* e = m_Context->m_Emitters[index];

    dmParticle::StartEmitter(m_Context, emitter);
    dmParticle::Particle* particle = &e->m_Particles[0];

    ASSERT_GE(0.0f, particle->m_TimeLeft);

    // t = 0, size = 0
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0);
    ASSERT_DOUBLE_EQ(0.0f, particle->m_Size);

    // t = 0.125, size < 0
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0);
    ASSERT_GT(0.0f, particle->m_Size);

    // t = 0.25, size = 0
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0);
    ASSERT_DOUBLE_EQ(0.0f, particle->m_Size);

    // t = 0.375, size > 0
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0);
    ASSERT_LT(0.0f, particle->m_Size);

    // t = 0.5, size = 1
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0);
    ASSERT_DOUBLE_EQ(1.0f, particle->m_Size);

    // t = 0.625, size > 0
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0);
    ASSERT_LT(0.0f, particle->m_Size);

    // t = 0.75, size = 0
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0);
    ASSERT_DOUBLE_EQ(0.0f, particle->m_Size);

    // t = 0.875, size < 0
    // Updating with a full dt here will make the emitter reach its duration
    float epsilon = 0.000001f;
    dmParticle::Update(m_Context, dt - epsilon, m_VertexBuffer, m_VertexBufferSize, 0x0);
    ASSERT_GT(0.0f, particle->m_Size);

    // t = 1, size = 0
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0);
    ASSERT_NEAR(0.0f, particle->m_Size, epsilon);

    dmParticle::DestroyEmitter(m_Context, emitter);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
