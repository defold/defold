package com.dynamo.cr.ddfeditor.wizards;

import com.dynamo.cr.editor.ui.AbstractNewDdfWizard;


public class CollisionObjectNewWizard extends AbstractNewDdfWizard {
    @Override
    public String getTitle() {
        return "Collision object file";
    }

    @Override
    public String getDescription() {
        return "This wizard creates a new collsion object file.";
    }

    @Override
    public String getExtension() {
        return "collisionobject";
    }

}
