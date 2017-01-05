package com.dynamo.cr.ddfeditor.wizards;

import com.dynamo.cr.editor.ui.AbstractNewDdfWizard;


public class AnimationSetNewWizard extends AbstractNewDdfWizard {
    @Override
    public String getTitle() {
        return "Animationset file";
    }

    @Override
    public String getDescription() {
        return "This wizard creates a new animationset file.";
    }

    @Override
    public String getExtension() {
        return "animationset";
    }

}
