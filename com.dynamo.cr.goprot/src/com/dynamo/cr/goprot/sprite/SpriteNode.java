package com.dynamo.cr.goprot.sprite;

import com.dynamo.cr.goprot.core.Messages;
import com.dynamo.cr.goprot.core.NodeModel;
import com.dynamo.cr.goprot.core.Resource;
import com.dynamo.cr.goprot.gameobject.ComponentNode;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.PropertyIntrospector;
import com.dynamo.cr.properties.PropertyIntrospectorModel;

public class SpriteNode extends ComponentNode {

    @Property(isResource = true)
    @Resource
    private String tileSet = "";

    public String getTileSet() {
        return this.tileSet;
    }

    public void setTileSet(String tileSet) {
        this.tileSet = tileSet;
    }

    public void addAnimation(AnimationNode animation) {
        addChild(animation);
    }

    public void removeAnimation(AnimationNode animation) {
        removeChild(animation);
    }

    private static PropertyIntrospector<SpriteNode, NodeModel> introspector = new PropertyIntrospector<SpriteNode, NodeModel>(SpriteNode.class, Messages.class);

    @Override
    public Object getAdapter(@SuppressWarnings("rawtypes") Class adapter) {
        if (adapter == IPropertyModel.class) {
            return new PropertyIntrospectorModel<SpriteNode, NodeModel>(this, getModel(), introspector);
        }
        return null;
    }
}
