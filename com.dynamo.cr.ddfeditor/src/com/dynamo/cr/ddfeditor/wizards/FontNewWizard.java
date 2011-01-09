package com.dynamo.cr.ddfeditor.wizards;

import java.io.ByteArrayInputStream;
import java.io.InputStream;

import com.dynamo.cr.ddfeditor.FontEditor;


public class FontNewWizard extends AbstractNewDdfWizard {
    @Override
    public String getTitle() {
        return "Font file";
    }

    @Override
    public String getDescription() {
        return "This wizard creates a new font file.";
    }

    @Override
    public String getExtension() {
        return "font";
    }

    @Override
    InputStream openContentStream() {
        String contents = FontEditor.newInitialWizardContent().toString();
        return new ByteArrayInputStream(contents.getBytes());
    }
}
