package com.dynamo.cr.tileeditor.wizards;


public class TileSet2NewWizard extends AbstractNewWizard {

    @Override
    public String getTitle() {
        return "Tile Set 2 file";
    }

    @Override
    public String getDescription() {
        return "This wizard creates a new tile set 2 file.";
    }

    @Override
    public String getExtension() {
        return "tileset2";
    }
}
