package com.dynamo.cr.parted.nodes;

import javax.vecmath.Quat4d;
import javax.vecmath.Vector4d;

import com.dynamo.cr.sceneed.core.AABB;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.particle.proto.Particle.Modifier;

public abstract class ModifierNode extends Node {

    private static final long serialVersionUID = 1L;

    public ModifierNode() {
        super();
        setFlags(Flags.NO_INHERIT_TRANSFORM);
        updateAABB();
    }

    public ModifierNode(Vector4d translation, Quat4d rotation) {
        super(translation, rotation);
        setFlags(Flags.NO_INHERIT_TRANSFORM);
        updateAABB();
    }

    private void updateAABB() {
        AABB aabb = new AABB();
        // NOTE: This is arbitrary as modifiers are rendered with constant screen-space size.
        int s = 10;
        aabb.union(-s, -s, -s);
        aabb.union(s, s, s);
        setAABB(aabb);
    }

    public abstract Modifier buildMessage();

}