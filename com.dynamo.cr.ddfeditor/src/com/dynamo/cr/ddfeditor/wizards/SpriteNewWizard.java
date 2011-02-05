package com.dynamo.cr.ddfeditor.wizards;


public class SpriteNewWizard extends AbstractNewDdfWizard {
    @Override
    public String getTitle() {
        return "Sprite file";
    }

    @Override
    public String getDescription() {
        return "This wizard creates a new sprite file.";
    }

    @Override
    public String getExtension() {
        return "sprite";
    }

}
