package com.dynamo.cr.tileeditor.wizards;

import com.dynamo.cr.editor.ui.AbstractNewDdfWizard;


public class GridNewWizard extends AbstractNewDdfWizard {

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
