package com.dynamo.cr.scene.resource;

import com.dynamo.camera.proto.Camera.CameraDesc;

public class CameraResource extends Resource {

    private CameraDesc cameraDesc;

    public CameraResource(String path, CameraDesc cameraDesc) {
        super(path);
        this.cameraDesc = cameraDesc;
    }

    public CameraDesc getCameraDesc() {
        return cameraDesc;
    }

    public void setCameraDesc(CameraDesc cameraDesc) {
        this.cameraDesc = cameraDesc;
    }
}
