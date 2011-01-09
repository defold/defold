package com.dynamo.cr.ddfeditor.wizards;

import java.io.ByteArrayInputStream;
import java.io.InputStream;

import com.dynamo.cr.ddfeditor.GameObjectEditor;


public class GameObjectNewWizard extends AbstractNewDdfWizard {
    @Override
    public String getTitle() {
        return "Game object file";
    }

    @Override
    public String getDescription() {
        return "This wizard creates a new game object file.";
    }

    @Override
    public String getExtension() {
        return "go";
    }

    @Override
    InputStream openContentStream() {
        String contents = GameObjectEditor.newInitialWizardContent().toString();
        return new ByteArrayInputStream(contents.getBytes());
    }
}
