package com.dynamo.cr.ddfeditor.wizards;

import java.io.ByteArrayInputStream;
import java.io.InputStream;

import com.dynamo.cr.ddfeditor.CameraEditor;


public class CameraNewWizard extends AbstractNewDdfWizard {
    @Override
    public String getTitle() {
        return "Camera file";
    }

    @Override
    public String getDescription() {
        return "This wizard creates a new camera file.";
    }

    @Override
    public String getExtension() {
        return "camera";
    }

    @Override
    InputStream openContentStream() {
        String contents = CameraEditor.newInitialWizardContent().toString();
        return new ByteArrayInputStream(contents.getBytes());
    }
}
