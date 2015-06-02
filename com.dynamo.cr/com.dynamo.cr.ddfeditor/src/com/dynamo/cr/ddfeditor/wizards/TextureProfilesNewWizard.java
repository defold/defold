package com.dynamo.cr.ddfeditor.wizards;

import com.dynamo.cr.editor.ui.AbstractNewDdfWizard;

public class TextureProfilesNewWizard extends AbstractNewDdfWizard {
    @Override
    public String getTitle() {
        return "Texture Profiles file";
    }

    @Override
    public String getDescription() {
        return "This wizard creates a new texture profiles file.";
    }

    @Override
    public String getExtension() {
        return "texture_profiles";
    }

}
