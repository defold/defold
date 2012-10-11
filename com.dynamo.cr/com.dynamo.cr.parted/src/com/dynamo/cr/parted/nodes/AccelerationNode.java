package com.dynamo.cr.parted.nodes;

import com.dynamo.particle.proto.Particle.Modifier;
import com.dynamo.particle.proto.Particle.Modifier.Builder;
import com.dynamo.particle.proto.Particle.ModifierType;

public class AccelerationNode extends AbstractModifierNode {

    private static final long serialVersionUID = 1L;

    public AccelerationNode(Modifier modifier) {
        super(modifier);

        for (Modifier.Property p : modifier.getPropertiesList()) {
            switch (p.getKey()) {
            case MODIFIER_KEY_MAGNITUDE:
                magnitude = ParticleUtils.toValueSpread(p.getPointsList(), p.getSpread());
                break;
            }
        }
    }

    @Override
    public ModifierType getModifierType() {
        return ModifierType.MODIFIER_TYPE_ACCELERATION;
    }

    @Override
    public String toString() {
        return "Acceleration";
    }

    @Override
    public void buildProperties(Builder builder) {
    }

}
