package com.dynamo.cr.ddfeditor.wizards;

import java.io.ByteArrayInputStream;
import java.io.InputStream;

import com.dynamo.cr.ddfeditor.GuiEditor;


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

    @Override
    InputStream openContentStream() {
        String contents = GuiEditor.newInitialWizardContent().toString();
        return new ByteArrayInputStream(contents.getBytes());
    }
}
