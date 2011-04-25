package com.dynamo.cr.scene.graph;

import javax.vecmath.Quat4d;
import javax.vecmath.Vector4d;

import com.dynamo.cr.scene.math.MathUtil;
import com.dynamo.cr.scene.math.Transform;
import com.dynamo.gameobject.proto.GameObject.InstanceDesc;

public class InstanceNode extends Node {

    private String prototype;
    private Node prototypeNode;

    public InstanceNode(String identifier, Scene scene, String prototype, Node prototype_node) {
        super(identifier, scene, FLAG_EDITABLE | FLAG_CAN_HAVE_CHILDREN | FLAG_TRANSFORMABLE);
        this.prototype = prototype;
        this.prototypeNode = prototype_node;
        if (this.prototypeNode != null) {
            this.prototypeNode.setParent(this);
        }
    }

    void andFlags(Node node, int flags) {
        if (!(node instanceof InstanceNode))
            node.setFlags(node.getFlags() & flags);
        for (Node n : node.getChildren()) {
            andFlags(n, flags);
        }
    }

    @Override
    public void nodeAdded(Node node) {
        andFlags(node, ~(Node.FLAG_EDITABLE | Node.FLAG_CAN_HAVE_CHILDREN | Node.FLAG_TRANSFORMABLE));
        // tell collection node
        Node parent = getParent();
        while (parent != null && !(parent instanceof CollectionNode)) {
            parent = parent.getParent();
        }
        if (parent != null) {
            parent.nodeAdded(node);
        }
    }

    @Override
    protected void nodeRemoved(Node node) {
        // tell collection node
        Node parent = getParent();
        while (parent != null && !(parent instanceof CollectionNode)) {
            parent = parent.getParent();
        }
        if (parent != null) {
            parent.nodeRemoved(node);
        }
    }
    @Override
    public void draw(DrawContext context) {
    }

    public String getPrototype() {
        return prototype;
    }

    public InstanceDesc buildDesciptor() {
        Transform t = new Transform();
        getLocalTransform(t);

        Vector4d translation = new Vector4d();
        Quat4d rotation = new Quat4d();
        t.getTranslation(translation);
        t.getRotation(rotation);

        InstanceDesc.Builder b = InstanceDesc.newBuilder()
            .setId(getIdentifier())
            .setPosition(MathUtil.toPoint3(translation))
            .setRotation(MathUtil.toQuat(rotation))
            .setPrototype(this.prototype);
        for (Node child : getChildren()) {
            if (child instanceof InstanceNode) {
                b.addChildren(child.getIdentifier());
            }
        }
        return b.build();
    }

    @Override
    protected void childIdentifierChanged(Node node, String oldId) {
        m_Parent.childIdentifierChanged(node, oldId);
    }

    @Override
    public boolean isChildIdentifierUsed(Node node, String id) {
        return m_Parent.isChildIdentifierUsed(node, id);
    }

    @Override
    public String getUniqueChildIdentifier(Node child) {
        return m_Parent.getUniqueChildIdentifier(child);
    }

    @Override
    protected boolean verifyChild(Node child) {
        return (child instanceof InstanceNode) || (child instanceof PrototypeNode);
    }
}
