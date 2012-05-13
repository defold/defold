package com.dynamo.cr.ddfeditor.wizards;

import com.dynamo.cr.editor.ui.AbstractNewDdfWizard;

public class RenderNewWizard extends AbstractNewDdfWizard {
    @Override
    public String getTitle() {
        return "Render file";
    }

    @Override
    public String getDescription() {
        return "This wizard creates a new render file.";
    }

    @Override
    public String getExtension() {
        return "render";
    }

}
