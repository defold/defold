package com.dynamo.cr.sceneed.ui.gameobject;

public class GenericComponentTypeNode extends ComponentTypeNode {

    private String typeId;
    private String typeName;

    public GenericComponentTypeNode(String typeId) {
        super();
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
