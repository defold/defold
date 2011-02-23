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
        return resource;
    }

    @Override
    public void preAddNode(Node node) {
        if (node instanceof CollectionInstanceNode) {
            if (getScene().getCollectionInstanceNodeFromId(node.getIdentifier()) != null) {
                node.setIdentifier(getScene().getUniqueCollectionInstanceId(node.getIdentifier()));
            }
        }
        else if (node instanceof InstanceNode) {
            if (getScene().getInstanceNodeFromId(node.getIdentifier()) != null) {
                node.setIdentifier(getScene().getUniqueInstanceId(node.getIdentifier()));
            }
        }
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
                CollectionInstanceDesc cid = cin.getDesciptor();
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

