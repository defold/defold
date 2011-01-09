package com.dynamo.cr.ddfeditor.wizards;

import java.io.ByteArrayInputStream;
import java.io.InputStream;

import com.dynamo.cr.ddfeditor.LightEditor;


public class LightNewWizard extends AbstractNewDdfWizard {
    @Override
    public String getTitle() {
        return "Light file";
    }

    @Override
    public String getDescription() {
        return "This wizard creates a new light file.";
    }

    @Override
    public String getExtension() {
        return "light";
    }

    @Override
    InputStream openContentStream() {
        String contents = LightEditor.newInitialWizardContent().toString();
        return new ByteArrayInputStream(contents.getBytes());
    }
}
