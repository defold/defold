package com.dynamo.cr.ddfeditor.wizards;

import com.dynamo.cr.editor.ui.AbstractNewDdfWizard;


public class FactoryNewWizard extends AbstractNewDdfWizard {
    @Override
    public String getTitle() {
        return "Factory file";
    }

    @Override
    public String getDescription() {
        return "This wizard creates a new factory file.";
    }

    @Override
    public String getExtension() {
        return "factory";
    }

}
