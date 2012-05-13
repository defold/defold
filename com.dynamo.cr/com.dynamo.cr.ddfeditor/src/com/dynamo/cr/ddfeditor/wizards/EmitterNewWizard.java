package com.dynamo.cr.ddfeditor.wizards;

import com.dynamo.cr.editor.ui.AbstractNewDdfWizard;


public class EmitterNewWizard extends AbstractNewDdfWizard {
    @Override
    public String getTitle() {
        return "Emitter file";
    }

    @Override
    public String getDescription() {
        return "This wizard creates a new emitter file.";
    }

    @Override
    public String getExtension() {
        return "emitter";
    }

}
