#include <gtest/gtest.h>
#include <stdio.h>

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
        m_VertexBufferSize = 1024 * 4 * 6 * sizeof(float);
        m_VertexBuffer = new float[m_VertexBufferSize];
        m_Prototype = 0x0;
    }

    virtual void TearDown()
    {
        if (m_Prototype != 0x0)
        {
            dmParticle::Particle_DeletePrototype(m_Context, m_Prototype);
        }
        dmParticle::DestroyContext(m_Context);
        delete [] m_VertexBuffer;
    }

    dmParticle::HContext m_Context;
    dmParticle::HPrototype m_Prototype;
    float* m_VertexBuffer;
    uint32_t m_VertexBufferSize;
};

bool LoadPrototype(dmParticle::HContext context, const char* filename, dmParticle::HPrototype* prototype)
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
        *prototype = dmParticle::Particle_NewPrototype(context, buffer, file_size);
        fclose(f);
        return *prototype != 0x0;
    }
    else
    {
        dmLogWarning("Particle FX could not be loaded: %s.", path);
        return false;
    }
}

TEST_F(ParticleTest, CreationSuccess)
{
    ASSERT_TRUE(LoadPrototype(m_Context, "once.particlefxc", &m_Prototype));
    dmParticle::HInstance instance = dmParticle::CreateInstance(m_Context, m_Prototype);
    ASSERT_NE(dmParticle::INVALID_INSTANCE, instance);
    ASSERT_EQ(1U, m_Context->m_InstanceIndexPool.Size());
    dmParticle::DestroyInstance(m_Context, instance);
    ASSERT_EQ(0U, m_Context->m_InstanceIndexPool.Size());
}

TEST_F(ParticleTest, CreationFailure)
{
    ASSERT_FALSE(LoadPrototype(m_Context, "null.particlefxc", &m_Prototype));
}

TEST_F(ParticleTest, StartInstance)
{
    float dt = 1.0f / 60.0f;

    ASSERT_TRUE(LoadPrototype(m_Context, "once.particlefxc", &m_Prototype));
    dmParticle::HInstance instance = dmParticle::CreateInstance(m_Context, m_Prototype);
    uint16_t index = instance & 0xffff;

    dmParticle::Instance* i = m_Context->m_Instances[index];
    ASSERT_NE((void*)0, (void*)i);
    ASSERT_FALSE(dmParticle::IsSpawning(m_Context, instance));
    ASSERT_EQ(1U, i->m_Emitters[0].m_Particles.Size());

    dmParticle::StartInstance(m_Context, instance);

    ASSERT_TRUE(dmParticle::IsSpawning(m_Context, instance));

    uint32_t out_vertex_buffer_size;
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, &out_vertex_buffer_size);

    i = m_Context->m_Instances[index];
    dmParticle::Emitter* e = &i->m_Emitters[0];
    ASSERT_NE((void*)0, (void*)i);
    ASSERT_LT(0.0f, e->m_Particles[0].m_TimeLeft);
    ASSERT_TRUE(dmParticle::IsSpawning(m_Context, instance));
    ASSERT_GT(e->m_Prototype->m_DDF->m_Duration, e->m_Timer);
    ASSERT_EQ(6 * 6 * sizeof(float), out_vertex_buffer_size); // 6 vertices of 6 floats (pos, uv, alpha)

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0);

    i = m_Context->m_Instances[index];
    e = &i->m_Emitters[0];
    ASSERT_NE((void*)0, (void*)i);
    ASSERT_GT(0.0f, e->m_Particles[0].m_TimeLeft);
    ASSERT_EQ(e->m_ParticleTimeLeft, e->m_Particles[0].m_TimeLeft);
    ASSERT_FALSE(dmParticle::IsSpawning(m_Context, instance));
    ASSERT_LT(e->m_Prototype->m_DDF->m_Duration, e->m_Timer);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0);

    ASSERT_LT(e->m_Prototype->m_DDF->m_Duration, e->m_Timer);
    ASSERT_FALSE(dmParticle::IsSpawning(m_Context, instance));

    dmParticle::DestroyInstance(m_Context, instance);
}

TEST_F(ParticleTest, StartOnceInstance)
{
    float dt = 1.0f / 60.0f;

    ASSERT_TRUE(LoadPrototype(m_Context, "once.particlefxc", &m_Prototype));
    dmParticle::HInstance instance = dmParticle::CreateInstance(m_Context, m_Prototype);
    uint16_t index = instance & 0xffff;

    dmParticle::StartInstance(m_Context, instance);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0);

    dmParticle::Emitter* e = &m_Context->m_Instances[index]->m_Emitters[0];
    ASSERT_LT(e->m_Prototype->m_DDF->m_Duration, e->m_Timer);
    ASSERT_FALSE(dmParticle::IsSpawning(m_Context, instance));

    dmParticle::DestroyInstance(m_Context, instance);
}

TEST_F(ParticleTest, StartLoopInstance)
{
    float dt = 1.0f / 60.0f;

    ASSERT_TRUE(LoadPrototype(m_Context, "loop.particlefxc", &m_Prototype));
    dmParticle::HInstance instance = dmParticle::CreateInstance(m_Context, m_Prototype);

    dmParticle::StartInstance(m_Context, instance);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0);

    ASSERT_TRUE(dmParticle::IsSpawning(m_Context, instance));

    dmParticle::DestroyInstance(m_Context, instance);
}

TEST_F(ParticleTest, FireAndForget)
{
    float dt = 1.0f / 60.0f;

    ASSERT_TRUE(LoadPrototype(m_Context, "once.particlefxc", &m_Prototype));
    dmParticle::FireAndForget(m_Context, m_Prototype, Vectormath::Aos::Point3(0.0f, 0.0f, 0.0f), Vectormath::Aos::Quat::identity());

    // counting on 0
    uint16_t index = 0;

    dmParticle::Instance* i = m_Context->m_Instances[index];

    ASSERT_EQ(1U, m_Context->m_InstanceIndexPool.Size());
    ASSERT_TRUE(dmParticle::IsSpawning(m_Context, i->m_VersionNumber << 16));

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0);

    ASSERT_NE((void*)0, (void*)m_Context->m_Instances[index]);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0);

    ASSERT_NE((void*)0, (void*)m_Context->m_Instances[index]);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0);

    ASSERT_EQ((void*)0, (void*)m_Context->m_Instances[index]);
    ASSERT_EQ(0U, m_Context->m_InstanceIndexPool.Size());
}

TEST_F(ParticleTest, EmissionSpace)
{
    float dt = 1.0f / 60.0f;

    // Test world space

    ASSERT_TRUE(LoadPrototype(m_Context, "once.particlefxc", &m_Prototype));
    dmParticle::HInstance instance = dmParticle::CreateInstance(m_Context, m_Prototype);
    uint16_t index = instance & 0xffff;

    dmParticle::SetPosition(m_Context, instance, Vectormath::Aos::Point3(10.0f, 0.0f, 0.0f));
    dmParticle::StartInstance(m_Context, instance);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0);

    dmParticle::Emitter* e = &m_Context->m_Instances[index]->m_Emitters[0];
    ASSERT_LT(0.0f, e->m_Particles[0].m_TimeLeft);
    ASSERT_EQ(10.0f, e->m_Particles[0].m_Position.getX());

    dmParticle::DestroyInstance(m_Context, instance);

    // Test emitter space

    dmParticle::Particle_DeletePrototype(m_Context, m_Prototype);
    ASSERT_TRUE(LoadPrototype(m_Context, "emitter_space.particlefxc", &m_Prototype));
    instance = dmParticle::CreateInstance(m_Context, m_Prototype);
    index = instance & 0xffff;

    dmParticle::SetPosition(m_Context, instance, Vectormath::Aos::Point3(10.0f, 0.0f, 0.0f));
    dmParticle::StartInstance(m_Context, instance);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0);

    e = &m_Context->m_Instances[index]->m_Emitters[0];
    ASSERT_LT(0.0f, e->m_Particles[0].m_TimeLeft);
    ASSERT_EQ(0.0f, e->m_Particles[0].m_Position.getX());

    dmParticle::DestroyInstance(m_Context, instance);
}

TEST_F(ParticleTest, RestartInstance)
{
    float dt = 1.0f / 60.0f;

    ASSERT_TRUE(LoadPrototype(m_Context, "restart.particlefxc", &m_Prototype));
    dmParticle::HInstance instance = dmParticle::CreateInstance(m_Context, m_Prototype);
    uint16_t index = instance & 0xffff;

    dmParticle::StartInstance(m_Context, instance);

    ASSERT_TRUE(dmParticle::IsSpawning(m_Context, instance));

    dmParticle::Emitter* e = &m_Context->m_Instances[index]->m_Emitters[0];
    ASSERT_GE(0.0f, e->m_Particles[0].m_TimeLeft);
    ASSERT_GE(0.0f, e->m_Particles[1].m_TimeLeft);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0);

    ASSERT_LT(0.0f, e->m_Particles[0].m_TimeLeft);
    ASSERT_GE(0.0f, e->m_Particles[1].m_TimeLeft);

    dmParticle::RestartInstance(m_Context, instance);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0);

    ASSERT_GT(0.0f, e->m_Particles[0].m_TimeLeft);
    ASSERT_LT(0.0f, e->m_Particles[1].m_TimeLeft);

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

    ASSERT_TRUE(LoadPrototype(m_Context, "emitter_spline.particlefxc", &m_Prototype));
    dmParticle::HInstance instance = dmParticle::CreateInstance(m_Context, m_Prototype);
    uint16_t index = instance & 0xffff;
    dmParticle::Instance* i = m_Context->m_Instances[index];

    dmParticle::StartInstance(m_Context, instance);
    dmParticle::Particle* particle = &i->m_Emitters[0].m_Particles[0];

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

    ASSERT_TRUE(LoadPrototype(m_Context, "particle_spline.particlefxc", &m_Prototype));
    dmParticle::HInstance instance = dmParticle::CreateInstance(m_Context, m_Prototype);
    uint16_t index = instance & 0xffff;
    dmParticle::Instance* i = m_Context->m_Instances[index];

    dmParticle::StartInstance(m_Context, instance);
    dmParticle::Particle* particle = &i->m_Emitters[0].m_Particles[0];

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

    dmParticle::DestroyInstance(m_Context, instance);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
