package com.dynamo.cr.tileeditor.wizards;


public class GridNewWizard extends AbstractNewWizard {

    @Override
    public String getTitle() {
        return "Tile Grid file";
    }

    @Override
    public String getDescription() {
        return "This wizard creates a new tile grid file.";
    }

    @Override
    public String getExtension() {
        return "tilegrid";
    }
}
