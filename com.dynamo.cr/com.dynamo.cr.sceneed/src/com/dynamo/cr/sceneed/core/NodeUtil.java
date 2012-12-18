package com.dynamo.cr.sceneed.core;

import java.util.List;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class NodeUtil {

    private static final Pattern ID_PATTERN = Pattern.compile("^(.*?)(\\d+)$");

    public static String getUniqueId(Set<String> ids, String baseId) {
        String format = "%s%d";
        Matcher matcher = ID_PATTERN.matcher(baseId);
        int i = 1;
        String base = baseId;
        if (matcher.matches()) {
            base = matcher.group(1);
            i = Integer.parseInt(matcher.group(2)) + 1;
        }
        String id = baseId;
        while (ids.contains(id)) {
            id = String.format(format, base, i);
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

    public static Node findPasteTarget(Node target, List<Node> nodes, ISceneView.IPresenterContext presenterContext) {
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

    public static Node findDropTarget(Node target, List<Node> nodes, ISceneView.IPresenterContext presenterContext) {
        INodeType targetType = null;
        // Verify that the target is not a descendant, would cause cycles
        Node t = target;
        while (t != null) {
            if (nodes.contains(t)) {
                return null;
            }
            t = t.getParent();
        }
        // Verify acceptance of child classes
        if (target != null) {
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
                        return null;
                    }
                }
            }
        }
        if (target == null || targetType == null)
            return null;
        return target;
    }
}
