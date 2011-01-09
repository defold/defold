package com.dynamo.cr.ddfeditor;

import com.dynamo.gamesystem.proto.GameSystem.SpawnPointDesc;
import com.google.protobuf.Message;

public class SpawnPointEditor extends DdfEditor {

    public SpawnPointEditor() {
        super(SpawnPointDesc.newBuilder().buildPartial());
    }

    public static Message newInitialWizardContent() {
        return SpawnPointDesc.newBuilder()
            .setPrototype("")
            .build();
    }
}
