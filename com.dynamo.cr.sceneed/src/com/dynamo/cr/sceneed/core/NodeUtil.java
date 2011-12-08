package com.dynamo.cr.sceneed.core;

import java.util.ArrayList;
import java.util.List;

public class NodeUtil {

    public static interface IdFetcher {
        String getId(Node child);
    }

    public static String getUniqueId(Node parent, String baseId, IdFetcher idFetcher) {
        List<Node> children = parent.getChildren();
        List<String> ids = new ArrayList<String>(children.size());
        for (Node child : children) {
            ids.add(idFetcher.getId(child));
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

    /**
     * Returns the node to be selected if the supplied node is removed.
     * @param node Node to be removed
     * @return Node to be selected, or null
     */
    public static Node getSelectionReplacement(Node node) {
        Node parent = node.getParent();
        if (parent != null) {
            List<Node> children = parent.getChildren();
            int index = children.indexOf(node);
            if (index + 1 < children.size()) {
                ++index;
            } else {
                --index;
            }
            Node selected = null;
            if (index >= 0) {
                selected = children.get(index);
            } else {
                selected = parent;
            }
            return selected;
        }
        return null;
    }
}
