package com.dynamo.cr.parted.nodes;

import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.particle.proto.Particle.EmissionSpace;
import com.dynamo.particle.proto.Particle.Modifier;

public abstract class RotationalModifierNode extends AbstractModifierNode {

    private static final long serialVersionUID = 1L;

    public RotationalModifierNode(Modifier modifier) {
        super(modifier);
        setFlags(Flags.NO_INHERIT_ROTATION);
    }

    @Override
    public void parentSet() {
        Node parent = getParent();
        if (parent instanceof EmitterNode) {
            EmitterNode emitter = (EmitterNode)parent;
            if (emitter.getEmissionSpace().equals(EmissionSpace.EMISSION_SPACE_WORLD)) {
                setFlags(Flags.NO_INHERIT_ROTATION);
            } else {
                clearFlags(Flags.NO_INHERIT_ROTATION);
            }
        }
    }
}
