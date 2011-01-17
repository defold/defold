package com.dynamo.cr.ddfeditor.wizards;


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
