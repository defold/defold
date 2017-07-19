package com.dynamo.cr.parted.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.FloatBuffer;

import javax.vecmath.Matrix4d;
import javax.vecmath.Vector3d;

import org.eclipse.swt.widgets.Display;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.parted.ParticleLibrary;
import com.dynamo.cr.parted.ParticleLibrary.AnimationData;
import com.dynamo.cr.parted.ParticleLibrary.FetchAnimationCallback;
import com.dynamo.cr.parted.ParticleLibrary.InstanceStats;
import com.dynamo.cr.parted.ParticleLibrary.Matrix4;
import com.dynamo.cr.parted.ParticleLibrary.Quat;
import com.dynamo.cr.parted.ParticleLibrary.RenderInstanceCallback;
import com.dynamo.cr.parted.ParticleLibrary.Stats;
import com.dynamo.cr.parted.ParticleLibrary.Vector3;
import com.dynamo.cr.parted.ParticleLibrary.Vector4;
import com.dynamo.particle.proto.Particle.BlendMode;
import com.dynamo.particle.proto.Particle.EmissionSpace;
import com.dynamo.particle.proto.Particle.Emitter;
import com.dynamo.particle.proto.Particle.EmitterKey;
import com.dynamo.particle.proto.Particle.EmitterType;
import com.dynamo.particle.proto.Particle.ParticleFX;
import com.dynamo.particle.proto.Particle.PlayMode;
import com.dynamo.particle.proto.Particle.SizeMode;
import com.dynamo.particle.proto.Particle.SplinePoint;
import com.jogamp.common.nio.Buffers;
import com.sun.jna.Pointer;
import com.sun.jna.ptr.IntByReference;

public class ParticleSystemTest {

    private Pointer context;

    private static final int MAX_PARTICLE_COUNT = 1024;

    private static final int PARTICLE_VERTEX_COUNT = 6;

    @Before
    public void setUp() throws Exception {
        // Avoid hang when running unit-test on Mac OSX
        // Related to SWT and threads?
        if (System.getProperty("os.name").toLowerCase().indexOf("mac") != -1) {
            Display.getDefault();
        }

        context = ParticleLibrary.Particle_CreateContext(32, MAX_PARTICLE_COUNT);
    }

    @After
    public void tearDown() {
        ParticleLibrary.Particle_DestroyContext(context);
    }

    @Test
    public void smokeTest() throws IOException {
        Emitter.Property.Builder pb_life_time = Emitter.Property.newBuilder()
                .setKey(EmitterKey.EMITTER_KEY_PARTICLE_LIFE_TIME)
                .addPoints(SplinePoint.newBuilder()
                        .setX(0.0f)
                        .setY(1.0f)
                        .setTX(1.0f)
                        .setTY(0.0f))
                .addPoints(SplinePoint.newBuilder()
                        .setX(1.0f)
                        .setY(1.0f)
                        .setTX(1.0f)
                        .setTY(0.0f));
        Emitter.Property.Builder pb_spawn_rate = Emitter.Property.newBuilder()
                .setKey(EmitterKey.EMITTER_KEY_SPAWN_RATE)
                .addPoints(SplinePoint.newBuilder()
                        .setX(0.0f)
                        .setY(60.0f)
                        .setTX(1.0f)
                        .setTY(0.0f));
        Emitter.Builder eb = Emitter.newBuilder()
                .setSizeMode(SizeMode.SIZE_MODE_AUTO)
                .setMode(PlayMode.PLAY_MODE_ONCE)
                .setSpace(EmissionSpace.EMISSION_SPACE_WORLD)
                .setPosition(com.dynamo.proto.DdfMath.Point3.newBuilder().setX(1).setY(2).setZ(3).build())
                .setRotation(com.dynamo.proto.DdfMath.Quat.newBuilder().setW(1).build())
                .setTileSource("foo")
                .setAnimation("anim")
                .setMaterial("test")
                .setBlendMode(BlendMode.BLEND_MODE_MULT)
                .setMaxParticleCount(1)
                .setDuration(1.0f)
                .setType(EmitterType.EMITTER_TYPE_SPHERE)
                .addProperties(pb_life_time)
                .addProperties(pb_spawn_rate);
        ParticleFX.Builder pfxb = ParticleFX.newBuilder()
                .addEmitters(eb);

        byte[] pfxData = pfxb.build().toByteArray();

        Pointer prototype = ParticleLibrary.Particle_NewPrototype(ByteBuffer.wrap(pfxData), pfxData.length);
        Pointer instance = ParticleLibrary.Particle_CreateInstance(context, prototype, null);
        assertNotNull(instance);
        ParticleLibrary.Particle_SetPosition(context, instance, new Vector3(1, 2, 3));
        ParticleLibrary.Particle_SetRotation(context, instance, new Quat(0, 0, 0, 1));

        ParticleLibrary.Particle_StartInstance(context, instance);

        final Pointer originalMaterial = new Pointer(1);
        ParticleLibrary.Particle_SetMaterial(prototype, 0, originalMaterial);
        final Pointer originalTileSource = new Pointer(2);
        ParticleLibrary.Particle_SetTileSource(prototype, 0, originalTileSource);
        final Pointer originalTexture = new Pointer(3);
        final FloatBuffer texCoords = Buffers.newDirectFloatBuffer(8);
        texCoords.put(1.0f/255.0f).put(4.0f/255.0f)
            .put(1.0f/255.0f).put(2.0f/255.0f)
            .put(3.0f/255.0f).put(2.0f/255.0f)
            .put(3.0f/255.0f).put(4.0f/255.0f);
        final FloatBuffer texDims = Buffers.newDirectFloatBuffer(2);
        texDims.put(1.0f).put(1.0f);
        IntByReference outSize = new IntByReference(0);
        final int vertexBufferSize = ParticleLibrary.Particle_GetVertexBufferSize(MAX_PARTICLE_COUNT, 0);
        final ByteBuffer vertexBuffer = Buffers.newDirectByteBuffer(vertexBufferSize);
        final boolean fetchAnim[] = new boolean[] { false };
        ParticleLibrary.Particle_Update(context, 1.0f / 60.0f,
                new FetchAnimationCallback() {

                    @Override
                    public int invoke(Pointer tileSource, long hash, AnimationData data) {
                        assertTrue(tileSource.equals(originalTileSource));
                        long h = ParticleLibrary.Particle_Hash("anim");
                        assertTrue(hash == h);
                        data.texCoords = texCoords;
                        data.texDims = texDims;
                        data.texture = originalTexture;
                        data.playback = ParticleLibrary.AnimPlayback.ANIM_PLAYBACK_ONCE_FORWARD;
                        data.startTile = 0;
                        data.endTile = 0;
                        data.fps = 30;
                        data.hFlip = 0;
                        data.vFlip = 0;
                        fetchAnim[0] = true;
                        data.structSize = data.size();
                        return ParticleLibrary.FetchAnimationResult.FETCH_ANIMATION_OK;
                    }
                });
        assertTrue(fetchAnim[0]);
        
        Vector4 color = new Vector4(1,1,1,1);
        ParticleLibrary.Particle_GenerateVertexData(context, 0.0f, instance, 0, color, vertexBuffer, ParticleLibrary.Particle_GetVertexBufferSize(1, 0), outSize, 0);
        int vertexSize = outSize.getValue();
        assertTrue(ParticleLibrary.Particle_GetVertexBufferSize(1, 0) == vertexSize);

        int uvIdx[] = new int[] {
                0, 1,
                2, 3,
                4, 5,
                4, 5,
                6, 7,
                0, 1
        };
        for (int i = 0; i < PARTICLE_VERTEX_COUNT; ++i) {
            // p
            assertTrue(2.0f == vertexBuffer.getFloat());
            assertTrue(4.0f == vertexBuffer.getFloat());
            assertTrue(6.0f == vertexBuffer.getFloat());
            // rgba
            assertTrue(0 == vertexBuffer.get());
            assertTrue(0 == vertexBuffer.get());
            assertTrue(0 == vertexBuffer.get());
            assertTrue(0 == vertexBuffer.get());
            // u
            int u = 0xffff & vertexBuffer.getShort();
            assertTrue(texCoords.get(uvIdx[i * 2 + 0]) == ((float)u)/65535.0f);
            // v
            int v = 0xffff & vertexBuffer.getShort();
            assertTrue(texCoords.get(uvIdx[i * 2 + 1]) == ((float)v)/65535.0f);
        }

        Stats stats = new Stats();
        InstanceStats instanceStats = new InstanceStats();
        ParticleLibrary.Particle_GetStats(context, stats);
        ParticleLibrary.Particle_GetInstanceStats(context, instance, instanceStats);

        assertEquals(1, stats.particles);
        assertEquals(1024, stats.maxParticles);
        assertEquals(1.0f / 60.0f, instanceStats.time, 0.0001f);

        final boolean rendered[] = new boolean[] { false };
        ParticleLibrary.Particle_RenderEmitter(context, instance, 0, new Pointer(1122), new RenderInstanceCallback() {
            @Override
            public void invoke(Pointer userContext, Pointer material,
                    Pointer texture, Matrix4 world, int blendMode, int vertexIndex, int vertexCount, Pointer constants, int constantCount) {
                assertTrue(material.equals(originalMaterial));
                assertTrue(texture.equals(originalTexture));
                Matrix4d worldTransform = world.toMatrix();
                Vector3d translation = new Vector3d();
                worldTransform.get(translation);
                assertEquals(2, translation.x, 0);
                assertEquals(4, translation.y, 0);
                assertEquals(6, translation.z, 0);
                assertEquals(new Pointer(1122), userContext);
                assertEquals(BlendMode.BLEND_MODE_MULT, BlendMode.valueOf(blendMode));
                rendered[0] = true;
            }
        });
        assertTrue(rendered[0]);

        ParticleLibrary.Particle_StopInstance(context, instance);
        ParticleLibrary.Particle_DestroyInstance(context, instance);
        ParticleLibrary.Particle_DeletePrototype(prototype);
    }

}

