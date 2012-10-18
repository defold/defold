package com.dynamo.cr.parted.nodes;

import com.dynamo.particle.proto.Particle.Modifier;
import com.dynamo.particle.proto.Particle.ModifierType;

public class RadialNode extends PositionalModifierNode {

    private static final long serialVersionUID = 1L;


    public RadialNode(Modifier modifier) {
        super(modifier);
    }

    @Override
    public ModifierType getModifierType() {
        return ModifierType.MODIFIER_TYPE_RADIAL;
    }

    @Override
    public String toString() {
        return "Radial";
    }


}
