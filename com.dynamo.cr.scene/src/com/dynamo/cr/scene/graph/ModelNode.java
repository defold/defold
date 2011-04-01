package com.dynamo.cr.scene.graph;


public class ModelNode extends ComponentNode {

    private MeshNode meshNode;

    public ModelNode(String resource, Scene scene, MeshNode mesh_node) {
        super(resource, scene);
        this.meshNode = mesh_node;
        this.meshNode.m_Parent = this;
        addNode(meshNode);
    }

    @Override
    protected boolean verifyChild(Node child) {
        return child instanceof MeshNode;
    }
}
