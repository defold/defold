package com.dynamo.cr.ddfeditor.scene;
import com.dynamo.cr.go.core.ComponentTypeNode;
import com.dynamo.cr.properties.Property;

@SuppressWarnings("serial")
public class CameraNode extends ComponentTypeNode {

    public CameraNode() {
        super();
    }

    @Property
    private float aspectRatio = 1.0f;

    @Property
    private float fov = 30;

    @Property
    private float nearZ = 1.0f;

    @Property
    private float farZ = 2000.0f;

    @Property
    private int autoAspectRatio = 0;


    public float getAspectRatio() {
        return this.aspectRatio;
    }

    public void setAspectRatio(float value) {
        this.aspectRatio = value;
    }

    public float getFov() {
        return this.fov;
    }

    public void setFov(float value) {
        this.fov = value;
    }

    public float getNearZ() {
        return this.nearZ;
    }

    public void setNearZ(float value) {
        this.nearZ = value;
    }

    public float getFarZ() {
        return this.farZ;
    }

    public void setFarZ(float value) {
        this.farZ = value;
    }

    public int getAutoAspectRatio() {
        return this.autoAspectRatio;
    }

    public void setautoAspectRatio(int value) {
        this.autoAspectRatio = value;
    }

}
