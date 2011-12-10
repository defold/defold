package com.dynamo.cr.tileeditor.scene;

import java.util.ArrayList;
import java.util.List;

import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.Node;

public class TileSetUtil {
    public static CollisionGroupNode getCurrentCollisionGroup(IPresenterContext presenterContext) {
        return (CollisionGroupNode) presenterContext.getSelection().getFirstElement();
    }

    public static TileSetNode getCurrentTileSet(IPresenterContext presenterContext) {
        Node node = (Node) presenterContext.getSelection().getFirstElement();
        if (node instanceof TileSetNode) {
            return (TileSetNode) node;
        } else if (node instanceof CollisionGroupNode) {
            return (TileSetNode) node.getParent();
        }
        return null;
    }

    public static List<String> getCurrentCollisionGroupNames(IPresenterContext presenterContext) {
        TileSetNode tileSet = getCurrentTileSet(presenterContext);
        List<Node> children = tileSet.getChildren();
        List<String> collisionGroups = new ArrayList<String>(children.size()+1);
        collisionGroups.add("(none)");
        for (Node child : children) {
            collisionGroups.add(((CollisionGroupNode)child).getName());
        }
        return collisionGroups;
    }
}
