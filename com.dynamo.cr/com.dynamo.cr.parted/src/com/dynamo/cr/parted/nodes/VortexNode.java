package com.dynamo.cr.parted.nodes;

import com.dynamo.particle.proto.Particle.Modifier;
import com.dynamo.particle.proto.Particle.ModifierType;

public class VortexNode extends PositionalModifierNode {

    private static final long serialVersionUID = 1L;

    public VortexNode(Modifier modifier) {
        super(modifier);
    }

    @Override
    public ModifierType getModifierType() {
        return ModifierType.MODIFIER_TYPE_VORTEX;
    }

    @Override
    public String toString() {
        return "Vortex";
    }

}
