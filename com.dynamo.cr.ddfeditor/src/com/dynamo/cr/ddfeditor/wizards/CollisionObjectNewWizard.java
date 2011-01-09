package com.dynamo.cr.ddfeditor.wizards;

import java.io.ByteArrayInputStream;
import java.io.InputStream;

import com.dynamo.cr.ddfeditor.CollisionObjectEditor;


public class CollisionObjectNewWizard extends AbstractNewDdfWizard {
    @Override
    public String getTitle() {
        return "Collision object file";
    }

    @Override
    public String getDescription() {
        return "This wizard creates a new collsion object file.";
    }

    @Override
    public String getExtension() {
        return "collisionobject";
    }

    @Override
    InputStream openContentStream() {
        String contents = CollisionObjectEditor.newInitialWizardContent().toString();
        return new ByteArrayInputStream(contents.getBytes());
    }
}
