package com.dynamo.cr.parted.nodes;

import com.dynamo.cr.parted.curve.HermiteSpline;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.types.ValueSpread;
import com.dynamo.cr.sceneed.core.util.LoaderUtil;
import com.dynamo.particle.proto.Particle.Modifier;
import com.dynamo.particle.proto.Particle.ModifierKey;
import com.dynamo.particle.proto.Particle.ModifierType;

public class AccelerationNode extends ModifierNode {

    private static final long serialVersionUID = 1L;

    @Property
    private ValueSpread magnitude = new ValueSpread(new HermiteSpline());

    @Property
    private ValueSpread attenuation = new ValueSpread(new HermiteSpline());

    public AccelerationNode(Modifier modifier) {
        setTransformable(true);
        setTranslation(LoaderUtil.toPoint3d(modifier.getPosition()));
        setRotation(LoaderUtil.toQuat4(modifier.getRotation()));

        for (Modifier.Property p : modifier.getPropertiesList()) {
            switch (p.getKey()) {
            case MODIFIER_KEY_MAGNITUDE:
                magnitude = ParticleUtils.toValueSpread(p.getPointsList());
                break;
            case MODIFIER_KEY_ATTENUATION:
                attenuation = ParticleUtils.toValueSpread(p.getPointsList());
                break;
            }
        }
    }

    public ValueSpread getMagnitude() {
        return new ValueSpread(magnitude);
    }

    public void setMagnitude(ValueSpread magnitude) {
        this.magnitude.set(magnitude);
    }

    public ValueSpread getAttenuation() {
        return new ValueSpread(attenuation);
    }

    public void setAttenuation(ValueSpread attenuation) {
        this.attenuation.set(attenuation);
    }

    @Override
    protected void transformChanged() {
        reloadSystem();
    }

    private void reloadSystem() {
        if (getParent() != null) {
            ParticleFXNode parent = (ParticleFXNode) getParent().getParent();
            if (parent != null) {
                parent.reload();
            }
        }
    }

    @Override
    public String toString() {
        return "Acceleration";
    }

    @Override
    public Modifier buildMessage() {
        return Modifier.newBuilder()
            .setType(ModifierType.MODIFIER_TYPE_ACCELERATION)
            .setUseDirection(0)
            .setPosition(LoaderUtil.toPoint3(getTranslation()))
            .setRotation(LoaderUtil.toQuat(getRotation()))
            .addProperties(Modifier.Property.newBuilder()
                    .setKey(ModifierKey.MODIFIER_KEY_MAGNITUDE)
                    .addAllPoints(ParticleUtils.toSplinePointList(magnitude)))
            .addProperties(Modifier.Property.newBuilder()
                .setKey(ModifierKey.MODIFIER_KEY_ATTENUATION)
                .addAllPoints(ParticleUtils.toSplinePointList(attenuation)))
            .build();
    }
}
