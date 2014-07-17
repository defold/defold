package com.dynamo.cr.spine.wizards;

import com.dynamo.cr.editor.ui.AbstractNewDdfWizard;


public class SpineSceneNewWizard extends AbstractNewDdfWizard {
    @Override
    public String getTitle() {
        return "Spine Scene file";
    }

    @Override
    public String getDescription() {
        return "This wizard creates a new spine scene file.";
    }

    @Override
    public String getExtension() {
        return "spinescene";
    }

}
