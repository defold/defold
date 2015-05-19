package com.dynamo.cr.ddfeditor.wizards;

import com.dynamo.cr.editor.ui.AbstractNewDdfWizard;

public class DisplayProfilesNewWizard extends AbstractNewDdfWizard {
    @Override
    public String getTitle() {
        return "Display Profiles file";
    }

    @Override
    public String getDescription() {
        return "This wizard creates a new display profiles file.";
    }

    @Override
    public String getExtension() {
        return "display_profiles";
    }

}
