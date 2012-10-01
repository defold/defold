package com.dynamo.cr.parted;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.eclipse.swt.graphics.Image;

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
import com.dynamo.particle.proto.Particle;
import com.dynamo.particle.proto.Particle.EmissionSpace;
import com.dynamo.particle.proto.Particle.Emitter;
import com.dynamo.particle.proto.Particle.Emitter.ParticleProperty;
import com.dynamo.particle.proto.Particle.Emitter.Property.Builder;
import com.dynamo.particle.proto.Particle.EmitterKey;
import com.dynamo.particle.proto.Particle.EmitterType;
import com.dynamo.particle.proto.Particle.ParticleKey;
import com.dynamo.particle.proto.Particle.PlayMode;
import com.dynamo.particle.proto.Particle.SplinePoint;
import com.dynamo.proto.DdfExtensions;

public class EmitterNode extends Node {

    private static final long serialVersionUID = 1L;

    private static IPropertyDesc<EmitterNode, ISceneModel>[] descriptors;
    private static EmitterKey[] emitterKeys;
    private static ParticleKey[] particleKeys;

    @Property(editorType = EditorType.DROP_DOWN)
    private PlayMode playMode;

    @Property(editorType = EditorType.DROP_DOWN)
    private EmissionSpace emissionSpace;

    @Property
    private float duration;

    @Property(editorType = EditorType.RESOURCE, extensions = { "tilesource", "tileset" })
    private String tileSource;

    @Property
    private String animation;

    @Property(editorType = EditorType.RESOURCE, extensions = { "material" })
    private String material;

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

    /* package */ void setProperty(String key, ValueSpread value) {

        if (isEmitterKey(key)) {
            properties.get(EmitterKey.valueOf(key)).set(value);
        } else {
            particleProperties.get(ParticleKey.valueOf(key)).set(value);
        }
        resetSystem();
    }

    /* package */  public ValueSpread getProperty(String key) {
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
            IPropertyDesc<EmitterNode, ISceneModel> p = new ValueSpreadPropertyDesc<EmitterNode, ISceneModel>(k.name(), displayName);
            lst.add(p);
        }

        for (ParticleKey k : particleKeys) {

            String displayName = k.getValueDescriptor().getOptions().getExtension(DdfExtensions.displayName);
            if (displayName == null) {
                displayName = k.name();
            }
            IPropertyDesc<EmitterNode, ISceneModel> p = new ValueSpreadPropertyDesc<EmitterNode, ISceneModel>(k.name(), displayName);
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
        resetSystem();
    }

    private void resetSystem() {
        ParticleFXNode parent = (ParticleFXNode) getParent();
        if (parent != null) {
            parent.reset();
        }
    }

    private ValueSpread toValueSpread(List<SplinePoint> pl) {
        int count = pl.size() * 4;
        boolean animated = pl.size() > 1;

        if (!animated) {
            count += 4;
        }

        float[] data = new float[count];
        int i = 0;
        for (SplinePoint sp : pl) {
            data[i++] = sp.getX();
            data[i++] = sp.getY();
            data[i++] = sp.getTX();
            data[i++] = sp.getTY();
        }

        ValueSpread vs = new ValueSpread();
        if (!animated) {
            SplinePoint sp = pl.get(0);
            data[i++] = 1;
            data[i++] = sp.getY();
            data[i++] = sp.getTX();
            data[i++] = sp.getTY();
            vs.setValue(sp.getY());
        }

        HermiteSpline spline = new HermiteSpline(data);
        vs.setCurve(spline);
        vs.setAnimated(animated);
        vs.setValue(pl.get(0).getY());
        return vs;
    }

    private List<SplinePoint> toSplinePointList(HermiteSpline spline) {
        List<SplinePoint> lst = new ArrayList<Particle.SplinePoint>();
        for (int i = 0; i < spline.getCount(); ++i) {
            com.dynamo.cr.parted.curve.SplinePoint p = spline.getPoint(i);
            SplinePoint sp = SplinePoint.newBuilder()
                    .setX((float) p.x)
                    .setY((float) p.y)
                    .setTX((float) p.tx)
                    .setTY((float) p.ty)
                    .build();

            lst.add(sp);
        }
        return lst;
    }

    private void setProperties(List<Emitter.Property> list) {
        for (EmitterKey k : emitterKeys) {
            ValueSpread vs = new ValueSpread();
            vs.setCurve(new HermiteSpline());
            this.properties.put(k, vs);
        }

        for (Emitter.Property p : list) {
            ValueSpread vs = toValueSpread(p.getPointsList());
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
            ValueSpread vs = toValueSpread(p.getPointsList());
            this.particleProperties.put(p.getKey(), vs);
        }
    }

    public PlayMode getPlayMode() {
        return playMode;
    }

    public void setPlayMode(PlayMode playMode) {
        this.playMode = playMode;
        resetSystem();
    }

    public EmissionSpace getEmissionSpace() {
        return emissionSpace;
    }

    public void setEmissionSpace(EmissionSpace emissionSpace) {
        this.emissionSpace = emissionSpace;
        resetSystem();
    }

    public float getDuration() {
        return duration;
    }

    public void setDuration(float duration) {
        this.duration = duration;
        resetSystem();
    }

    public String getTileSource() {
        return tileSource;
    }

    public void setTileSource(String tileSource) {
        this.tileSource = tileSource;
        resetSystem();
    }

    public String getAnimation() {
        return animation;
    }

    public void setAnimation(String animation) {
        this.animation = animation;
        resetSystem();
    }

    public String getMaterial() {
        return material;
    }

    public void setMaterial(String material) {
        this.material = material;
        resetSystem();
    }

    public int getMaxParticleCount() {
        return maxParticleCount;
    }

    public void setMaxParticleCount(int maxParticleCount) {
        this.maxParticleCount = maxParticleCount;
        resetSystem();
    }

    public EmitterType getEmitterType() {
        return emitterType;
    }

    public void setEmitterType(EmitterType emitterType) {
        this.emitterType = emitterType;
        resetSystem();
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
            .setMaxParticleCount(getMaxParticleCount())
            .setType(getEmitterType())
            .setPosition(LoaderUtil.toPoint3(getTranslation()))
            .setRotation(LoaderUtil.toQuat(getRotation()));

        // NOTE: Use enum for predictable order
        for (EmitterKey k : emitterKeys) {
            ValueSpread vs = properties.get(k);
            Builder pb = Emitter.Property.newBuilder().setKey(k);
            HermiteSpline spline = (HermiteSpline) vs.getCurve();

            if (vs.isAnimated()) {
                pb.addAllPoints(toSplinePointList(spline));
            } else {
                pb.addPoints(SplinePoint.newBuilder()
                        .setX(0)
                        .setY((float) vs.getValue())
                        .setTX(1)
                        .setTY(0));
            }

            b.addProperties(pb);
        }

        for (ParticleKey k : particleKeys) {
            ParticleProperty.Builder pb = Emitter.ParticleProperty.newBuilder().setKey(k);
            ValueSpread vs = particleProperties.get(k);
            HermiteSpline spline = (HermiteSpline) vs.getCurve();
            if (vs.isAnimated()) {
                pb.addAllPoints(toSplinePointList(spline));
            } else {
                pb.addPoints(SplinePoint.newBuilder()
                        .setX(0)
                        .setY((float) vs.getValue())
                        .setTX(1)
                        .setTY(0));
            }
            b.addParticleProperties(pb);
        }

        return b;
    }

    @Override
    public Image getIcon() {
        return ParticleEditorPlugin.getDefault().getImageRegistry().get(ParticleEditorPlugin.EMITTER_IMAGE_ID);
    }

}
