package com.dynamo.cr.parted.nodes;

import com.dynamo.cr.properties.Property;
import com.dynamo.cr.sceneed.core.AABB;
import com.dynamo.particle.proto.Particle.Modifier;
import com.dynamo.particle.proto.Particle.Modifier.Builder;
import com.dynamo.particle.proto.Particle.ModifierKey;

public abstract class PositionalModifierNode extends AbstractModifierNode {

    private static final long serialVersionUID = 1L;

    @Property
    protected double maxDistance = 0.0;

    public PositionalModifierNode(Modifier modifier) {
        super(modifier);
        for (Modifier.Property p : modifier.getPropertiesList()) {
            switch (p.getKey()) {
            case MODIFIER_KEY_MAX_DISTANCE:
                maxDistance = p.getPointsList().get(0).getY();
                break;
            }
        }
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

    public double getMaxDistance() {
        return this.maxDistance;
    }

    public void setMaxDistance(double maxDistance) {
        this.maxDistance = maxDistance;
        updateAABB();
        reloadSystem();
    }

    @Override
    public void buildProperties(Builder builder) {
        builder.addProperties(Modifier.Property.newBuilder()
                .setKey(ModifierKey.MODIFIER_KEY_MAX_DISTANCE)
                .addAllPoints(ParticleUtils.toSplinePointList(maxDistance)));
    }

}
