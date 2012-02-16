package com.dynamo.cr.sceneed.core.util;

import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;

import org.eclipse.swt.graphics.Image;

import com.dynamo.cr.sceneed.Activator;
import com.dynamo.cr.sceneed.core.Node;

public class GroupNode<T extends Node> extends Node {

    @SuppressWarnings("unchecked")
    public List<T> getNodes() {
        List<Node> children = getChildren();
        int n = children.size();
        List<T> nodes = new ArrayList<T>(n);
        for (Node child : children) {
            nodes.add((T)child);
        }
        return nodes;
    }

    public void sortNodes(Comparator<? super Node> comparator) {
        super.sortChildren(comparator);
    }

    @Override
    public Image getIcon() {
        return Activator.getDefault().getImageRegistry().get(Activator.IMG_FOLDER);
    }
}
