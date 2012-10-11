package com.dynamo.cr.parted.nodes;

import com.dynamo.cr.parted.curve.HermiteSpline;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.types.ValueSpread;
import com.dynamo.particle.proto.Particle.Modifier;
import com.dynamo.particle.proto.Particle.Modifier.Builder;
import com.dynamo.particle.proto.Particle.ModifierKey;
import com.dynamo.particle.proto.Particle.ModifierType;

public class DragNode extends AbstractModifierNode {

    private static final long serialVersionUID = 1L;

    @Property
    protected ValueSpread attenuation = new ValueSpread(new HermiteSpline());

    public DragNode(Modifier modifier) {
        super(modifier);

        for (Modifier.Property p : modifier.getPropertiesList()) {
            switch (p.getKey()) {
            case MODIFIER_KEY_MAGNITUDE:
                magnitude = ParticleUtils.toValueSpread(p.getPointsList(), p.getSpread());
                break;
            case MODIFIER_KEY_ATTENUATION:
                attenuation = ParticleUtils.toValueSpread(p.getPointsList(), p.getSpread());
                break;
            }
        }
    }

    public ValueSpread getAttenuation() {
        return new ValueSpread(attenuation);
    }

    public void setAttenuation(ValueSpread attenuation) {
        this.attenuation.set(attenuation);
        reloadSystem();
    }

    @Override
    public ModifierType getModifierType() {
        return ModifierType.MODIFIER_TYPE_DRAG;
    }

    @Override
    public void buildProperties(Builder builder) {
        builder.addProperties(Modifier.Property.newBuilder()
                .setKey(ModifierKey.MODIFIER_KEY_ATTENUATION)
                .addAllPoints(ParticleUtils.toSplinePointList(attenuation))
                .setSpread((float)attenuation.getSpread()));
    }

    @Override
    public String toString() {
        return "Drag";
    }

}
