package com.dynamo.cr.tileeditor.wizards;

import com.dynamo.cr.editor.ui.AbstractNewDdfWizard;


public class AtlasNewWizard extends AbstractNewDdfWizard {
    @Override
    public String getTitle() {
        return "Atlas file";
    }

    @Override
    public String getDescription() {
        return "This wizard creates a new atlas file.";
    }

    @Override
    public String getExtension() {
        return "atlas";
    }

}
