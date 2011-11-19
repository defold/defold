package com.dynamo.cr.ddfeditor.wizards;

import com.dynamo.cr.editor.ui.AbstractNewDdfWizard;


public class MaterialNewWizard extends AbstractNewDdfWizard {
    @Override
    public String getTitle() {
        return "Material file";
    }

    @Override
    public String getDescription() {
        return "This wizard creates a new material file.";
    }

    @Override
    public String getExtension() {
        return "material";
    }

}
