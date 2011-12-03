package com.dynamo.cr.tileeditor.scene;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.List;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.sceneed.core.ISceneView.ILoaderContext;
import com.dynamo.cr.sceneed.core.ISceneView.INodeLoader;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.tile.proto.Tile;
import com.dynamo.tile.proto.Tile.ConvexHull;
import com.dynamo.tile.proto.Tile.TileSet;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;

public class TileSetLoader implements INodeLoader<TileSetNode> {

    @Override
    public TileSetNode load(ILoaderContext context, InputStream contents)
            throws IOException, CoreException {
        // Build message
        TileSet.Builder tileSetBuilder = TileSet.newBuilder();
        TextFormat.merge(new InputStreamReader(contents), tileSetBuilder);
        TileSet ddf = tileSetBuilder.build();
        TileSetNode node = new TileSetNode();
        // Set properties
        node.setImage(ddf.getImage());
        node.setTileWidth(ddf.getTileWidth());
        node.setTileHeight(ddf.getTileHeight());
        node.setTileMargin(ddf.getTileMargin());
        node.setTileSpacing(ddf.getTileSpacing());
        node.setCollision(ddf.getCollision());
        node.setMaterialTag(ddf.getMaterialTag());
        // Load tile collision groups
        List<String> tileCollisionGroups = new ArrayList<String>(ddf.getConvexHullsCount());
        for (ConvexHull hull : ddf.getConvexHullsList()) {
            tileCollisionGroups.add(hull.getCollisionGroup());
        }
        node.setTileCollisionGroups(tileCollisionGroups);
        // Load collision groups
        for (int i = 0; i < ddf.getCollisionGroupsCount(); ++i) {
            CollisionGroupNode groupNode = new CollisionGroupNode();
            groupNode.setName(ddf.getCollisionGroups(i));
            node.addCollisionGroup(groupNode);
        }

        return node;
    }

    @Override
    public Message buildMessage(ILoaderContext context, TileSetNode node,
            IProgressMonitor monitor) throws IOException, CoreException {
        TileSet.Builder tileSetBuilder = TileSet.newBuilder()
                .setImage(node.getImage())
                .setTileWidth(node.getTileWidth())
                .setTileHeight(node.getTileHeight())
                .setTileMargin(node.getTileMargin())
                .setTileSpacing(node.getTileSpacing())
                .setCollision(node.getCollision())
                .setMaterialTag(node.getMaterialTag());
        // Save tile collision groups
        List<com.dynamo.tile.ConvexHull> hulls = node.getConvexHulls();
        List<String> tileCollisionGroups = node.getTileCollisionGroups();
        int count = hulls.size();
        for (int i = 0; i < count; ++i) {
            com.dynamo.tile.ConvexHull convexHull = hulls.get(i);
            Tile.ConvexHull.Builder convexHullBuilder = Tile.ConvexHull.newBuilder();
            convexHullBuilder.setIndex(convexHull.getIndex());
            convexHullBuilder.setCount(convexHull.getCount());
            String collisionGroup = tileCollisionGroups.get(i);
            convexHullBuilder.setCollisionGroup(collisionGroup);
            tileSetBuilder.addConvexHulls(convexHullBuilder);
        }
        // Save collision groups
        for (Node child : node.getChildren()) {
            CollisionGroupNode collisionGroup = (CollisionGroupNode)child;
            tileSetBuilder.addCollisionGroups(collisionGroup.getName());
        }
        return tileSetBuilder.build();
    }

}
