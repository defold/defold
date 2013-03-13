package com.dynamo.cr.go.core;

import java.util.Comparator;

import com.dynamo.cr.sceneed.core.Node;


@SuppressWarnings("serial")
public class GameObjectPropertyNode extends PropertyNode<GameObjectInstanceNode> {

    public GameObjectPropertyNode(GameObjectInstanceNode node) {
        super(node);
    }

    public String getId() {
        return getNode().getId();
    }

    @Override
    protected void sortChildren() {
        sortChildren(new Comparator<Node>() {
            @Override
            public int compare(Node o1, Node o2) {
                if (o1 instanceof GameObjectPropertyNode && o2 instanceof GameObjectPropertyNode) {
                    String id1 = ((GameObjectPropertyNode)o1).getId();
                    String id2 = ((GameObjectPropertyNode)o2).getId();
                    return id1.compareTo(id2);
                } else if (o1 instanceof ComponentPropertyNode && o2 instanceof ComponentPropertyNode) {
                    String id1 = ((ComponentPropertyNode)o1).getId();
                    String id2 = ((ComponentPropertyNode)o2).getId();
                    return id1.compareTo(id2);
                } else {
                    if (o1 instanceof ComponentPropertyNode) {
                        return 1;
                    } else {
                        return -1;
                    }
                }
            }
        });
    }
}
