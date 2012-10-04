package com.dynamo.cr.parted.nodes;

import javax.vecmath.Quat4d;
import javax.vecmath.Vector4d;

import com.dynamo.cr.parted.curve.HermiteSpline;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.types.ValueSpread;
import com.dynamo.cr.sceneed.core.util.LoaderUtil;
import com.dynamo.particle.proto.Particle.Modifier;
import com.dynamo.particle.proto.Particle.Modifier.Builder;
import com.dynamo.particle.proto.Particle.ModifierKey;
import com.dynamo.particle.proto.Particle.ModifierType;

public abstract class AbstractModifierNode extends ModifierNode {

    private static final long serialVersionUID = 1L;

    @Property
    protected ValueSpread magnitude = new ValueSpread(new HermiteSpline());

    public AbstractModifierNode(Modifier modifier) {
        setTransformable(true);
        setTranslation(LoaderUtil.toPoint3d(modifier.getPosition()));
        setRotation(LoaderUtil.toQuat4(modifier.getRotation()));
    }

    public AbstractModifierNode(Vector4d translation, Quat4d rotation) {
        super(translation, rotation);
    }

    public ValueSpread getMagnitude() {
        return new ValueSpread(magnitude);
    }

    public void setMagnitude(ValueSpread magnitude) {
        this.magnitude.set(magnitude);
        reloadSystem();
    }

    @Override
    protected void transformChanged() {
        reloadSystem();
    }

    protected void reloadSystem() {
        if (getParent() != null) {
            ParticleFXNode parent = (ParticleFXNode) getParent().getParent();
            if (parent != null) {
                parent.reload();
            }
        }
    }

    public abstract ModifierType getModifierType();
    public abstract void buildProperties(Modifier.Builder builder);

    @Override
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

}