package com.dynamo.cr.ddfeditor.wizards;


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

}
