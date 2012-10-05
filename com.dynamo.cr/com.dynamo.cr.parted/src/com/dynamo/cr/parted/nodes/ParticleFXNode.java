package com.dynamo.cr.parted.nodes;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.FloatBuffer;

import javax.vecmath.Quat4d;
import javax.vecmath.Vector4d;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.parted.ParticleLibrary;
import com.dynamo.cr.parted.ParticleLibrary.AnimationData;
import com.dynamo.cr.parted.ParticleLibrary.FetchAnimationCallback;
import com.dynamo.cr.parted.ParticleLibrary.Quat;
import com.dynamo.cr.parted.ParticleLibrary.Vector3;
import com.dynamo.cr.sceneed.core.INodeLoader;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.tileeditor.scene.AnimationNode;
import com.dynamo.cr.tileeditor.scene.TileSetNode;
import com.google.protobuf.Message;
import com.sun.jna.Pointer;
import com.sun.jna.ptr.IntByReference;
import com.sun.opengl.util.BufferUtil;

public class ParticleFXNode extends Node {
    public static final int VERTEX_COMPONENT_COUNT = 9;
    public static final int PARTICLE_VERTEX_COUNT = 6;

    private static Logger logger = LoggerFactory.getLogger(ParticleFXNode.class);

    private static final long serialVersionUID = 1L;

    private class FetchAnimCallback implements FetchAnimationCallback {
        @Override
        public int invoke(Pointer tileSource, long hash, AnimationData data) {
            int emitterIndex = (int)Pointer.nativeValue(tileSource);
            // emitterIndex is coded as index + 1
            if (emitterIndex == 0) {
                return ParticleLibrary.FetchAnimationResult.FETCH_ANIMATION_NOT_FOUND;
            }
            --emitterIndex;
            EmitterNode emitterNode = (EmitterNode)getChildren().get(emitterIndex);
            TileSetNode tileSetNode = emitterNode.getTileSetNode();
            if (tileSetNode == null) {
                return ParticleLibrary.FetchAnimationResult.FETCH_ANIMATION_NOT_FOUND;
            }
            for (AnimationNode animation : tileSetNode.getAnimations()) {
                long h = ParticleLibrary.Particle_Hash(animation.getId());
                if (h == hash) {
                    data.texture = new Pointer(emitterIndex + 1);
                    data.texCoords = tileSetNode.getTexCoords();
                    switch (animation.getPlayback()) {
                    case PLAYBACK_NONE:
                        data.playback = ParticleLibrary.AnimPlayback.ANIM_PLAYBACK_NONE;
                        break;
                    case PLAYBACK_ONCE_FORWARD:
                        data.playback = ParticleLibrary.AnimPlayback.ANIM_PLAYBACK_ONCE_FORWARD;
                        break;
                    case PLAYBACK_ONCE_BACKWARD:
                        data.playback = ParticleLibrary.AnimPlayback.ANIM_PLAYBACK_ONCE_BACKWARD;
                        break;
                    case PLAYBACK_LOOP_FORWARD:
                        data.playback = ParticleLibrary.AnimPlayback.ANIM_PLAYBACK_LOOP_FORWARD;
                        break;
                    case PLAYBACK_LOOP_BACKWARD:
                        data.playback = ParticleLibrary.AnimPlayback.ANIM_PLAYBACK_LOOP_BACKWARD;
                        break;
                    case PLAYBACK_LOOP_PINGPONG:
                        data.playback = ParticleLibrary.AnimPlayback.ANIM_PLAYBACK_LOOP_PINGPONG;
                        break;
                    }
                    data.startTile = animation.getStartTile();
                    data.endTile = animation.getEndTile();
                    data.fps = animation.getFps();
                    data.hFlip = animation.isFlipHorizontally() ? 1 : 0;
                    data.vFlip = animation.isFlipVertically() ? 1 : 0;
                    data.structSize = data.size();
                    return ParticleLibrary.FetchAnimationResult.FETCH_ANIMATION_OK;
                }
            }
            return ParticleLibrary.FetchAnimationResult.FETCH_ANIMATION_NOT_FOUND;
        }
    }

    private transient Pointer prototype;
    private transient Pointer instance;
    private transient Pointer context;
    private transient FetchAnimCallback animCallback = new FetchAnimCallback();
    private transient boolean reload = false;
    private transient FloatBuffer vertexBuffer;
    private transient int maxParticleCount = 0;

    public ParticleFXNode() {
    }

    public ParticleFXNode(Vector4d translation, Quat4d rotation) {
        super(translation, rotation);
    }

    @Override
    public void dispose() {
       super.dispose();
       if (instance != null) {
           ParticleLibrary.Particle_DestroyInstance(context, instance);
       }
       if (prototype != null) {
           ParticleLibrary.Particle_DeletePrototype(prototype);
       }
    }

    public void bindContext(Pointer context) {
        if (this.context != null) {
            throw new UnsupportedOperationException("A context can only be bound once.");
        }
        this.context = context;
        byte[] data = toByteArray();
        if (data == null) {
            return;
        }
        prototype = ParticleLibrary.Particle_NewPrototype(ByteBuffer.wrap(data), data.length);
        updateTileSources();

        instance = ParticleLibrary.Particle_CreateInstance(context, prototype);
        ParticleLibrary.Particle_SetPosition(context, instance, new Vector3(0, 0, 0));
        ParticleLibrary.Particle_SetRotation(context, instance, new Quat(0, 0, 0, 1));
    }

    public Pointer getContext() {
        return this.context;
    }

    public FloatBuffer getVertexBuffer() {
        return this.vertexBuffer;
    }

    @Override
    protected void childAdded(Node child) {
        super.childAdded(child);
        reload();
    }

    @Override
    protected void childRemoved(Node child) {
        super.childRemoved(child);
        reload();
    }

    private byte[] toByteArray() {
        ISceneModel model = getModel();
        INodeLoader<Node> nodeLoader = model.getNodeLoader(ParticleFXNode.class);
        if (nodeLoader != null) {
            Message msg;
            try {
                msg = nodeLoader.buildMessage(model.getLoaderContext(), this, new NullProgressMonitor());
                return msg.toByteArray();
            } catch (IOException e) {
                logger.error("Particle FX could not be serialized.", e);
            } catch (CoreException e) {
                logger.error("Particle FX could not be serialized.", e);
            }
        }
        return null;
    }

    private void updateTileSources() {
        int emitterCount = getChildren().size();
        for (int i = 0; i < emitterCount; ++i) {
            // Fake pointer as index + 1 (avoid 0x0)
            ParticleLibrary.Particle_SetTileSource(prototype, i, new Pointer(i + 1));
        }
    }

    private void doReload() {
        if (prototype == null || !reload) {
            return;
        }
        reload = false;
        byte[] data = toByteArray();
        if (data != null) {
            ParticleLibrary.Particle_ReloadPrototype(prototype, ByteBuffer.wrap(data), data.length);
            updateTileSources();
            ParticleLibrary.Particle_ReloadInstance(this.context, this.instance);
        }
    }

    public void simulate(double dt) {
        int maxParticleCount = ParticleLibrary.Particle_GetContextMaxParticleCount(this.context);
        if (maxParticleCount != this.maxParticleCount) {
            this.maxParticleCount = maxParticleCount;
            this.vertexBuffer = BufferUtil.newFloatBuffer(this.maxParticleCount * VERTEX_COMPONENT_COUNT * PARTICLE_VERTEX_COUNT);
        }

        doReload();
        IntByReference outSize = new IntByReference(0);

        if (dt > 0) {
            if (ParticleLibrary.Particle_IsSleeping(context, this.instance)) {
                ParticleLibrary.Particle_StartInstance(context, this.instance);
            }
            ParticleLibrary.Particle_Update(context, (float) dt, this.vertexBuffer, this.vertexBuffer.capacity(), outSize,
                    animCallback);

        } else {
            ParticleLibrary.Particle_ResetInstance(context, instance);
        }
    }

    public void reload() {
        this.reload = true;
    }

}
