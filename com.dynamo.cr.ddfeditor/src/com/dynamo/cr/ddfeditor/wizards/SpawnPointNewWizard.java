package com.dynamo.cr.ddfeditor.wizards;

import java.io.ByteArrayInputStream;
import java.io.InputStream;

import com.dynamo.cr.ddfeditor.SpawnPointEditor;


public class SpawnPointNewWizard extends AbstractNewDdfWizard {
    @Override
    public String getTitle() {
        return "Spawn point file";
    }

    @Override
    public String getDescription() {
        return "This wizard creates a new spawn point file.";
    }

    @Override
    public String getExtension() {
        return "spawnpoint";
    }

    @Override
    InputStream openContentStream() {
        String contents = SpawnPointEditor.newInitialWizardContent().toString();
        return new ByteArrayInputStream(contents.getBytes());
    }
}
