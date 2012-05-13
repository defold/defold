package com.dynamo.cr.tileeditor.wizards;

import com.dynamo.cr.editor.ui.AbstractNewDdfWizard;


public class TileSetNewWizard extends AbstractNewDdfWizard {

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
