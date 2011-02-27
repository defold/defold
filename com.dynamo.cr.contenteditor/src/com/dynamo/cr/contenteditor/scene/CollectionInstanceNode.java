package com.dynamo.cr.contenteditor.scene;

import javax.vecmath.Quat4d;
import javax.vecmath.Vector4d;

import com.dynamo.cr.contenteditor.editors.DrawContext;
import com.dynamo.cr.contenteditor.math.MathUtil;
import com.dynamo.cr.contenteditor.math.Transform;
import com.dynamo.gameobject.ddf.GameObject.CollectionInstanceDesc;

public class CollectionInstanceNode extends Node {

    private String collection;
    private Node collectionNode;

    public CollectionInstanceNode(Scene scene, String id, String collection, Node collection_node) {
        super(scene, FLAG_EDITABLE | FLAG_CAN_HAVE_CHILDREN | FLAG_TRANSFORMABLE);
        setIdentifier(id);
        this.collection = collection;
        this.collectionNode = collection_node;
        this.collectionNode.setParent(this);
    }

    void andFlags(Node node, int flags) {
        node.setFlags(node.getFlags() & flags);
        for (Node n : node.getChildren()) {
            andFlags(n, flags);
        }
    }

    public void nodeAdded(Node node) {
        andFlags(node, ~(Node.FLAG_EDITABLE | Node.FLAG_CAN_HAVE_CHILDREN | Node.FLAG_TRANSFORMABLE));
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

    public CollectionInstanceDesc getDesciptor() {
        Transform t = new Transform();
        getLocalTransform(t);

        Vector4d translation = new Vector4d();
        Quat4d rotation = new Quat4d();
        t.getTranslation(translation);
        t.getRotation(rotation);

        CollectionInstanceDesc desc = new CollectionInstanceDesc();
        desc.m_Id = getIdentifier();
        desc.m_Position = MathUtil.toPoint3(translation);
        desc.m_Rotation = MathUtil.toQuat(rotation);
        desc.m_Collection = collection;

        // TODO: REST HERE!
        return desc;
    }

    @Override
    protected boolean verifyChild(Node child) {
        return child instanceof CollectionNode;
    }
}
