package com.dynamo.cr.parted.nodes;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import javax.vecmath.Point2d;
import javax.vecmath.Vector2d;
import javax.vecmath.Vector3d;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.osgi.util.NLS;
import org.eclipse.swt.graphics.Image;

import com.dynamo.bob.textureset.TextureSetGenerator;
import com.dynamo.cr.common.util.DDFUtil;
import com.dynamo.cr.parted.Messages;
import com.dynamo.cr.parted.ParticleEditorPlugin;
import com.dynamo.cr.parted.curve.HermiteSpline;
import com.dynamo.cr.properties.DynamicProperties;
import com.dynamo.cr.properties.DynamicPropertyAccessor;
import com.dynamo.cr.properties.IPropertyAccessor;
import com.dynamo.cr.properties.IPropertyDesc;
import com.dynamo.cr.properties.NotEmpty;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.cr.properties.Range;
import com.dynamo.cr.properties.descriptors.ValueSpreadPropertyDesc;
import com.dynamo.cr.properties.types.ValueSpread;
import com.dynamo.cr.sceneed.core.AABB;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Identifiable;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.util.LoaderUtil;
import com.dynamo.cr.tileeditor.scene.RuntimeTextureSet;
import com.dynamo.cr.tileeditor.scene.TextureSetNode;
import com.dynamo.particle.proto.Particle;
import com.dynamo.particle.proto.Particle.BlendMode;
import com.dynamo.particle.proto.Particle.EmissionSpace;
import com.dynamo.particle.proto.Particle.Emitter;
import com.dynamo.particle.proto.Particle.Emitter.ParticleProperty;
import com.dynamo.particle.proto.Particle.Emitter.Property.Builder;
import com.dynamo.particle.proto.Particle.EmitterKey;
import com.dynamo.particle.proto.Particle.EmitterType;
import com.dynamo.particle.proto.Particle.Modifier;
import com.dynamo.particle.proto.Particle.ParticleKey;
import com.dynamo.particle.proto.Particle.ParticleOrientation;
import com.dynamo.particle.proto.Particle.PlayMode;
import com.dynamo.particle.proto.Particle.SizeMode;
import com.dynamo.proto.DdfExtensions;
import com.dynamo.textureset.proto.TextureSetProto.TextureSetAnimation;

public class EmitterNode extends Node implements Identifiable {

    private static final long serialVersionUID = 1L;

    private static IPropertyDesc<EmitterNode, ISceneModel>[] descriptors;
    private static EmitterKey[] emitterKeys;
    private static ParticleKey[] particleKeys;

    @Property(editorType = EditorType.DROP_DOWN)
    private SizeMode sizeMode = SizeMode.SIZE_MODE_AUTO;

    @Property(category = "Emitter")
    @NotEmpty(severity = IStatus.ERROR)
    private String id;

    @Property(editorType = EditorType.DROP_DOWN)
    private PlayMode playMode;

    @Property(editorType = EditorType.DROP_DOWN)
    private EmissionSpace emissionSpace;

    @Property
    private ValueSpread duration;

    @Property
    private ValueSpread startDelay;

    @Property(displayName = "Image", editorType = EditorType.RESOURCE, extensions = { "tilesource", "tileset", "atlas" })
    private String tileSource = "";

    private transient TextureSetNode textureSetNode = null;

    @Property(editorType=EditorType.DROP_DOWN)
    private String animation = "";

    @Property(editorType = EditorType.RESOURCE, extensions = { "material" })
    private String material = "";

    @Property(editorType = EditorType.DROP_DOWN)
    private BlendMode blendMode = BlendMode.BLEND_MODE_ALPHA;

    @Property
    @Range(min = 0.0)
    private int maxParticleCount;

    @Property
    private EmitterType emitterType;

    @Property
    private ParticleOrientation particleOrientation;

    @Property
    private double inheritVelocity;

    private Map<EmitterKey, ValueSpread> properties = new HashMap<EmitterKey, ValueSpread>();
    private Map<ParticleKey, ValueSpread> particleProperties = new HashMap<ParticleKey, ValueSpread>();

    public EmitterNode(Emitter emitter) {
        initDefaults();
        setTransformable(true);
        setTranslation(LoaderUtil.toPoint3d(emitter.getPosition()));
        setRotation(LoaderUtil.toQuat4(emitter.getRotation()));
        setSizeMode(emitter.getSizeMode());
        setId(emitter.getId());
        setPlayMode(emitter.getMode());
        duration.setValue(emitter.getDuration());
        duration.setSpread(emitter.getDurationSpread());
        startDelay.setValue(emitter.getStartDelay());
        startDelay.setSpread(emitter.getStartDelaySpread());
        setEmissionSpace(emitter.getSpace());
        setTileSource(emitter.getTileSource());
        setAnimation(emitter.getAnimation());
        setMaterial(emitter.getMaterial());
        setBlendMode(emitter.getBlendMode());
        setMaxParticleCount(emitter.getMaxParticleCount());
        setEmitterType(emitter.getType());
        setParticleOrientation(emitter.getParticleOrientation());
        setInheritVelocity(emitter.getInheritVelocity());

        setProperties(emitter.getPropertiesList());
        setParticleProperties(emitter.getParticlePropertiesList());

        for (Modifier m : emitter.getModifiersList()) {
            addChild(ParticleUtils.createModifierNode(m));
        }

        updateAABB();
    }

    private void initDefaults() {
        duration = new ValueSpread();
        duration.setCurvable(false);
        startDelay = new ValueSpread();
        startDelay.setCurvable(false);

        for (EmitterKey k : emitterKeys) {
            ValueSpread vs = new ValueSpread();
            vs.setCurve(new HermiteSpline());
            this.properties.put(k, vs);
        }

        for (ParticleKey k : particleKeys) {
            ValueSpread vs = new ValueSpread();
            vs.setHideSpread(true);
            vs.setCurve(new HermiteSpline());
            this.particleProperties.put(k, vs);
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

    public static class UVTransform {
        public Vector2d scale;
        public boolean rotated;
    }

    public UVTransform getUVTransform() {
        UVTransform uvTransform = new UVTransform();
        RuntimeTextureSet rTS = null;
        TextureSetAnimation animation = null;
        if(this.textureSetNode != null && this.animation != null) {
            rTS = this.textureSetNode.getRuntimeTextureSet();
            animation = rTS.getAnimation(this.animation);
        }
        if(animation == null) {
            uvTransform.scale = new Vector2d(1,-1);
            uvTransform.rotated = false;
        } else {
            TextureSetGenerator.UVTransform uvt = rTS.getUvTransform(animation, 0);
            uvTransform.scale = uvt.scale;
            uvTransform.rotated = uvt.rotated;
        }
        return uvTransform;
    }

    public void updateSize() {
        if (this.textureSetNode == null || getSizeMode() == SizeMode.SIZE_MODE_MANUAL) {
            return;
        }
        Point2d textureSize = this.textureSetNode.getTextureHandle().getTextureSize();
        if(getSizeMode() == SizeMode.SIZE_MODE_AUTO) {
            UVTransform uvTransform = getUVTransform();
            Vector3d size = new Vector3d();
            if(uvTransform.rotated) {
                size.y = uvTransform.scale.x * textureSize.x;
                size.x = uvTransform.scale.y * textureSize.y;
            } else {
                size.x = uvTransform.scale.x * textureSize.x;
                size.y = uvTransform.scale.y * textureSize.y;
            }

            double uniformSize = Math.max(size.x, size.y);
            properties.get(EmitterKey.EMITTER_KEY_PARTICLE_SIZE).setValue(uniformSize);
        }
    }

    private final void updateAABB() {

        AABB aabb = new AABB();
        double sizeX = getProperty(EmitterKey.EMITTER_KEY_SIZE_X.toString()).getValue();
        double sizeY = getProperty(EmitterKey.EMITTER_KEY_SIZE_Y.toString()).getValue();
        double sizeZ = getProperty(EmitterKey.EMITTER_KEY_SIZE_Z.toString()).getValue();

        double wExt = 1;
        double hExt = 1;
        double dExt = 1;

        switch (getEmitterType()) {
        case EMITTER_TYPE_CIRCLE:
            wExt = sizeX;
            hExt = sizeX;
            dExt = sizeX;
            break;
        case EMITTER_TYPE_BOX:
            wExt = sizeX;
            hExt = sizeY;
            dExt = sizeZ;
            break;
        case EMITTER_TYPE_SPHERE:
            wExt = sizeX;
            hExt = sizeX;
            dExt = sizeX;
            break;
        case EMITTER_TYPE_CONE:
            wExt = sizeX;
            hExt = sizeY;
            dExt = sizeX;
            break;
        case EMITTER_TYPE_2DCONE:
            wExt = sizeX;
            hExt = sizeY;
            dExt = sizeX;
            break;
        }

        aabb.union(-wExt, -hExt, -dExt);
        aabb.union(wExt, hExt, dExt);
        setAABB(aabb);
    }

    public void setProperty(String key, ValueSpread value) {
        if (isEmitterKey(key)) {
            properties.get(EmitterKey.valueOf(key)).set(value);
            updateAABB();
        } else {
            particleProperties.get(ParticleKey.valueOf(key)).set(value);
        }
        reloadSystem(false);
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

    private static boolean isColor(String key) {
        // NOTE: Hack to check if a key is a color component, see below
        if (key.endsWith("_RED") ||
            key.endsWith("_GREEN") ||
            key.endsWith("_BLUE") ||
            key.endsWith("_ALPHA")) {
            return true;
        }
        return false;
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
            p.setMin(0.0);
            lst.add(p);
        }

        for (ParticleKey k : particleKeys) {

            String displayName = k.getValueDescriptor().getOptions().getExtension(DdfExtensions.displayName);
            if (displayName == null) {
                displayName = k.name();
            }
            IPropertyDesc<EmitterNode, ISceneModel> p = new ValueSpreadPropertyDesc<EmitterNode, ISceneModel>(k.name(), displayName, "Particle");
            p.setMin(0.0);
            lst.add(p);
        }

        descriptors = lst.toArray(new IPropertyDesc[lst.size()]);

        // NOTE: "Hack" to set max-value for color components
        for (IPropertyDesc<EmitterNode, ISceneModel> pd : lst) {
            if (isColor(pd.getId())) {
                pd.setMax(1.0);
            }
        }
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
        reloadSystem(false);
    }

    @Override
    protected void childAdded(Node child) {
        reloadSystem(true);
    }

    @Override
    protected void childRemoved(Node child) {
        reloadSystem(true);
    }

    protected void reloadSystem(boolean forceReplay) {
        ParticleFXNode parent = (ParticleFXNode) getParent();
        if (parent != null) {
            parent.reload(forceReplay);
        }
    }

    private void setProperties(List<Emitter.Property> list) {
        for (Emitter.Property p : list) {
            ValueSpread vs = ParticleUtils.toValueSpread(p.getPointsList(), p.getSpread());
            this.properties.put(p.getKey(), vs);
        }
    }

    private void setParticleProperties(List<Emitter.ParticleProperty> list) {
        for (ParticleProperty p : list) {
            ValueSpread vs = ParticleUtils.toValueSpread(p.getPointsList());
            vs.setHideSpread(true);
            this.particleProperties.put(p.getKey(), vs);
        }
    }

    public String getId() {
        return this.id;
    }

    public void setId(String id) {
        this.id = id;
    }

    public PlayMode getPlayMode() {
        return playMode;
    }

    public void setPlayMode(PlayMode playMode) {
        this.playMode = playMode;
        reloadSystem(false);
    }

    public EmissionSpace getEmissionSpace() {
        return emissionSpace;
    }

    public void setEmissionSpace(EmissionSpace emissionSpace) {
        this.emissionSpace = emissionSpace;
        for (Node node : getChildren()) {
            if (node instanceof RotationalModifierNode) {
                if (emissionSpace.equals(EmissionSpace.EMISSION_SPACE_WORLD)) {
                    node.setFlags(Flags.NO_INHERIT_ROTATION);
                } else {
                    node.clearFlags(Flags.NO_INHERIT_ROTATION);
                }
            }
        }
        reloadSystem(false);
    }

    public ValueSpread getDuration() {
        return duration;
    }

    public void setDuration(ValueSpread valueSpread) {
    	this.duration = valueSpread;
        reloadSystem(false);
    }

    public ValueSpread getStartDelay() {
        return startDelay;
    }

    public void setStartDelay(ValueSpread startDelay) {
        this.startDelay = startDelay;
        reloadSystem(false);
    }

    public String getTileSource() {
        return tileSource;
    }

    public void setTileSource(String tileSource) {
        if (!this.tileSource.equals(tileSource)) {
            this.tileSource = tileSource;
            reloadTileSource();
            reloadSystem(false);
        }
    }

    public IStatus validateTileSource() {
        if (this.textureSetNode != null) {
            this.textureSetNode.updateStatus();
            IStatus status = this.textureSetNode.getStatus();
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
        reloadSystem(false);
        updateSize();
    }

    public Object[] getAnimationOptions() {
        if (this.textureSetNode != null) {
            updateSize();
            return this.textureSetNode.getAnimationIds().toArray();
        } else {
            return new Object[0];
        }
    }

    public IStatus validateAnimation() {
        if (!this.animation.isEmpty()) {
            if (this.textureSetNode != null) {
                boolean exists = this.textureSetNode.getRuntimeTextureSet().getAnimation(this.animation) != null;
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
        reloadSystem(false);
    }

    public BlendMode getBlendMode() {
        return blendMode;
    }

    public void setBlendMode(BlendMode blendMode) {
        this.blendMode = blendMode;
        reloadSystem(false);
    }

    public IStatus validateBlendMode() {
        if (this.blendMode == BlendMode.BLEND_MODE_ADD_ALPHA) {
            String add = DDFUtil.getEnumValueDisplayName(BlendMode.BLEND_MODE_ADD.getValueDescriptor());
            String addAlpha = DDFUtil.getEnumValueDisplayName(BlendMode.BLEND_MODE_ADD_ALPHA.getValueDescriptor());
            return new Status(Status.WARNING, ParticleEditorPlugin.PLUGIN_ID, String.format(
                    "'%s' has been replaced by '%s'",
                    addAlpha, add));
        } else {
            return Status.OK_STATUS;
        }
    }

    public int getMaxParticleCount() {
        return maxParticleCount;
    }

    public void setMaxParticleCount(int maxParticleCount) {
        this.maxParticleCount = maxParticleCount;
        reloadSystem(false);
    }

    public EmitterType getEmitterType() {
        return emitterType;
    }

    public void setEmitterType(EmitterType emitterType) {
        this.emitterType = emitterType;
        updateAABB();
        reloadSystem(false);
    }


    protected IStatus validateEmitterType() {

        // Disabled info about emitter type as the information is propagated as error
        return Status.OK_STATUS;

        /*
        EmitterType t = getEmitterType();
        if (t == EmitterType.EMITTER_TYPE_SPHERE || t == EmitterType.EMITTER_TYPE_CONE) {
            return new Status(IStatus.INFO, ParticleEditorPlugin.PLUGIN_ID, Messages.EmitterNode_animation_3DEMITTER_INFO);
        } else {
            return Status.OK_STATUS;
        }*/

    }

    public ParticleOrientation getParticleOrientation() {
        return particleOrientation;
    }

    public void setParticleOrientation(ParticleOrientation particleOrientation) {
        this.particleOrientation = particleOrientation;
        reloadSystem(false);
    }

    public double getInheritVelocity() {
        return this.inheritVelocity;
    }

    public void setInheritVelocity(double inheritVelocity) {
        this.inheritVelocity = inheritVelocity;
        reloadSystem(false);
    }

    public boolean isInheritVelocityEditable() {
        return this.emissionSpace == EmissionSpace.EMISSION_SPACE_WORLD;
    }

    public TextureSetNode getTextureSetNode() {
        return this.textureSetNode;
    }

    public SizeMode getSizeMode() {
        return this.sizeMode;
    }

    public void setSizeMode(SizeMode sizeMode) {
        this.sizeMode = sizeMode;
        updateSize();
        reloadSystem(false);
    }

    public boolean isSizeEditable() {
        return getSizeMode() == SizeMode.SIZE_MODE_MANUAL;
    }

    @Override
    public String toString() {
        return this.id;
    }

    public Emitter.Builder buildMessage() {
        Emitter.Builder b = Emitter.newBuilder()
            .setSizeMode(getSizeMode())
            .setId(getId())
            .setMode(getPlayMode())
            .setDuration((float)duration.getValue())
            .setDurationSpread((float)duration.getSpread())
            .setStartDelay((float)startDelay.getValue())
            .setStartDelaySpread((float)startDelay.getSpread())
            .setSpace(getEmissionSpace())
            .setTileSource(getTileSource())
            .setAnimation(getAnimation())
            .setMaterial(getMaterial())
            .setBlendMode(getBlendMode())
            .setMaxParticleCount(getMaxParticleCount())
            .setType(getEmitterType())
            .setParticleOrientation(getParticleOrientation())
            .setInheritVelocity((float)getInheritVelocity())
            .setPosition(LoaderUtil.toPoint3(getTranslation()))
            .setRotation(LoaderUtil.toQuat(getRotation()));

        // NOTE: Use enum for predictable order
        for (EmitterKey k : emitterKeys) {
            ValueSpread vs = properties.get(k);
            Builder pb = Emitter.Property.newBuilder().setKey(k);
            pb.addAllPoints(ParticleUtils.toSplinePointList(vs));
            pb.setSpread((float)vs.getSpread());
            b.addProperties(pb);
        }

        for (ParticleKey k : particleKeys) {
            ParticleProperty.Builder pb = Emitter.ParticleProperty.newBuilder().setKey(k);
            ValueSpread vs = particleProperties.get(k);
            pb.addAllPoints(ParticleUtils.toSplinePointList(vs));
            b.addParticleProperties(pb);
        }

        for (Node c : getChildren()) {
            if (c instanceof AbstractModifierNode) {
                AbstractModifierNode m = (AbstractModifierNode) c;
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
        if (model != null && this.textureSetNode == null) {
            reloadTileSource();
        }
    }

    @Override
    public boolean handleReload(IFile file, boolean childWasReloaded) {
        boolean reloaded = false;
        if (!this.tileSource.isEmpty()) {
            IFile tileSetFile = getModel().getFile(this.tileSource);
            if (tileSetFile.exists() && tileSetFile.equals(file)) {
                if (reloadTileSource()) {
                    reloaded = true;
                    reloadSystem(false);
                }
            }
            if (this.textureSetNode != null) {
                if (this.textureSetNode.handleReload(file, childWasReloaded)) {
                    reloaded = true;
                    reloadSystem(false);
                }
            }
        }
        return reloaded;
    }

    private boolean reloadTileSource() {
        ISceneModel model = getModel();
        if (model != null) {
            this.textureSetNode = null;
            if (!this.tileSource.isEmpty()) {
                try {
                    Node node = model.loadNode(this.tileSource);
                    if (node instanceof TextureSetNode) {
                        this.textureSetNode = (TextureSetNode) node;
                        this.textureSetNode.setModel(getModel());
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
