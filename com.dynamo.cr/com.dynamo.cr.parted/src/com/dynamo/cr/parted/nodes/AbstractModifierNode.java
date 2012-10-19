package com.dynamo.cr.parted.nodes;

import org.eclipse.swt.graphics.Image;

import com.dynamo.cr.parted.ParticleEditorPlugin;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.sceneed.core.AABB;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.util.LoaderUtil;
import com.dynamo.particle.proto.Particle.Modifier;
import com.dynamo.particle.proto.Particle.Modifier.Builder;
import com.dynamo.particle.proto.Particle.ModifierKey;
import com.dynamo.particle.proto.Particle.ModifierType;

public abstract class AbstractModifierNode extends Node {

    private static final long serialVersionUID = 1L;

    @Property
    protected double magnitude = 0.0f;

    public AbstractModifierNode(Modifier modifier) {
        super();
        updateAABB();
        setTransformable(true);
        setTranslation(LoaderUtil.toPoint3d(modifier.getPosition()));
        setRotation(LoaderUtil.toQuat4(modifier.getRotation()));

        for (Modifier.Property p : modifier.getPropertiesList()) {
            switch (p.getKey()) {
            case MODIFIER_KEY_MAGNITUDE:
                magnitude = p.getPointsList().get(0).getY();
                break;
            }
        }
    }

    protected void updateAABB() {
        AABB aabb = new AABB();
        // NOTE: This is arbitrary as modifiers are rendered with constant screen-space size.
        int s = 10;
        aabb.union(-s, -s, -s);
        aabb.union(s, s, s);
        setAABB(aabb);
    }

    public double getMagnitude() {
        return magnitude;
    }

    public void setMagnitude(double magnitude) {
        this.magnitude = magnitude;
        reloadSystem();
    }

    @Override
    protected void transformChanged() {
        reloadSystem();
    }

    public abstract ModifierType getModifierType();
    public abstract void buildProperties(Modifier.Builder builder);

    public Modifier buildMessage() {
        Builder b = Modifier.newBuilder()
            .setType(getModifierType())
            .setUseDirection(0)
            .setPosition(LoaderUtil.toPoint3(getTranslation()))
            .setRotation(LoaderUtil.toQuat(getRotation()))
            .addProperties(Modifier.Property.newBuilder()
                    .setKey(ModifierKey.MODIFIER_KEY_MAGNITUDE)
                    .addAllPoints(ParticleUtils.toSplinePointList(magnitude)));
        buildProperties(b);
        return b.build();
    }

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