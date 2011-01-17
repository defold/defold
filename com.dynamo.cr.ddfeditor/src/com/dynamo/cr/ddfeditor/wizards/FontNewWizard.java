package com.dynamo.cr.ddfeditor.wizards;


public class FontNewWizard extends AbstractNewDdfWizard {
    @Override
    public String getTitle() {
        return "Font file";
    }

    @Override
    public String getDescription() {
        return "This wizard creates a new font file.";
    }

    @Override
    public String getExtension() {
        return "font";
    }

}
