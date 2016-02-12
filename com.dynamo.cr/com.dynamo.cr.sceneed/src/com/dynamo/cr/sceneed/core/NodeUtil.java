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

    /**
     * Test if the supplied parent is valid for the supplied children.
     * All children must be valid for a match.
     *
     * @param parent Node to test as parent
     * @param children Children to test, all children must be valid for the parent
     * @param presenterContext Context to use for testing validity
     * @return Whether the parent is valid or not
     */
    public static boolean isValidParent(Node parent, List<Node> nodes, ISceneView.IPresenterContext presenterContext) {
        if (parent == null) {
            return false;
        }
        if (nodes.isEmpty()) {
            return true;
        }
        INodeType parentType = presenterContext.getNodeType(parent.getClass());
        if (parentType != null) {
            for (Node node : nodes) {
                boolean nodeAccepted = false;
                for (INodeType nodeType : parentType.getReferenceNodeTypes()) {
                    if (nodeType.getNodeClass().isAssignableFrom(node.getClass())) {
                        nodeAccepted = true;
                        break;
                    }
                }
                if (!nodeAccepted || !node.isValidParent(parent)) {
                    return false;
                }
            }
            return true;
        }
        return false;
    }

    /**
     * Search for ancestor that accepts the given children towards the root of the supplied parent.
     * The search is aborted as soon as a valid ancestor is found.
     * All children must be valid for a match.
     *
     * @param parent Node to start searching from
     * @param children Children to test, all children must be valid for the ancestor
     * @param presenterContext Context to use for testing validity
     * @return Node if an acceptable ancestor could be found, null otherwise
     */
    public static Node findValidAncestor(Node parent, List<Node> children, ISceneView.IPresenterContext presenterContext) {
        while (parent != null) {
            if (isValidParent(parent, children, presenterContext)) {
                return parent;
            }
            parent = parent.getParent();
        }
        return null;
    }

}
