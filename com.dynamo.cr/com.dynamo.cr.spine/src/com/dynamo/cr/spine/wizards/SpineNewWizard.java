package com.dynamo.cr.spine.wizards;

import com.dynamo.cr.editor.ui.AbstractNewDdfWizard;


public class SpineNewWizard extends AbstractNewDdfWizard {
    @Override
    public String getTitle() {
        return "Spine Model file";
    }

    @Override
    public String getDescription() {
        return "This wizard creates a new spine model file.";
    }

    @Override
    public String getExtension() {
        return "spinemodel";
    }

}
