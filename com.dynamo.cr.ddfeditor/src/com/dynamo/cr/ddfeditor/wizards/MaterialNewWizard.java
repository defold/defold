package com.dynamo.cr.ddfeditor.wizards;

import java.io.ByteArrayInputStream;
import java.io.InputStream;

import com.dynamo.cr.ddfeditor.MaterialEditor;


public class MaterialNewWizard extends AbstractNewDdfWizard {
    @Override
    public String getTitle() {
        return "Material file";
    }

    @Override
    public String getDescription() {
        return "This wizard creates a new material file.";
    }

    @Override
    public String getExtension() {
        return "material";
    }

    @Override
    InputStream openContentStream() {
        String contents = MaterialEditor.newInitialWizardContent().toString();
        return new ByteArrayInputStream(contents.getBytes());
    }
}
