package com.dynamo.cr.ddfeditor.wizards;

import com.dynamo.cr.editor.ui.AbstractNewDdfWizard;


public class CubemapNewWizard extends AbstractNewDdfWizard {
    @Override
    public String getTitle() {
        return "Cubemap file";
    }

    @Override
    public String getDescription() {
        return "This wizard creates a new cubemap file.";
    }

    @Override
    public String getExtension() {
        return "cubemap";
    }

}
