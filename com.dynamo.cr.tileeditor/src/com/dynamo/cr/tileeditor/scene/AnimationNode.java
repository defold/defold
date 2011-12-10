package com.dynamo.cr.tileeditor.scene;

import com.dynamo.cr.properties.NotEmpty;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.validators.Unique;

public class AnimationNode extends Node {

    @Property
    @NotEmpty
    @Unique
    private String id;

    public String getId() {
        return this.id;
    }

    public void setId(String id) {
        this.id = id;
    }

    public TileSetNode getTileSetNode() {
        return (TileSetNode) getParent().getParent();
    }

}
