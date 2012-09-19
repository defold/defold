package com.dynamo.cr.ddfeditor.resourcetypes;

import com.dynamo.cr.editor.core.IResourceTypeEditSupport;
import com.dynamo.particle.proto.Particle.Emitter.ParticleProperty;
import com.dynamo.particle.proto.Particle.Emitter.Property;
import com.dynamo.particle.proto.Particle.EmitterKey;
import com.dynamo.particle.proto.Particle.ParticleKey;
import com.dynamo.particle.proto.Particle.PropertySample;
import com.google.protobuf.Descriptors.Descriptor;
import com.google.protobuf.Message;

public class EmitterEditSupport implements IResourceTypeEditSupport {

    public EmitterEditSupport() {
    }

    @Override
    public Message getTemplateMessageFor(Descriptor descriptor) {
        if (descriptor.getFullName().equals(Property.getDescriptor().getFullName())) {
            return Property.newBuilder()
                    .setKey(EmitterKey.EMITTER_KEY_PARTICLE_LIFE_TIME)
                .build();
        }
        else if (descriptor.getFullName().equals(ParticleProperty.getDescriptor().getFullName())) {
            return ParticleProperty.newBuilder()
                    .setKey(ParticleKey.PARTICLE_KEY_SCALE)
                .build();
        }
        else if (descriptor.getFullName().equals(PropertySample.getDescriptor().getFullName())) {
            return PropertySample.newBuilder()
                    .setX(0.0f)
                    .setY(0.0f)
                .build();
        }
        return null;
    }

    @Override
    public String getLabelText(Message message) {
        return "";
    }
}
