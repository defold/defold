package com.dynamo.cr.parted.nodes;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.List;

import javax.media.opengl.GL2;
import javax.vecmath.Point3d;
import javax.vecmath.Quat4d;
import javax.vecmath.Vector4d;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.bob.util.MathUtil;
import com.dynamo.cr.go.core.ComponentTypeNode;
import com.dynamo.cr.parted.ParticleLibrary;
import com.dynamo.cr.parted.ParticleLibrary.AnimationData;
import com.dynamo.cr.parted.ParticleLibrary.FetchAnimationCallback;
import com.dynamo.cr.parted.ParticleLibrary.Quat;
import com.dynamo.cr.parted.ParticleLibrary.Vector3;
import com.dynamo.cr.parted.ParticleLibrary.Vector4;
import com.dynamo.cr.sceneed.core.INodeLoader;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.tileeditor.scene.TextureSetNode;
import com.dynamo.particle.proto.Particle.Emitter;
import com.dynamo.particle.proto.Particle.Modifier;
import com.dynamo.particle.proto.Particle.ParticleFX;
import com.dynamo.textureset.proto.TextureSetProto.TextureSetAnimation;
import com.google.protobuf.Message;
import com.jogamp.common.nio.Buffers;
import com.sun.jna.Pointer;
import com.sun.jna.ptr.IntByReference;

public class ParticleFXNode extends ComponentTypeNode {
    public static final int VERTEX_COMPONENT_COUNT = 9;

    private static Logger logger = LoggerFactory.getLogger(ParticleFXNode.class);

    private static final long serialVersionUID = 1L;

    private static final FetchAnimCallback animCallback = new FetchAnimCallback();

    private static class FetchAnimCallback implements FetchAnimationCallback {
        private List<Node> children;

        @Override
        public int invoke(Pointer tileSource, final long hash, AnimationData data) {
            int emitterIndex = (int)Pointer.nativeValue(tileSource);
            // emitterIndex is coded as index + 1
            if (emitterIndex == 0) {
                return ParticleLibrary.FetchAnimationResult.FETCH_ANIMATION_NOT_FOUND;
            }
            --emitterIndex;
            EmitterNode emitterNode = (EmitterNode)children.get(emitterIndex);
            TextureSetNode textureSetNode = emitterNode.getTextureSetNode();
            if (textureSetNode == null) {
                return ParticleLibrary.FetchAnimationResult.FETCH_ANIMATION_NOT_FOUND;
            }

            TextureSetAnimation animation = textureSetNode.getRuntimeTextureSet().getAnimation(new Comparable<String>() {

                @Override
                public int compareTo(String s) {
                    return (int) (ParticleLibrary.Particle_Hash(s) - hash);
                }
            });

            if (animation != null) {
                data.texture = new Pointer(emitterIndex + 1);
                data.texCoords = textureSetNode.getRuntimeTextureSet().getTexCoords().asFloatBuffer();
                data.texDims = textureSetNode.getRuntimeTextureSet().getTexDims().asFloatBuffer();
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
                case PLAYBACK_ONCE_PINGPONG:
                    data.playback = ParticleLibrary.AnimPlayback.ANIM_PLAYBACK_ONCE_PINGPONG;
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
                data.tileWidth = (int) animation.getWidth();
                data.tileHeight = (int) animation.getHeight();
                data.startTile = animation.getStart();
                data.endTile = animation.getEnd();
                data.fps = animation.getFps();
                data.hFlip = animation.getFlipHorizontal();
                data.vFlip = animation.getFlipVertical();
                data.structSize = data.size();
                return ParticleLibrary.FetchAnimationResult.FETCH_ANIMATION_OK;
            }
            return ParticleLibrary.FetchAnimationResult.FETCH_ANIMATION_NOT_FOUND;
        }
    }

    private transient Pointer prototype;
    private transient Pointer instance;
    private transient Pointer context;
    private transient boolean reload = false;
    private transient boolean forceReplay = false;
    private transient ByteBuffer vertexBuffer;
    private transient int vertexBufferSize;
    private transient int maxParticleCount = 0;
    private transient double elapsedTime = 0.0f;
    private transient boolean running = false;

    public ParticleFXNode() {
    }

    public ParticleFXNode(Vector4d translation, Quat4d rotation) {
        super();
        setTranslation(new Point3d(translation.getX(), translation.getY(), translation.getZ()));
        setRotation(rotation);
    }

    @Override
    public void dispose(GL2 gl) {
       super.dispose(gl);
       if (this.context != null) {
           ParticleLibrary.Particle_DestroyInstance(context, instance);
           ParticleLibrary.Particle_DeletePrototype(prototype);
       }
    }

    @Override
    public void parentSet() {
        setTransformable(getParent() != null);
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
        if (Pointer.nativeValue(prototype) == 0) {
            unbindContext();
            logger.error("Could not create a particle FX prototype.");
            return;
        }
        updateTileSources();

        animCallback.children = getChildren();
        instance = ParticleLibrary.Particle_CreateInstance(context, prototype, null);
        if (Pointer.nativeValue(instance) == 0) {
            unbindContext();
            logger.error("Could not create a particle FX instance.");
            return;
        }
        ParticleLibrary.Particle_SetPosition(context, instance, new Vector3(0, 0, 0));
        ParticleLibrary.Particle_SetRotation(context, instance, new Quat(0, 0, 0, 1));
        ParticleLibrary.Particle_SetScale(context, instance, 1);
    }

    public void unbindContext() {
        if (instance != null) {
            ParticleLibrary.Particle_DestroyInstance(context, instance);
            instance = null;
        }
        if (prototype != null) {
            ParticleLibrary.Particle_DeletePrototype(prototype);
            prototype = null;
        }
        this.context = null;
    }

    public Pointer getContext() {
        return this.context;
    }

    public Pointer getInstance() {
        return instance;
    }

    public ByteBuffer getVertexBuffer() {
        return this.vertexBuffer;
    }

    public int getVertexBufferSize() {
        return this.vertexBufferSize;
    }

    public double getElapsedTime() {
        return this.elapsedTime;
    }

    @Override
    protected void childAdded(Node child) {
        super.childAdded(child);
        reload(true);
    }

    @Override
    protected void childRemoved(Node child) {
        super.childRemoved(child);
        reload(true);
    }

    private byte[] toByteArray() {
        ISceneModel model = getModel();
        INodeLoader<Node> nodeLoader = model.getNodeLoader(ParticleFXNode.class);
        if (nodeLoader != null) {
            Message msg;
            try {
                msg = nodeLoader.buildMessage(model.getLoaderContext(), this, new NullProgressMonitor());
                // Reconstruct message so:
                // * all emitters contain the instance modifiers
                // * rotation properties are expressed in radians and not degrees
                ParticleFX.Builder builder = ParticleFX.newBuilder((ParticleFX)msg);
                List<Modifier> modifiers = builder.getModifiersList();
                builder.clearModifiers();
                int emitterCount = builder.getEmittersCount();
                for (int i = 0; i < emitterCount; ++i) {
                    Emitter.Builder eb = Emitter.newBuilder(builder.getEmitters(i));
                    Point3d ep = MathUtil.ddfToVecmath(eb.getPosition());
                    Quat4d er = MathUtil.ddfToVecmath(eb.getRotation());
                    for (Modifier modifier : modifiers) {
                        Modifier.Builder mb = Modifier.newBuilder(modifier);
                        Point3d p = MathUtil.ddfToVecmath(modifier.getPosition());
                        Quat4d r = MathUtil.ddfToVecmath(modifier.getRotation());
                        MathUtil.invTransform(ep, er, p);
                        mb.setPosition(MathUtil.vecmathToDDF(p));
                        MathUtil.invTransform(er, r);
                        mb.setRotation(MathUtil.vecmathToDDF(r));
                        eb.addModifiers(mb.build());
                    }
                    builder.setEmitters(i, eb);
                }
                msg = builder.build();

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
        List<Node> children = getChildren();
        int childCount = children.size();
        int emitterIndex = 0;
        for (int i = 0; i < childCount; ++i) {
            Node n = children.get(i);
            if (n instanceof EmitterNode) {
                // Fake pointer as index + 1 (avoid 0x0)
                ParticleLibrary.Particle_SetTileSource(prototype, emitterIndex++, new Pointer(i + 1));
            }
        }
    }

    private void doReload(boolean running) {
        if (context == null || !reload) {
            return;
        }
        byte[] data = toByteArray();
        if (data != null) {
            ParticleLibrary.Particle_ReloadPrototype(prototype, ByteBuffer.wrap(data), data.length);
            updateTileSources();
            boolean replay = this.forceReplay || !running;
            ParticleLibrary.Particle_ReloadInstance(this.context, this.instance, replay);
        }
        this.reload = false;
        this.forceReplay = false;
    }

    public void simulate(double dt) {
        if (this.context == null) {
            return;
        }
        int maxParticleCount = ParticleLibrary.Particle_GetContextMaxParticleCount(this.context);
        if (maxParticleCount != this.maxParticleCount) {
            this.maxParticleCount = maxParticleCount;
            this.vertexBuffer = Buffers.newDirectByteBuffer(ParticleLibrary.Particle_GetVertexBufferSize(this.maxParticleCount, 0));
        }

        boolean running = dt > 0.0;
        if (!this.running && running) {
            reset();
        }
        this.running = running;
        doReload(this.running);
        IntByReference outSize = new IntByReference(0);

        if (ParticleLibrary.Particle_IsSleeping(context, this.instance)) {
            reset();
            ParticleLibrary.Particle_StartInstance(context, this.instance);
        }
        animCallback.children = getChildren();
        
        ParticleLibrary.Particle_Update(context, (float) dt, animCallback);
        List<Node> children = getChildren();
        int childCount = children.size();
        int emitterIndex = 0;
        for (int i = 0; i < childCount; ++i) {
            Node n = children.get(i);
            if (n instanceof EmitterNode) {
                ParticleLibrary.Particle_GenerateVertexData(context, (float) dt, this.instance, emitterIndex, new Vector4(1,1,1,1), this.vertexBuffer, this.vertexBuffer.capacity(), outSize, 0);
                ++emitterIndex;
            }
        }
        this.vertexBufferSize = outSize.getValue();

        this.elapsedTime += dt;
    }

    public void reset() {
        this.elapsedTime = 0.0f;
        ParticleLibrary.Particle_ResetInstance(context, instance);
    }

    public void reload(boolean forceReplay) {
        this.reload = true;
        this.forceReplay = forceReplay;
    }

}
