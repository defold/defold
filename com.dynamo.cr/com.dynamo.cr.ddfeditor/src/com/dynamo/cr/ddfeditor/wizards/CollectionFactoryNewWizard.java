package com.dynamo.cr.ddfeditor.wizards;

import com.dynamo.cr.editor.ui.AbstractNewDdfWizard;


public class CollectionFactoryNewWizard extends AbstractNewDdfWizard {
    @Override
    public String getTitle() {
        return "Collection Factory file";
    }

    @Override
    public String getDescription() {
        return "This wizard creates a new collection factory file.";
    }

    @Override
    public String getExtension() {
        return "collectionfactory";
    }

}
