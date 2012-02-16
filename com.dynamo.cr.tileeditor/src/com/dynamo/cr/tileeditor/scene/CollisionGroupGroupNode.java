package com.dynamo.cr.tileeditor.scene;

import java.util.Comparator;

import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.util.GroupNode;

public class CollisionGroupGroupNode extends GroupNode<CollisionGroupNode> {
    @Override
    public String toString() {
        return Messages.CollisionGroupGroupNode_label;
    }

    @Override
    protected void childAdded(Node child) {
        sortCollisionGroups();
    }

    private void sortCollisionGroups() {
        sortNodes(new Comparator<Node>() {
            @Override
            public int compare(Node o1, Node o2) {
                String id1 = ((CollisionGroupNode)o1).getId();
                String id2 = ((CollisionGroupNode)o2).getId();
                return id1.compareTo(id2);
            }
        });
    }

}
