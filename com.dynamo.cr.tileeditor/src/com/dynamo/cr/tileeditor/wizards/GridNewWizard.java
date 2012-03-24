package com.dynamo.cr.tileeditor.wizards;


public class GridNewWizard extends AbstractNewWizard {

    @Override
    public String getTitle() {
        return "Tile Map file";
    }

    @Override
    public String getDescription() {
        return "This wizard creates a new tile map file.";
    }

    @Override
    public String getExtension() {
        return "tilemap";
    }
}
