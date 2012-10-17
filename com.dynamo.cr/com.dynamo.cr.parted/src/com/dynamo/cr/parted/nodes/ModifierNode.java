package com.dynamo.cr.parted.nodes;

import org.eclipse.swt.graphics.Image;

import com.dynamo.cr.parted.ParticleEditorPlugin;
import com.dynamo.cr.sceneed.core.AABB;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.particle.proto.Particle.Modifier;

public abstract class ModifierNode extends Node {

    private static final long serialVersionUID = 1L;

    public ModifierNode() {
        super();
        updateAABB();
    }

    protected void updateAABB() {
        AABB aabb = new AABB();
        // NOTE: This is arbitrary as modifiers are rendered with constant screen-space size.
        int s = 10;
        aabb.union(-s, -s, -s);
        aabb.union(s, s, s);
        setAABB(aabb);
    }

    public abstract Modifier buildMessage();

    @Override
    public Image getIcon() {
        return ParticleEditorPlugin.getDefault().getImageRegistry().get(ParticleEditorPlugin.MODIFIER_IMAGE_ID);
    }

    protected void reloadSystem() {
        Node parent = getParent();
        if (parent != null) {
            if (parent instanceof EmitterNode) {
                EmitterNode e = (EmitterNode)parent;
                e.reloadSystem();
            } else {
                ParticleFXNode p = (ParticleFXNode) parent;
                p.reload();
            }
        }
    }
}