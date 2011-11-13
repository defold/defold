package com.dynamo.cr.sceneed.gameobject;

import com.dynamo.cr.sceneed.core.NodePresenter;

public class GenericComponentTypeNode extends ComponentTypeNode {

    private String typeId;
    private String typeName;

    public GenericComponentTypeNode(NodePresenter presenter, String typeId) {
        super(presenter);
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
