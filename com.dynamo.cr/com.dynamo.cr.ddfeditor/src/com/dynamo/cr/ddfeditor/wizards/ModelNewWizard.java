package com.dynamo.cr.ddfeditor.wizards;

import com.dynamo.cr.editor.ui.AbstractNewDdfWizard;


public class ModelNewWizard extends AbstractNewDdfWizard {
    @Override
    public String getTitle() {
        return "Model file";
    }

    @Override
    public String getDescription() {
        return "This wizard creates a new model file.";
    }

    @Override
    public String getExtension() {
        return "model";
    }

}
