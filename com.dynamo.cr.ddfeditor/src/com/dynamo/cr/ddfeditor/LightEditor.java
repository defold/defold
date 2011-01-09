package com.dynamo.cr.ddfeditor;

import com.dynamo.gamesystem.proto.GameSystem.LightDesc;
import com.dynamo.gamesystem.proto.GameSystem.LightType;
import com.dynamo.proto.DdfMath.Vector3;
import com.google.protobuf.Message;

public class LightEditor extends DdfEditor {

    public LightEditor() {
        super(LightDesc.newBuilder().buildPartial());
    }

    public static Message newInitialWizardContent() {
        return LightDesc.newBuilder()
            .setId("unnamed")
            .setType(LightType.POINT)
            .setIntensity(10)
            .setColor(Vector3.newBuilder().setX(1).setY(1).setZ(1))
            .setRange(50)
            .setDecay(0.9f)
            .build();
    }
}
