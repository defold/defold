package com.dynamo.cr.ddfeditor;

import com.dynamo.gamesystem.proto.GameSystem.SpawnPointDesc;

public class SpawnPointEditor extends DdfEditor {

    public SpawnPointEditor() {
        super(SpawnPointDesc.newBuilder().buildPartial());
    }

    @Override
    public String getTitle() {
        return "Spawn Point";
    }

}
