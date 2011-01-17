package com.dynamo.cr.ddfeditor.wizards;


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
