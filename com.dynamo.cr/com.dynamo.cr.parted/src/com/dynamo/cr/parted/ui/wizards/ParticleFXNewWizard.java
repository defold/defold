package com.dynamo.cr.parted.ui.wizards;

import com.dynamo.cr.editor.core.EditorCorePlugin;
import com.dynamo.cr.editor.core.IResourceType;
import com.dynamo.cr.editor.core.IResourceTypeRegistry;
import com.dynamo.cr.editor.ui.AbstractNewDdfWizard;
import com.dynamo.particle.proto.Particle.Emitter;
import com.dynamo.particle.proto.Particle.ParticleFX;
import com.google.protobuf.Message;

public class ParticleFXNewWizard extends AbstractNewDdfWizard {
    @Override
    public String getTitle() {
        return "ParticleFX file";
    }

    @Override
    public String getDescription() {
        return "This wizard creates a new particle-fx file.";
    }

    @Override
    public String getExtension() {
        return "particlefx";
    }

    @Override
    protected Message createTemplateMessage() {
        ParticleFX particle = (ParticleFX)super.createTemplateMessage();
        ParticleFX.Builder b = ParticleFX.newBuilder(particle);
        IResourceTypeRegistry registry = EditorCorePlugin.getDefault().getResourceTypeRegistry();
        IResourceType resourceType = registry.getResourceTypeFromExtension("emitter");
        b.addEmitters((Emitter)resourceType.createTemplateMessage());
        return b.build();
    }
}
