package com.dynamo.cr.ddfeditor.resourcetypes;

import com.dynamo.cr.editor.core.IResourceTypeEditSupport;
import com.dynamo.particle.proto.Particle.Emitter.Modifier;
import com.dynamo.particle.proto.Particle.Emitter.Modifier.InputType;
import com.dynamo.particle.proto.Particle.Emitter.ParticleModifier;
import com.dynamo.particle.proto.Particle.Emitter.Property;
import com.dynamo.particle.proto.Particle.EmitterKey;
import com.dynamo.particle.proto.Particle.ModifierKey;
import com.dynamo.particle.proto.Particle.ModifierProperty;
import com.dynamo.particle.proto.Particle.ModifierType;
import com.dynamo.particle.proto.Particle.ParticleKey;
import com.google.protobuf.Descriptors.Descriptor;
import com.google.protobuf.Message;

public class EmitterEditSupport implements IResourceTypeEditSupport {

    public EmitterEditSupport() {
    }

    @Override
    public Message getTemplateMessageFor(Descriptor descriptor) {
        if (descriptor.getFullName().equals(Modifier.getDescriptor().getFullName())) {
            return Modifier.newBuilder()
                .setType(ModifierType.MODIFIER_TYPE_LINEAR)
                .setInput(InputType.INPUT_TYPE_GAIN)
                .build();
        }
        else if (descriptor.getFullName().equals(Property.getDescriptor().getFullName())) {
            return Property.newBuilder()
                .setKey(EmitterKey.EMITTER_KEY_PARTICLE_ALPHA)
                .setValue(1)
                .build();
        }
        else if (descriptor.getFullName().equals(ParticleModifier.getDescriptor().getFullName())) {
            return ParticleModifier.newBuilder()
                .setType(ModifierType.MODIFIER_TYPE_LINEAR)
                .setModify(ParticleKey.PARTICLE_KEY_ALPHA)
                .build();
        }
        else if (descriptor.getFullName().equals(ModifierProperty.getDescriptor().getFullName())) {
            return ModifierProperty.newBuilder()
                .setKey(ModifierKey.MODIFIER_KEY_MIN)
                .setValue(1)
                .build();
        }
        return null;
    }

    @Override
    public String getLabelText(Message message) {
        return "";
    }
}
