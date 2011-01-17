package com.dynamo.cr.ddfeditor.wizards;


public class GuiNewWizard extends AbstractNewDdfWizard {
    @Override
    public String getTitle() {
        return "Gui file";
    }

    @Override
    public String getDescription() {
        return "This wizard creates a new gui file.";
    }

    @Override
    public String getExtension() {
        return "gui";
    }

}
