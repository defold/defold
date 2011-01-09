package com.dynamo.cr.ddfeditor.wizards;

import java.io.ByteArrayInputStream;
import java.io.InputStream;

import com.dynamo.cr.ddfeditor.RenderEditor;


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

    @Override
    InputStream openContentStream() {
        String contents = RenderEditor.newInitialWizardContent().toString();
        return new ByteArrayInputStream(contents.getBytes());
    }
}
