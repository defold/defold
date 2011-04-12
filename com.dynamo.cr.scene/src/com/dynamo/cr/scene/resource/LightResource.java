package com.dynamo.cr.scene.resource;

import com.dynamo.gamesystem.proto.GameSystem.LightDesc;

public class LightResource extends Resource {

    private LightDesc lightDesc;

    public LightResource(String path, LightDesc lightDesc) {
        super(path);
        this.lightDesc = lightDesc;
    }

    public LightDesc getLightDesc() {
        return lightDesc;
    }

    public void setLightDesc(LightDesc lightDesc) {
        this.lightDesc = lightDesc;
    }
}
