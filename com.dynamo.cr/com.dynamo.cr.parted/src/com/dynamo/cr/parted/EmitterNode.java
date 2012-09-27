package com.dynamo.cr.parted;

import java.util.ArrayList;
import java.util.List;

import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.util.LoaderUtil;
import com.dynamo.particle.proto.Particle.EmissionSpace;
import com.dynamo.particle.proto.Particle.Emitter;
import com.dynamo.particle.proto.Particle.EmitterType;
import com.dynamo.particle.proto.Particle.PlayMode;
import com.dynamo.particle.proto.Particle.Texture_t;

public class EmitterNode extends Node {

    private static final long serialVersionUID = 1L;

    @Property(editorType = EditorType.DROP_DOWN)
    private PlayMode playMode;

    @Property(editorType = EditorType.DROP_DOWN)
    private EmissionSpace emissionSpace;

    @Property
    private float duration;

    @Property(editorType = EditorType.RESOURCE)
    private String material;

    @Property
    private int maxParticleCount;

    @Property
    private EmitterType emitterType;

    private ArrayList<Emitter.Property> properties;
    private ArrayList<Emitter.ParticleProperty> particleProperties;

    public EmitterNode(Emitter emitter) {
        setPlayMode(emitter.getMode());
        setDuration(emitter.getDuration());
        setEmissionSpace(emitter.getSpace());
        setMaterial(emitter.getMaterial());
        setMaxParticleCount(emitter.getMaxParticleCount());
        setEmitterType(emitter.getType());

        setProperties(emitter.getPropertiesList());
        setParticleProperties(emitter.getParticlePropertiesList());
    }

    private void resetSystem() {
        ParticleFXNode parent = (ParticleFXNode) getParent();
        if (parent != null) {
            parent.reset();
        }
    }

    private void setProperties(List<Emitter.Property> list) {
        this.properties = new ArrayList<Emitter.Property>(list);
    }

    private void setParticleProperties(List<Emitter.ParticleProperty> particleProperties) {
        this.particleProperties = new ArrayList<Emitter.ParticleProperty>(particleProperties);
    }

    public PlayMode getPlayMode() {
        return playMode;
    }

    public void setPlayMode(PlayMode playMode) {
        this.playMode = playMode;
        resetSystem();
    }

    public EmissionSpace getEmissionSpace() {
        return emissionSpace;
    }

    public void setEmissionSpace(EmissionSpace emissionSpace) {
        this.emissionSpace = emissionSpace;
        resetSystem();
    }

    public float getDuration() {
        return duration;
    }

    public void setDuration(float duration) {
        this.duration = duration;
        resetSystem();
    }

    public String getMaterial() {
        return material;
    }

    public void setMaterial(String material) {
        this.material = material;
        resetSystem();
    }

    public int getMaxParticleCount() {
        return maxParticleCount;
    }

    public void setMaxParticleCount(int maxParticleCount) {
        this.maxParticleCount = maxParticleCount;
        resetSystem();
    }

    public EmitterType getEmitterType() {
        return emitterType;
    }

    public void setEmitterType(EmitterType emitterType) {
        this.emitterType = emitterType;
        resetSystem();
    }

    @Override
    public String toString() {
        return "Emitter";
    }

    public Emitter.Builder buildMessage() {
        return Emitter.newBuilder()
            .setMode(getPlayMode())
            .setDuration(getDuration())
            .setSpace(getEmissionSpace())
            .setMaterial(getMaterial())
            .setMaxParticleCount(getMaxParticleCount())
            .setType(getEmitterType())
            .setPosition(LoaderUtil.toPoint3(getTranslation()))
            .setRotation(LoaderUtil.toQuat(getRotation()))
            .setTexture(Texture_t.newBuilder().setName("TODO").setTX(16).setTY(16))
            .addAllProperties(this.properties)
            .addAllParticleProperties(this.particleProperties);
    }

}
