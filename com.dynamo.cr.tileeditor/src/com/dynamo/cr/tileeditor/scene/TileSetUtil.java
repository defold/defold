package com.dynamo.cr.tileeditor.scene;

import java.util.ArrayList;
import java.util.List;

import org.eclipse.jface.viewers.IStructuredSelection;

import com.dynamo.cr.sceneed.core.Node;

public class TileSetUtil {
    public static CollisionGroupNode getCurrentCollisionGroup(IStructuredSelection selection) {
        Object selected = selection.getFirstElement();
        if (selected instanceof CollisionGroupNode) {
            return (CollisionGroupNode) selected;
        }
        return null;
    }

    public static TileSetNode getCurrentTileSet(IStructuredSelection selection) {
        Object selected = selection.getFirstElement();
        if (selected instanceof Node) {
            Node node = (Node)selected;
            if (node instanceof TileSetNode) {
                return (TileSetNode) node;
            } else if (node instanceof CollisionGroupNode) {
                return ((CollisionGroupNode)node).getTileSetNode();
            } else if (node instanceof CollisionGroupGroupNode) {
                return (TileSetNode)((CollisionGroupGroupNode)node).getParent();
            } else if (node instanceof AnimationNode) {
                return ((AnimationNode)node).getTileSetNode();
            } else if (node instanceof AnimationGroupNode) {
                return (TileSetNode)((AnimationGroupNode)node).getParent();
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
}
