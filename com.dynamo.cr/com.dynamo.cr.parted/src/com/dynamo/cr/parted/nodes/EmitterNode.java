package com.dynamo.cr.parted.nodes;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.osgi.util.NLS;
import org.eclipse.swt.graphics.Image;

import com.dynamo.cr.parted.Messages;
import com.dynamo.cr.parted.ParticleEditorPlugin;
import com.dynamo.cr.parted.curve.HermiteSpline;
import com.dynamo.cr.properties.DynamicProperties;
import com.dynamo.cr.properties.DynamicPropertyAccessor;
import com.dynamo.cr.properties.IPropertyAccessor;
import com.dynamo.cr.properties.IPropertyDesc;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.cr.properties.descriptors.ValueSpreadPropertyDesc;
import com.dynamo.cr.properties.types.ValueSpread;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.util.LoaderUtil;
import com.dynamo.cr.tileeditor.scene.AnimationNode;
import com.dynamo.cr.tileeditor.scene.TileSetNode;
import com.dynamo.particle.proto.Particle;
import com.dynamo.particle.proto.Particle.EmissionSpace;
import com.dynamo.particle.proto.Particle.Emitter;
import com.dynamo.particle.proto.Particle.Emitter.ParticleProperty;
import com.dynamo.particle.proto.Particle.Emitter.Property.Builder;
import com.dynamo.particle.proto.Particle.BlendMode;
import com.dynamo.particle.proto.Particle.EmitterKey;
import com.dynamo.particle.proto.Particle.EmitterType;
import com.dynamo.particle.proto.Particle.Modifier;
import com.dynamo.particle.proto.Particle.ParticleKey;
import com.dynamo.particle.proto.Particle.PlayMode;
import com.dynamo.proto.DdfExtensions;

public class EmitterNode extends Node {

    private static final long serialVersionUID = 1L;

    private static IPropertyDesc<EmitterNode, ISceneModel>[] descriptors;
    private static EmitterKey[] emitterKeys;
    private static ParticleKey[] particleKeys;

    @Property(editorType = EditorType.DROP_DOWN, category = "Emitter")
    private PlayMode playMode;

    @Property(editorType = EditorType.DROP_DOWN)
    private EmissionSpace emissionSpace;

    @Property
    private float duration;

    @Property(editorType = EditorType.RESOURCE, extensions = { "tilesource", "tileset" })
    private String tileSource = "";

    private transient TileSetNode tileSetNode = null;

    @Property
    private String animation = "";

    @Property(editorType = EditorType.RESOURCE, extensions = { "material" })
    private String material = "";

    @Property(editorType = EditorType.DROP_DOWN)
    private BlendMode blendMode = BlendMode.BLEND_MODE_ALPHA;

    @Property
    private int maxParticleCount;

    @Property
    private EmitterType emitterType;

    private Map<EmitterKey, ValueSpread> properties = new HashMap<EmitterKey, ValueSpread>();
    private Map<ParticleKey, ValueSpread> particleProperties = new HashMap<ParticleKey, ValueSpread>();

    public EmitterNode(Emitter emitter) {
        setTransformable(true);
        setTranslation(LoaderUtil.toPoint3d(emitter.getPosition()));
        setRotation(LoaderUtil.toQuat4(emitter.getRotation()));

        setPlayMode(emitter.getMode());
        setDuration(emitter.getDuration());
        setEmissionSpace(emitter.getSpace());
        setTileSource(emitter.getTileSource());
        setAnimation(emitter.getAnimation());
        setMaterial(emitter.getMaterial());
        setMaxParticleCount(emitter.getMaxParticleCount());
        setEmitterType(emitter.getType());

        setProperties(emitter.getPropertiesList());
        setParticleProperties(emitter.getParticlePropertiesList());

        for (Modifier m : emitter.getModifiersList()) {
            switch (m.getType()) {
            case MODIFIER_TYPE_ACCELERATION:
                addChild(new AccelerationNode(m));
                break;
            case MODIFIER_TYPE_DRAG:
                addChild(new DragNode(m));
            }
        }
    }

    static {
        createPropertyKeys();
        createDescriptors();
    }

    private static void createPropertyKeys() {
        // Copy everything apart from last element (*_COUNT)

        EmitterKey[] ek = Particle.EmitterKey.values();
        emitterKeys = new Particle.EmitterKey[ek.length - 1];
        System.arraycopy(ek, 0, emitterKeys, 0, ek.length - 1);

        ParticleKey[] pk = ParticleKey.values();
        particleKeys = new ParticleKey[pk.length - 1];
        System.arraycopy(pk, 0, particleKeys, 0, pk.length - 1);
    }

    private boolean isEmitterKey(String key) {
        try {
            return EmitterKey.valueOf(key) != null;
        } catch (Throwable e) {}
        return false;
    }

    public void setProperty(String key, ValueSpread value) {

        if (isEmitterKey(key)) {
            properties.get(EmitterKey.valueOf(key)).set(value);
        } else {
            particleProperties.get(ParticleKey.valueOf(key)).set(value);
        }
        reloadSystem();
    }

    public ValueSpread getProperty(String key) {
        ValueSpread ret = null;
        if (isEmitterKey(key)) {
            ret = properties.get(EmitterKey.valueOf(key));
        } else {
            ret = particleProperties.get(ParticleKey.valueOf(key));
        }
        return new ValueSpread(ret);
    }

    @SuppressWarnings("unchecked")
    private static void createDescriptors() {
        List<IPropertyDesc<EmitterNode, ISceneModel>> lst = new ArrayList<IPropertyDesc<EmitterNode,ISceneModel>>();
        for (EmitterKey k : emitterKeys) {

            String displayName = k.getValueDescriptor().getOptions().getExtension(DdfExtensions.displayName);
            if (displayName == null) {
                displayName = k.name();
            }
            IPropertyDesc<EmitterNode, ISceneModel> p = new ValueSpreadPropertyDesc<EmitterNode, ISceneModel>(k.name(), displayName, "Emitter");
            lst.add(p);
        }

        for (ParticleKey k : particleKeys) {

            String displayName = k.getValueDescriptor().getOptions().getExtension(DdfExtensions.displayName);
            if (displayName == null) {
                displayName = k.name();
            }
            IPropertyDesc<EmitterNode, ISceneModel> p = new ValueSpreadPropertyDesc<EmitterNode, ISceneModel>(k.name(), displayName, "Particle");
            lst.add(p);
        }

        descriptors = lst.toArray(new IPropertyDesc[lst.size()]);
    }

    @DynamicProperties
    public IPropertyDesc<EmitterNode, ISceneModel>[] getDynamicProperties() {
        return descriptors;
    }

    @DynamicPropertyAccessor
    public IPropertyAccessor<EmitterNode, ISceneModel> getDynamicAccessor(ISceneModel world) {
        return new EmitterNodeAccessor(this);
    }

    @Override
    protected void transformChanged() {
        reloadSystem();
    }

    private void reloadSystem() {
        ParticleFXNode parent = (ParticleFXNode) getParent();
        if (parent != null) {
            parent.reload();
        }
    }

    private void setProperties(List<Emitter.Property> list) {
        for (EmitterKey k : emitterKeys) {
            ValueSpread vs = new ValueSpread();
            vs.setCurve(new HermiteSpline());
            this.properties.put(k, vs);
        }

        for (Emitter.Property p : list) {
            ValueSpread vs = ParticleUtils.toValueSpread(p.getPointsList());
            this.properties.put(p.getKey(), vs);
        }
    }

    private void setParticleProperties(List<Emitter.ParticleProperty> list) {
        for (ParticleKey k : particleKeys) {
            ValueSpread vs = new ValueSpread();
            vs.setCurve(new HermiteSpline());
            this.particleProperties.put(k, vs);
        }

        for (ParticleProperty p : list) {
            ValueSpread vs = ParticleUtils.toValueSpread(p.getPointsList());
            this.particleProperties.put(p.getKey(), vs);
        }
    }

    public PlayMode getPlayMode() {
        return playMode;
    }

    public void setPlayMode(PlayMode playMode) {
        this.playMode = playMode;
        reloadSystem();
    }

    public EmissionSpace getEmissionSpace() {
        return emissionSpace;
    }

    public void setEmissionSpace(EmissionSpace emissionSpace) {
        this.emissionSpace = emissionSpace;
        reloadSystem();
    }

    public float getDuration() {
        return duration;
    }

    public void setDuration(float duration) {
        this.duration = duration;
        reloadSystem();
    }

    public String getTileSource() {
        return tileSource;
    }

    public void setTileSource(String tileSource) {
        if (!this.tileSource.equals(tileSource)) {
            this.tileSource = tileSource;
            reloadTileSource();
            reloadSystem();
        }
    }

    public IStatus validateTileSource() {
        if (this.tileSetNode != null) {
            this.tileSetNode.updateStatus();
            IStatus status = this.tileSetNode.getStatus();
            if (!status.isOK()) {
                return new Status(IStatus.ERROR, ParticleEditorPlugin.PLUGIN_ID,
                        Messages.EmitterNode_tileSource_INVALID_REFERENCE);
            }
        } else if (!this.tileSource.isEmpty()) {
            return new Status(IStatus.ERROR, ParticleEditorPlugin.PLUGIN_ID,
                    Messages.EmitterNode_tileSource_CONTENT_ERROR);
        }
        return Status.OK_STATUS;
    }

    public String getAnimation() {
        return animation;
    }

    public void setAnimation(String animation) {
        this.animation = animation;
        reloadSystem();
    }

    public IStatus validateAnimation() {
        if (!this.animation.isEmpty()) {
            if (this.tileSetNode != null) {
                boolean exists = false;
                for (AnimationNode animation : this.tileSetNode.getAnimations()) {
                    if (animation.getId().equals(this.animation)) {
                        exists = true;
                        break;
                    }
                }
                if (!exists) {
                    return new Status(IStatus.ERROR, ParticleEditorPlugin.PLUGIN_ID, NLS.bind(
                            Messages.EmitterNode_animation_INVALID, this.animation));
                }
            }
        }
        return Status.OK_STATUS;
    }

    public String getMaterial() {
        return material;
    }

    public void setMaterial(String material) {
        this.material = material;
        reloadSystem();
    }

    public BlendMode getBlendMode() {
        return blendMode;
    }

    public void setBlendMode(BlendMode blendMode) {
        this.blendMode = blendMode;
        reloadSystem();
    }

    public int getMaxParticleCount() {
        return maxParticleCount;
    }

    public void setMaxParticleCount(int maxParticleCount) {
        this.maxParticleCount = maxParticleCount;
        reloadSystem();
    }

    public EmitterType getEmitterType() {
        return emitterType;
    }

    public void setEmitterType(EmitterType emitterType) {
        this.emitterType = emitterType;
        reloadSystem();
    }

    public TileSetNode getTileSetNode() {
        return this.tileSetNode;
    }

    @Override
    public String toString() {
        return "Emitter";
    }

    public Emitter.Builder buildMessage() {
        Emitter.Builder b = Emitter.newBuilder()
            .setMode(getPlayMode())
            .setDuration(getDuration())
            .setSpace(getEmissionSpace())
            .setTileSource(getTileSource())
            .setAnimation(getAnimation())
            .setMaterial(getMaterial())
            .setBlendMode(getBlendMode())
            .setMaxParticleCount(getMaxParticleCount())
            .setType(getEmitterType())
            .setPosition(LoaderUtil.toPoint3(getTranslation()))
            .setRotation(LoaderUtil.toQuat(getRotation()));

        // NOTE: Use enum for predictable order
        for (EmitterKey k : emitterKeys) {
            ValueSpread vs = properties.get(k);
            Builder pb = Emitter.Property.newBuilder().setKey(k);
            pb.addAllPoints(ParticleUtils.toSplinePointList(vs));
            b.addProperties(pb);
        }

        for (ParticleKey k : particleKeys) {
            ParticleProperty.Builder pb = Emitter.ParticleProperty.newBuilder().setKey(k);
            ValueSpread vs = particleProperties.get(k);
            pb.addAllPoints(ParticleUtils.toSplinePointList(vs));
            b.addParticleProperties(pb);
        }

        for (Node c : getChildren()) {
            if (c instanceof ModifierNode) {
                ModifierNode m = (ModifierNode) c;
                b.addModifiers(m.buildMessage());
            }
        }

        return b;
    }

    @Override
    public Image getIcon() {
        return ParticleEditorPlugin.getDefault().getImageRegistry().get(ParticleEditorPlugin.EMITTER_IMAGE_ID);
    }

    @Override
    public void setModel(ISceneModel model) {
        super.setModel(model);
        if (model != null && this.tileSetNode == null) {
            reloadTileSource();
        }
    }

    @Override
    public boolean handleReload(IFile file) {
        boolean reloaded = false;
        if (!this.tileSource.isEmpty()) {
            IFile tileSetFile = getModel().getFile(this.tileSource);
            if (tileSetFile.exists() && tileSetFile.equals(file)) {
                if (reloadTileSource()) {
                    reloaded = true;
                }
            }
            if (this.tileSetNode != null) {
                if (this.tileSetNode.handleReload(file)) {
                    reloaded = true;
                }
            }
        }
        return reloaded;
    }

    private boolean reloadTileSource() {
        ISceneModel model = getModel();
        if (model != null) {
            this.tileSetNode = null;
            if (!this.tileSource.isEmpty()) {
                try {
                    Node node = model.loadNode(this.tileSource);
                    if (node instanceof TileSetNode) {
                        this.tileSetNode = (TileSetNode) node;
                        this.tileSetNode.setModel(getModel());
                        updateStatus();
                    }
                } catch (Exception e) {
                    // no reason to handle exception since having a null type is
                    // invalid state, will be caught in validateComponent below
                }
            }
            // attempted to reload
            return true;
        }
        return false;
    }
}
