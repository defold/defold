package com.dynamo.cr.parted.nodes;

import com.dynamo.cr.parted.curve.HermiteSpline;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.types.ValueSpread;
import com.dynamo.cr.sceneed.core.AABB;
import com.dynamo.particle.proto.Particle.Modifier;
import com.dynamo.particle.proto.Particle.Modifier.Builder;
import com.dynamo.particle.proto.Particle.ModifierKey;
import com.dynamo.particle.proto.Particle.ModifierType;

public class RadialNode extends AbstractModifierNode {

    private static final long serialVersionUID = 1L;

    @Property
    protected ValueSpread attenuation = new ValueSpread(new HermiteSpline());

    @Property
    protected double maxDistance = 10.0f;

    public RadialNode(Modifier modifier) {
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
        updateAABB();
    }

    @Override
    protected void updateAABB() {
        AABB aabb = new AABB();
        double r = getMaxDistance();
        if (Math.abs(r) > 0.0001) {
            aabb.union(-r, -r, -r);
            aabb.union(r, r, r);
        }
        setAABB(aabb);
    }

    public ValueSpread getAttenuation() {
        return new ValueSpread(attenuation);
    }

    public void setAttenuation(ValueSpread attenuation) {
        this.attenuation.set(attenuation);
        reloadSystem();
    }

    public double getMaxDistance() {
        return maxDistance;
    }

    public void setMaxDistance(double maxDistance) {
        this.maxDistance = maxDistance;
        updateAABB();
    }

    @Override
    public ModifierType getModifierType() {
        return ModifierType.MODIFIER_TYPE_RADIAL;
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
        return "Radial";
    }


}
