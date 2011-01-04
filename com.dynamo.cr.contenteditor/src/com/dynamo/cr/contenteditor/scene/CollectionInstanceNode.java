package com.dynamo.cr.contenteditor.scene;

import com.dynamo.cr.contenteditor.editors.DrawContext;

public class CollectionInstanceNode extends Node {

    private String collection;
    private Node collectionNode;

    public CollectionInstanceNode(Scene scene, String id, String collection, Node collection_node) {
        super(scene, FLAG_TRANSFORMABLE | FLAG_SELECTABLE | FLAG_CAN_HAVE_CHILDREN);
        setIdentifier(id);
        this.collection = collection;
        this.collectionNode = collection_node;
        this.collectionNode.setParent(this);
    }

    void andFlags(Node node, int flags) {
        node.setFlags(node.getFlags() & flags);
        for (Node n : node.getChilden()) {
            andFlags(n, flags);
        }
    }

    public void postAddNode(Node node) {
        andFlags(node, ~(Node.FLAG_SELECTABLE | Node.FLAG_TRANSFORMABLE));
    }

    @Override
    public String getName() {
        return getIdentifier();
    }

    @Override
    public void draw(DrawContext context) {
    }

    public String getCollection() {
        return collection;
    }
/*
    public InstanceDesc getDesciptor() {
        Transform t = new Transform();
        getLocalTransform(t);

        Vector4d translation = new Vector4d();
        Quat4d rotation = new Quat4d();
        t.getTranslation(translation);
        t.getRotation(rotation);

        InstanceDesc desc = new InstanceDesc();
        desc.m_Id = getIdentifier();
        desc.m_Position = MathUtil.toPoint3(translation);
        desc.m_Rotation = MathUtil.toQuat(rotation);
        desc.m_Prototype = prototype;

        // TODO: REST HERE!
        return desc;
    }*/

}
