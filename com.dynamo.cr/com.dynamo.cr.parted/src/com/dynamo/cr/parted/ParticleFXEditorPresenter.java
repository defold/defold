package com.dynamo.cr.parted;

import javax.annotation.PreDestroy;

import org.eclipse.core.runtime.CoreException;

import com.dynamo.cr.editor.core.ProjectProperties;
import com.dynamo.cr.parted.nodes.ParticleFXNode;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.ScenePresenter;
import com.sun.jna.Pointer;

public class ParticleFXEditorPresenter extends ScenePresenter {

    private Pointer context;
    /// Only set when there is a particle FX node as root
    private ParticleFXNode root;

    private static final String PARTICLE_FX_CATEGORY = "particle_fx";
    private static final String MAX_PARTICLE_COUNT_KEY = "max_particle_count";
    private static final int MAX_PARTICLE_COUNT_DEFAULT = 1024;

    @PreDestroy
    public void dispose() {
        if (this.context != null) {
            ParticleLibrary.Particle_DestroyContext(this.context);
        }
    }

    @Override
    public void onProjectPropertiesChanged(ProjectProperties properties) throws CoreException {
        super.onProjectPropertiesChanged(properties);
        Integer maxParticleCount = properties.getIntValue(PARTICLE_FX_CATEGORY, MAX_PARTICLE_COUNT_KEY);
        if (maxParticleCount == null)
            maxParticleCount = MAX_PARTICLE_COUNT_DEFAULT;
        if (this.context == null) {
            // Ignore instance count in project properties, since we currently only have one instance
            int instanceCount = 1;
            this.context = ParticleLibrary.Particle_CreateContext(instanceCount, maxParticleCount);
        } else {
            ParticleLibrary.Particle_SetContextMaxParticleCount(this.context, maxParticleCount);
        }
    }

    @Override
    public void rootChanged(Node root) {
        if (this.root != null) {
            this.root.unbindContext();
        }
        ParticleFXNode particleFX = (ParticleFXNode)root;
        particleFX.bindContext(this.context);
        this.root = particleFX;

        super.rootChanged(root);
    }
}
