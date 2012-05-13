package com.dynamo.cr.tileeditor.scene;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import org.eclipse.jface.viewers.IStructuredSelection;

import com.dynamo.cr.sceneed.core.Node;

public class TileSetUtil {
    public static List<Node> getCurrentCollisionGroups(IStructuredSelection selection) {
        Object[] selected = selection.toArray();
        List<Node> result = new ArrayList<Node>(selected.length);
        for (Object obj : selected) {
            if (!(obj instanceof CollisionGroupNode)) {
                return Collections.emptyList();
            }
            result.add((Node)obj);
        }
        return result;
    }

    public static TileSetNode getCurrentTileSet(IStructuredSelection selection) {
        Object selected = selection.getFirstElement();
        if (selected instanceof Node) {
            Node node = (Node)selected;
            if (node instanceof TileSetNode) {
                return (TileSetNode) node;
            } else if (node instanceof CollisionGroupNode) {
                return ((CollisionGroupNode)node).getTileSetNode();
            } else if (node instanceof AnimationNode) {
                return ((AnimationNode)node).getTileSetNode();
            }
        }
        return null;
    }

    public static List<String> getCurrentCollisionGroupIds(IStructuredSelection selection, boolean includeNone) {
        TileSetNode tileSet = getCurrentTileSet(selection);
        List<CollisionGroupNode> collisionGroups = tileSet.getCollisionGroups();
        List<String> collisionGroupIds = new ArrayList<String>(collisionGroups.size()+1);
        if (includeNone) {
            collisionGroupIds.add("(none)");
        }
        for (CollisionGroupNode collisionGroup : collisionGroups) {
            collisionGroupIds.add(collisionGroup.getId());
        }
        return collisionGroupIds;
    }

    public static List<Node> getCurrentAnimations(IStructuredSelection selection) {
        Object[] selected = selection.toArray();
        List<Node> result = new ArrayList<Node>(selected.length);
        for (Object obj : selected) {
            if (!(obj instanceof AnimationNode)) {
                return Collections.emptyList();
            }
            result.add((Node)obj);
        }
        return result;
    }

}
