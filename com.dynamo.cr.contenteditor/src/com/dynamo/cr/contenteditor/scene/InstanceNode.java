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

    public InstanceNode(Scene scene, String id, String prototype, Node prototype_node) {
        super(scene, FLAG_TRANSFORMABLE | FLAG_SELECTABLE | FLAG_CAN_HAVE_CHILDREN);
        setIdentifier(id);
        this.prototype = prototype;
        this.prototypeNode = prototype_node;
        this.prototypeNode.setParent(this);
    }

    @Override
    public String getName() {
        return getIdentifier();
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

}
