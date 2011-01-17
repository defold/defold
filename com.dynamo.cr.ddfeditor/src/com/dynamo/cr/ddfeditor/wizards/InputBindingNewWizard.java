package com.dynamo.cr.ddfeditor.wizards;


public class InputBindingNewWizard extends AbstractNewDdfWizard {
    @Override
    public String getTitle() {
        return "Input binding file";
    }

    @Override
    public String getDescription() {
        return "This wizard creates a new input binding file.";
    }

    @Override
    public String getExtension() {
        return "input_binding";
    }

}
