package com.dynamo.cr.contenteditor.scene;

import javax.vecmath.Quat4d;
import javax.vecmath.Vector4d;

import com.dynamo.cr.contenteditor.editors.DrawContext;
import com.dynamo.cr.contenteditor.math.MathUtil;
import com.dynamo.cr.contenteditor.math.Transform;
import com.dynamo.gameobject.proto.GameObject.CollectionInstanceDesc;

public class CollectionInstanceNode extends Node {

    private String collection;
    private Node collectionNode;

    public CollectionInstanceNode(String identifier, Scene scene, String collection, Node collection_node) {
        super(identifier, scene, FLAG_EDITABLE | FLAG_CAN_HAVE_CHILDREN | FLAG_TRANSFORMABLE);
        this.collection = collection;
        this.collectionNode = collection_node;
        if (this.collectionNode != null) {
            this.collectionNode.setParent(this);
        }
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
    public void draw(DrawContext context) {
    }

    public String getCollection() {
        return collection;
    }

    public CollectionInstanceDesc buildDescriptor() {
        Transform t = new Transform();
        getLocalTransform(t);

        Vector4d translation = new Vector4d();
        Quat4d rotation = new Quat4d();
        t.getTranslation(translation);
        t.getRotation(rotation);

        return CollectionInstanceDesc.newBuilder()
            .setId(getIdentifier())
            .setPosition(MathUtil.toPoint3(translation))
            .setRotation(MathUtil.toQuat(rotation))
            .setCollection(this.collection)
            .build();
    }

    @Override
    protected boolean verifyChild(Node child) {
        return child instanceof CollectionNode;
    }
}
