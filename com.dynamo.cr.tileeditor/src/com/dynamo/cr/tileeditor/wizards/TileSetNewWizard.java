package com.dynamo.cr.tileeditor.wizards;


public class TileSetNewWizard extends AbstractNewWizard {

    @Override
    public String getTitle() {
        return "Tile Source file";
    }

    @Override
    public String getDescription() {
        return "This wizard creates a new tile source file.";
    }

    @Override
    public String getExtension() {
        return "tilesource";
    }
}
