package com.dynamo.cr.contenteditor.scene;

import javax.vecmath.Quat4d;
import javax.vecmath.Vector4d;

import com.dynamo.cr.contenteditor.editors.DrawContext;
import com.dynamo.cr.contenteditor.math.MathUtil;
import com.dynamo.cr.contenteditor.math.Transform;
import com.dynamo.gameobject.ddf.GameObject.CollectionDesc;
import com.dynamo.gameobject.ddf.GameObject.CollectionInstanceDesc;
import com.dynamo.gameobject.ddf.GameObject.InstanceDesc;

public class CollectionNode extends Node {

    private String name;
    private String resource;

    public CollectionNode(Scene scene, String name, String resource) {
        super(scene, FLAG_CAN_HAVE_CHILDREN);
        this.name = name;
        this.resource = resource;
    }

    @Override
    public String getName() {
        return name;
    }

    @Override
    public void preAddNode(Node node) {
        assert (node instanceof InstanceNode);
    }

    @Override
    public void draw(DrawContext context) {
    }

    public String getResource() {
        return resource;
    }

    void doGetDescriptor(CollectionDesc desc, InstanceNode in) {
        InstanceDesc id = in.getDesciptor();
        for (Node n2 : in.getChilden()) {
            if (n2 instanceof InstanceNode) {
                InstanceNode in2 = (InstanceNode) n2;
                id.m_Children.add(in2.getName());
                doGetDescriptor(desc, in2);
            }
        }
        desc.m_Instances.add(id);
    }

    public CollectionDesc getDescriptor() {
        CollectionDesc desc = new CollectionDesc();
        desc.m_Name = this.name;

        for (Node n : getChilden()) {

            if (n instanceof CollectionInstanceNode) {
                CollectionInstanceNode cin = (CollectionInstanceNode) n;

                Transform t = new Transform();
                cin.getLocalTransform(t);

                Vector4d translation = new Vector4d();
                Quat4d rotation = new Quat4d();
                t.getTranslation(translation);
                t.getRotation(rotation);

                CollectionInstanceDesc cid = new CollectionInstanceDesc();
                cid.m_Collection = cin.getCollection();
                cid.m_Id = cin.getIdentifier();
                cid.m_Position = MathUtil.toPoint3(translation);
                cid.m_Rotation = MathUtil.toQuat(rotation);

                desc.m_CollectionInstances.add(cid);
            }
            else if (n instanceof InstanceNode) {
                InstanceNode in = (InstanceNode) n;
                doGetDescriptor(desc, in);
            }
        }
        return desc;
    }

}

