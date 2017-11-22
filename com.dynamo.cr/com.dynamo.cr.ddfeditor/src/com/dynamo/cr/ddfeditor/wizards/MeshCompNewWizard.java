package com.dynamo.cr.ddfeditor.wizards;

import com.dynamo.cr.editor.ui.AbstractNewDdfWizard;


public class MeshCompNewWizard extends AbstractNewDdfWizard {
    @Override
    public String getTitle() {
        return "Mesh Comp file";
    }

    @Override
    public String getDescription() {
        return "This wizard creates a new mesh file.";
    }

    @Override
    public String getExtension() {
        return "mesh";
    }

}
