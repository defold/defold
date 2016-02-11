package com.dynamo.cr.go.core;

import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;

import com.dynamo.cr.sceneed.core.Node;

@SuppressWarnings("serial")
public class GameObjectNode extends GameObjectInstanceNode {

    public GameObjectNode() {
        super();
        setTransformable(false);
        clearFlags(Flags.SCALABLE);
    }

    @Override
    public void parentSet() {
        super.parentSet();
        if (getParent() != null) {
            setTransformable(true);
            setFlags(Flags.SCALABLE);
        } else {
            setTransformable(false);
            clearFlags(Flags.SCALABLE);
        }
    }

    public boolean isIdVisible() {
        return getParent() != null;
    }

    @Override
    public void sortChildren() {
        sortChildren(new Comparator<Node>() {
            @Override
            public int compare(Node o1, Node o2) {
                if (o1 instanceof GameObjectInstanceNode && o2 instanceof GameObjectInstanceNode) {
                    String id1 = ((GameObjectInstanceNode)o1).getId();
                    String id2 = ((GameObjectInstanceNode)o2).getId();
                    return id1.compareTo(id2);
                } else if (o1 instanceof ComponentNode && o2 instanceof ComponentNode) {
                    String id1 = ((ComponentNode)o1).getId();
                    String id2 = ((ComponentNode)o2).getId();
                    return id1.compareTo(id2);
                } else {
                    if (o1 instanceof ComponentNode) {
                        return 1;
                    } else {
                        return -1;
                    }
                }
            }
        });
    }

    @Override
    protected boolean isValidParent(Node node) {
        while(node != null) {
            if(node instanceof CollectionNode) {
                return true;
            }
            node = node.getParent();
        }
        return false;
    }

    public String getUniqueId(String baseId) {
        List<Node> children = getChildren();
        List<String> ids = new ArrayList<String>(children.size());
        for (Node child : children) {
            ids.add(((ComponentNode)child).getId());
        }
        String id = baseId;
        String format = "%s%d";
        int i = 1;
        while (ids.contains(id)) {
            id = String.format(format, baseId, i);
            ++i;
        }
        return id;
    }

    @Override
    public String toString() {
        if (getParent() == null && getModel() != null) {
            return getModel().getTypeName(getClass());
        } else {
            return super.toString();
        }
    }
}