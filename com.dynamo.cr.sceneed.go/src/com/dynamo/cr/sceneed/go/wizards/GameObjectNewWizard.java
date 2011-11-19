package com.dynamo.cr.sceneed.go.wizards;

import com.dynamo.cr.editor.ui.AbstractNewDdfWizard;


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
