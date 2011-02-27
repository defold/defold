package com.dynamo.cr.contenteditor.scene;


public class ModelNode extends ComponentNode {

    private MeshNode meshNode;

    public ModelNode(Scene scene, String resource, MeshNode mesh_node) {
        super(scene, resource);
        this.meshNode = mesh_node;
        this.meshNode.m_Parent = this;
        addNode(meshNode);
    }

    @Override
    protected boolean verifyChild(Node child) {
        return child instanceof MeshNode;
    }
}
