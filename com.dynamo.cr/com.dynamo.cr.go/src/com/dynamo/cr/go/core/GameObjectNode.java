package com.dynamo.cr.go.core;

import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;

import com.dynamo.cr.sceneed.core.Node;

@SuppressWarnings("serial")
public class GameObjectNode extends GameObjectInstanceNode {

    public boolean isIdVisible() {
        return getParent() != null;
    }

    public boolean isTranslationVisible() {
        return getParent() != null && super.isTranslationVisible();
    }

    public boolean isEulerVisible() {
        return getParent() != null && super.isEulerVisible();
    }

    public boolean isScaleVisible() {
        return getParent() != null && super.isScaleVisible();
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