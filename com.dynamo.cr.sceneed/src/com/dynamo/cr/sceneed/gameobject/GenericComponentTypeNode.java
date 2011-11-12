package com.dynamo.cr.sceneed.gameobject;

public class GenericComponentTypeNode extends ComponentTypeNode {

    private String typeName;

    public GenericComponentTypeNode(String typeName) {
        this.typeName = typeName;
    }

    @Override
    public String getTypeName() {
        return this.typeName;
    }

    @Override
    public String toString() {
        return getTypeName();
    }
}
