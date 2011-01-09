package com.dynamo.cr.ddfeditor.wizards;

import java.io.ByteArrayInputStream;
import java.io.InputStream;

import com.dynamo.cr.ddfeditor.InputBindingEditor;


public class InputBindingNewWizard extends AbstractNewDdfWizard {
    @Override
    public String getTitle() {
        return "Input binding file";
    }

    @Override
    public String getDescription() {
        return "This wizard creates a new input binding file.";
    }

    @Override
    public String getExtension() {
        return "input_binding";
    }

    @Override
    InputStream openContentStream() {
        String contents = InputBindingEditor.newInitialWizardContent().toString();
        return new ByteArrayInputStream(contents.getBytes());
    }
}
