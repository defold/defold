package com.dynamo.cr.sceneed.gameobject;

public class GenericComponentTypeNode extends ComponentTypeNode {

    private String typeId;
    private String typeName;

    public GenericComponentTypeNode(String typeId) {
        this.typeId = typeId;
        this.typeName = typeId;
    }

    @Override
    public String getTypeId() {
        return this.typeId;
    }

    @Override
    public String getTypeName() {
        return this.typeName;
    }

}
