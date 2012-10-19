package com.dynamo.cr.parted.nodes;

import com.dynamo.particle.proto.Particle.Modifier;
import com.dynamo.particle.proto.Particle.Modifier.Builder;
import com.dynamo.particle.proto.Particle.ModifierType;

public class DragNode extends AbstractModifierNode {

    private static final long serialVersionUID = 1L;

    public DragNode(Modifier modifier) {
        super(modifier);
    }

    @Override
    public ModifierType getModifierType() {
        return ModifierType.MODIFIER_TYPE_DRAG;
    }

    @Override
    public void buildProperties(Builder builder) {}

    @Override
    public String toString() {
        return "Drag";
    }

}
