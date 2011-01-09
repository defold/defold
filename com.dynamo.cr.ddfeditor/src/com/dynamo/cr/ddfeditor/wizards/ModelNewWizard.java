package com.dynamo.cr.ddfeditor.wizards;

import java.io.ByteArrayInputStream;
import java.io.InputStream;

import com.dynamo.cr.ddfeditor.ModelEditor;


public class ModelNewWizard extends AbstractNewDdfWizard {
    @Override
    public String getTitle() {
        return "Model file";
    }

    @Override
    public String getDescription() {
        return "This wizard creates a new model file.";
    }

    @Override
    public String getExtension() {
        return "model";
    }

    @Override
    InputStream openContentStream() {
        String contents = ModelEditor.newInitialWizardContent().toString();
        return new ByteArrayInputStream(contents.getBytes());
    }
}
