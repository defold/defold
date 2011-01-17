package com.dynamo.cr.ddfeditor;

import com.dynamo.camera.proto.Camera.CameraDesc;

public class CameraEditor extends DdfEditor {

    public CameraEditor() {
        super(CameraDesc.newBuilder().buildPartial());
    }

    @Override
    public String getTitle() {
        return "Camera";
    }
}

