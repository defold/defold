package com.dynamo.cr.parted;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.FloatBuffer;

import javax.vecmath.Quat4d;
import javax.vecmath.Vector4d;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

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

public class ParticleFXNode extends Node {

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
                    return ParticleLibrary.FetchAnimationResult.FETCH_ANIMATION_OK;
                }
            }
            return ParticleLibrary.FetchAnimationResult.FETCH_ANIMATION_NOT_FOUND;
        }
    }

    private transient Pointer prototype;
    private transient Pointer instance;
    private transient Pointer context;
    private transient boolean reset;
    private transient FetchAnimCallback animCallback = new FetchAnimCallback();
    private transient boolean reloadPrototype = false;

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

    @Override
    protected void childAdded(Node child) {
        super.childAdded(child);
        reloadPrototype();
    }

    private void createInstance(Pointer context) {
        if (instance != null && !reset) {
            // TODO: Recreation predicate
            return;
        }
        reset = false;

        if (instance != null) {
            ParticleLibrary.Particle_DeletePrototype(prototype);
            ParticleLibrary.Particle_DestroyInstance(context, instance);
        }

        if (this.context == null) {
            this.context = context;
        } else {
            if (this.context != context) {
                throw new IllegalArgumentException("createInstance called with different context");
            }
        }

        ParticleFXLoader loader = new ParticleFXLoader();
        Message msg;
        try {
            msg = loader.buildMessage(null, this, new NullProgressMonitor());
            byte[] pfxData = msg.toByteArray();
            prototype = ParticleLibrary.Particle_NewPrototype(ByteBuffer.wrap(pfxData), pfxData.length);
            int emitterCount = getChildren().size();
            for (int i = 0; i < emitterCount; ++i) {
                ParticleLibrary.Particle_SetTileSource(prototype, i, new Pointer(i));
            }
            instance = ParticleLibrary.Particle_CreateInstance(context, prototype);
            ParticleLibrary.Particle_SetPosition(context, instance, new Vector3(0, 0, 0));
            ParticleLibrary.Particle_SetRotation(context, instance, new Quat(0, 0, 0, 1));
            ParticleLibrary.Particle_StartInstance(context, instance);
        } catch (Exception e) {
            // Logging here might be dangerous (inside simulation loop)
            e.printStackTrace();
        }
    }

    private void updatePrototype() {
        ISceneModel model = getModel();
        if (model != null && prototype != null && reloadPrototype) {
            INodeLoader<Node> nodeLoader = model.getNodeLoader(ParticleFXNode.class);
            if (nodeLoader != null) {
                Message msg;
                try {
                    msg = nodeLoader.buildMessage(model.getLoaderContext(), this, new NullProgressMonitor());
                    byte[] pfxData = msg.toByteArray();
                    ParticleLibrary.Particle_ReloadPrototype(prototype, ByteBuffer.wrap(pfxData), pfxData.length);
                    int emitterCount = getChildren().size();
                    for (int i = 0; i < emitterCount; ++i) {
                        // Fake pointer as index + 1 (avoid 0x0)
                        ParticleLibrary.Particle_SetTileSource(prototype, i, new Pointer(i + 1));
                    }
                    reloadPrototype = false;
                } catch (IOException e) {
                    logger.error("Prototype could not be reloaded.", e);
                } catch (CoreException e) {
                    logger.error("Prototype could not be reloaded.", e);
                }
            }
        }
    }

    public void simulate(Pointer context, FloatBuffer vertexBuffer, double dt) {
        createInstance(context);
        updatePrototype();
        IntByReference outSize = new IntByReference(0);

        if (dt > 0) {
            ParticleLibrary.Particle_Update(context, (float) dt, vertexBuffer, vertexBuffer.capacity(), outSize,
                    animCallback);

        } else {
            ParticleLibrary.Particle_RestartInstance(context, instance);
        }
    }

    public void reset() {
        this.reset = true;
    }

    public void reloadPrototype() {
        this.reloadPrototype = true;
    }

}
