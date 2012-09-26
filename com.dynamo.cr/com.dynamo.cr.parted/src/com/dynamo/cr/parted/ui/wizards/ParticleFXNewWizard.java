package com.dynamo.cr.parted.ui.wizards;

import com.dynamo.cr.editor.ui.AbstractNewDdfWizard;

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

}
