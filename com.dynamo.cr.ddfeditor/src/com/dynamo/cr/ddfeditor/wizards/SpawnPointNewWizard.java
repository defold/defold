package com.dynamo.cr.ddfeditor.wizards;


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

}
