package com.dynamo.cr.ddfeditor;

import com.dynamo.camera.proto.Camera.CameraDesc;
import com.google.protobuf.Message;

public class CameraEditor extends DdfEditor {

    public CameraEditor() {
        super(CameraDesc.newBuilder().buildPartial());
    }

    public static Message newInitialWizardContent() {
        return CameraDesc.newBuilder()
            .setAspectRatio(1)
            .setFOV(45.0f)
            .setNearZ(0.1f)
            .setFarZ(1000.0f)
            .build();
    }
}

