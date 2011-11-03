package com.dynamo.cr.goprot.sprite;

import com.dynamo.cr.goprot.gameobject.ComponentNode;

public class SpriteNode extends ComponentNode {

    public void addAnimation(AnimationNode animation) {
        addChild(animation);
    }

    public void removeAnimation(AnimationNode animation) {
        removeChild(animation);
    }
}
