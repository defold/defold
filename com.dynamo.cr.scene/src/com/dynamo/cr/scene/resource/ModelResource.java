package com.dynamo.cr.scene.resource;

import java.util.List;

import com.dynamo.model.proto.Model.ModelDesc;

public class ModelResource extends Resource {

    private ModelDesc desc;
    private MeshResource meshResource;
    private List<TextureResource> textureResources;

    public ModelResource(String path, ModelDesc desc, MeshResource meshResource, List<TextureResource> textureResources) {
        super(path);
        this.desc = desc;
        this.meshResource = meshResource;
        this.textureResources = textureResources;
    }

    public ModelDesc getModelDesc() {
        return this.desc;
    }

    public void setModelDesc(ModelDesc desc) {
        this.desc = desc;
    }

    public MeshResource getMeshResource() {
        return this.meshResource;
    }

    public void setMeshResource(MeshResource meshResource) {
        this.meshResource = meshResource;
    }

    public List<TextureResource> getTextureResources() {
        return this.textureResources;
    }

    public void setTextureResources(List<TextureResource> textureResources) {
        this.textureResources = textureResources;
    }

}
