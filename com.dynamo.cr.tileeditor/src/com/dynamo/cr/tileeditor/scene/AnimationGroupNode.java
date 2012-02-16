package com.dynamo.cr.tileeditor.scene;

import java.util.Comparator;

import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.util.GroupNode;

public class AnimationGroupNode extends GroupNode<AnimationNode> {

    @Override
    public String toString() {
        return Messages.AnimationNode_label;
    }

    @Override
    protected void childAdded(Node child) {
        sortAnimations();
    }

    public void sortAnimations() {
        sortNodes(new Comparator<Node>() {
            @Override
            public int compare(Node o1, Node o2) {
                String id1 = ((AnimationNode)o1).getId();
                String id2 = ((AnimationNode)o2).getId();
                return id1.compareTo(id2);
            }
        });
    }

}
