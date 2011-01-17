package com.dynamo.cr.ddfeditor;

import com.dynamo.gamesystem.proto.GameSystem.LightDesc;

public class LightEditor extends DdfEditor {

    public LightEditor() {
        super(LightDesc.newBuilder().buildPartial());
    }

}
