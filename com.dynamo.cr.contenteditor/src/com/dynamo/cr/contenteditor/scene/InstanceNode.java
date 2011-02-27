package com.dynamo.cr.contenteditor.scene;

import javax.vecmath.Quat4d;
import javax.vecmath.Vector4d;

import com.dynamo.cr.contenteditor.editors.DrawContext;
import com.dynamo.cr.contenteditor.math.MathUtil;
import com.dynamo.cr.contenteditor.math.Transform;
import com.dynamo.gameobject.ddf.GameObject.InstanceDesc;

public class InstanceNode extends Node {

    private String prototype;
    private Node prototypeNode;

    public InstanceNode(String identifier, Scene scene, String prototype, Node prototype_node) {
        super(identifier, scene, FLAG_EDITABLE | FLAG_CAN_HAVE_CHILDREN | FLAG_TRANSFORMABLE);
        this.prototype = prototype;
        this.prototypeNode = prototype_node;
        this.prototypeNode.setParent(this);
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
        // TODO Auto-generated method stub

    }

    public String getPrototype() {
        return prototype;
    }

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
        return (child instanceof InstanceNode) || (child instanceof PrototypeNode) || (child instanceof BrokenNode);
    }
}
