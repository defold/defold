package com.dynamo.cr.ddfeditor.wizards;

import com.dynamo.cr.editor.ui.AbstractNewDdfWizard;


public class ConvexShapeNewWizard extends AbstractNewDdfWizard {
    @Override
    public String getTitle() {
        return "Convext Shape file";
    }

    @Override
    public String getDescription() {
        return "This wizard creates a new convexshape file.";
    }

    @Override
    public String getExtension() {
        return "convexshape";
    }

}
