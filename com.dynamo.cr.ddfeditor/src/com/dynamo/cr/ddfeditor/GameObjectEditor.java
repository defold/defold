package com.dynamo.cr.ddfeditor;

import com.dynamo.gameobject.proto.GameObject.PrototypeDesc;

public class GameObjectEditor extends DdfEditor {

    public GameObjectEditor() {
        super(PrototypeDesc.newBuilder().buildPartial());
    }
}

