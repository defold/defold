package com.dynamo.cr.ddfeditor.wizards;

import com.dynamo.cr.editor.ui.AbstractNewDdfWizard;

public class LabelNewWizard extends AbstractNewDdfWizard {
    @Override
    public String getTitle() {
        return "Label file";
    }

    @Override
    public String getDescription() {
        return "This wizard creates a new label file.";
    }

    @Override
    public String getExtension() {
        return "label";
    }

}
