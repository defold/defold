package com.dynamo.cr.tileeditor.scene;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.INodeLoader;
import com.dynamo.tile.proto.Tile;
import com.dynamo.tile.proto.Tile.Animation;
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
        node.setExtrudeBorders(ddf.getExtrudeBorders());
        node.setCollision(ddf.getCollision());
        node.setMaterialTag(ddf.getMaterialTag());
        // Load collision groups
        Map<String, CollisionGroupNode> collisionGroups = new HashMap<String, CollisionGroupNode>();
        for (int i = 0; i < ddf.getCollisionGroupsCount(); ++i) {
            CollisionGroupNode groupNode = new CollisionGroupNode();
            String groupId = ddf.getCollisionGroups(i);
            groupNode.setId(groupId);
            node.addChild(groupNode);
            if (!collisionGroups.containsKey(groupId)) {
                collisionGroups.put(groupId, groupNode);
            }
        }
        // Load tile collision groups
        List<CollisionGroupNode> tileCollisionGroups = new ArrayList<CollisionGroupNode>(ddf.getConvexHullsCount());
        for (ConvexHull hull : ddf.getConvexHullsList()) {
            tileCollisionGroups.add(collisionGroups.get(hull.getCollisionGroup()));
        }
        node.setTileCollisionGroups(tileCollisionGroups);
        // Load animations
        for (int i = 0; i < ddf.getAnimationsCount(); ++i) {
            AnimationNode animNode = new AnimationNode();
            Tile.Animation animDdf = ddf.getAnimations(i);
            animNode.setId(animDdf.getId());
            animNode.setStartTile(animDdf.getStartTile());
            animNode.setEndTile(animDdf.getEndTile());
            animNode.setPlayback(animDdf.getPlayback());
            animNode.setFps(animDdf.getFps());
            animNode.setFlipHorizontally(animDdf.getFlipHorizontal() != 0);
            animNode.setFlipVertically(animDdf.getFlipVertical() != 0);
            node.addChild(animNode);
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
                .setExtrudeBorders(node.getExtrudeBorders())
                .setCollision(node.getCollision())
                .setMaterialTag(node.getMaterialTag());
        // Save tile collision groups
        List<com.dynamo.bob.tile.ConvexHull> hulls = node.getConvexHulls();
        List<CollisionGroupNode> tileCollisionGroups = node.getTileCollisionGroups();
        int count = hulls.size();
        for (int i = 0; i < count; ++i) {
            com.dynamo.bob.tile.ConvexHull convexHull = hulls.get(i);
            Tile.ConvexHull.Builder convexHullBuilder = Tile.ConvexHull.newBuilder();
            convexHullBuilder.setIndex(convexHull.getIndex());
            convexHullBuilder.setCount(convexHull.getCount());
            String collisionGroupId = "";
            CollisionGroupNode collisionGroup = tileCollisionGroups.get(i);
            if (collisionGroup != null) {
                collisionGroupId = collisionGroup.getId();
            }
            convexHullBuilder.setCollisionGroup(collisionGroupId);
            tileSetBuilder.addConvexHulls(convexHullBuilder);
        }
        // Save collision groups
        for (CollisionGroupNode collisionGroup : node.getCollisionGroups()) {
            tileSetBuilder.addCollisionGroups(collisionGroup.getId());
        }
        // Save animation
        for (AnimationNode animNode : node.getAnimations()) {
            Animation.Builder animBuilder = Animation.newBuilder();
            animBuilder.setId(animNode.getId())
                .setStartTile(animNode.getStartTile())
                .setEndTile(animNode.getEndTile())
                .setPlayback(animNode.getPlayback())
                .setFps(animNode.getFps())
                    .setFlipHorizontal(animNode.isFlipHorizontally() ? 1 : 0)
                    .setFlipVertical(animNode.isFlipVertically() ? 1 : 0);
            tileSetBuilder.addAnimations(animBuilder);
        }
        return tileSetBuilder.build();
    }

}
