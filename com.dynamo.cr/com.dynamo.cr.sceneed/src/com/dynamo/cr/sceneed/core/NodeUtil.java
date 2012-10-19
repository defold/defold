package com.dynamo.cr.sceneed.core;

import java.util.ArrayList;
import java.util.List;

public class NodeUtil {

    public static interface IdFetcher<T extends Node> {
        String getId(T node);
    }

    public static <T extends Node> String getUniqueId(List<T> nodes, String baseId, IdFetcher<T> idFetcher) {
        List<String> ids = new ArrayList<String>(nodes.size());
        for (T node: nodes) {
            String nodeId = idFetcher.getId(node);
            if (nodeId != null) {
                ids.add(nodeId);
            }
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

    public static String getUniqueId(Node parent, String baseId, IdFetcher<Node> idFetcher) {
        return getUniqueId(parent.getChildren(), baseId, idFetcher);
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

    /**
     * Returns the node to be selected if the supplied sibling nodes are removed.
     * @param siblings Nodes to be removed, assumed to be siblings
     * @return Node to be selected, or null
     */
    public static Node getSelectionReplacement(List<Node> siblings) {
        if (siblings.isEmpty())
            return null;
        else
            return siblings.get(0).getParent();
    }

    public static Node findAcceptingParent(Node target, List<Node> nodes, ISceneView.IPresenterContext presenterContext) {
        INodeType targetType = null;
        // Verify that the target is not a descendant, in which case use common parent instead
        Node t = target;
        while (t != null) {
            if (nodes.contains(t)) {
                target = t.getParent();
                break;
            }
            t = t.getParent();
        }
        // Verify acceptance of child classes
        while (target != null) {
            boolean accepted = true;
            targetType = presenterContext.getNodeType(target.getClass());
            if (targetType != null) {
                for (Node node : nodes) {
                    boolean nodeAccepted = false;
                    for (INodeType nodeType : targetType.getReferenceNodeTypes()) {
                        if (nodeType.getNodeClass().isAssignableFrom(node.getClass())) {
                            nodeAccepted = true;
                            break;
                        }
                    }
                    if (!nodeAccepted) {
                        accepted = false;
                        break;
                    }
                }
                if (accepted) {
                    break;
                }
            }
            target = target.getParent();
        }
        if (target == null || targetType == null)
            return null;
        return target;
    }
}
