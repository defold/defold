package com.dynamo.cr.go.core;

import java.util.Comparator;

import com.dynamo.cr.sceneed.core.Node;


@SuppressWarnings("serial")
public class CollectionPropertyNode extends PropertyNode<CollectionInstanceNode> {

    public CollectionPropertyNode(CollectionInstanceNode node) {
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
                if (o1 instanceof CollectionPropertyNode && o2 instanceof CollectionPropertyNode) {
                    String id1 = ((CollectionPropertyNode)o1).getId();
                    String id2 = ((CollectionPropertyNode)o2).getId();
                    return id1.compareTo(id2);
                } else if (o1 instanceof GameObjectPropertyNode && o2 instanceof GameObjectPropertyNode) {
                    String id1 = ((GameObjectPropertyNode)o1).getId();
                    String id2 = ((GameObjectPropertyNode)o2).getId();
                    return id1.compareTo(id2);
                } else {
                    if (o1 instanceof GameObjectPropertyNode) {
                        return 1;
                    } else {
                        return -1;
                    }
                }
            }
        });
    }
}
