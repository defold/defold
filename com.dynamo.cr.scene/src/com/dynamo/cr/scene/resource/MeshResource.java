package com.dynamo.cr.scene.resource;

import com.dynamo.cr.scene.util.Mesh;

public class MeshResource extends Resource {

    private Mesh mesh;

    public MeshResource(String path, Mesh mesh) {
        super(path);
        this.mesh = mesh;
    }

    public Mesh getMesh() {
        return this.mesh;
    }

    public void setMesh(Mesh mesh) {
        this.mesh = mesh;
    }

}
