package com.dynamo.cr.go.core;

import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;

import com.dynamo.cr.sceneed.core.Node;

public class GameObjectNode extends Node {

    private String path;

    public String getPath() {
        return this.path;
    }

    public void setPath(String path) {
        this.path = path;
    }

    @Override
    public void childAdded(Node child) {
        sortComponents();
    }

    public void sortComponents() {
        sortChildren(new Comparator<Node>() {
            @Override
            public int compare(Node o1, Node o2) {
                String id1 = ((ComponentNode)o1).getId();
                String id2 = ((ComponentNode)o2).getId();
                return id1.compareTo(id2);
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
        if (this.path != null) {
            return this.path;
        } else {
            return super.toString();
        }
    }
}