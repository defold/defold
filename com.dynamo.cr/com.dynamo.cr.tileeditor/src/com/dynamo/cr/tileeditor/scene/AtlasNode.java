package com.dynamo.cr.tileeditor.scene;

import java.awt.image.BufferedImage;
import java.util.ArrayList;
import java.util.List;

import com.dynamo.cr.properties.GreaterEqualThanZero;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.sceneed.core.AABB;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.tileeditor.atlas.AtlasGenerator;
import com.dynamo.cr.tileeditor.atlas.AtlasMap;

@SuppressWarnings("serial")
public class AtlasNode extends Node {
    private AtlasMap atlasMap;

    private int version = 0;
    private int cleanVersion = 0;

    @Property
    @GreaterEqualThanZero
    private int margin;

    private AtlasAnimationNode playBackNode;

    public AtlasNode() {
        AABB aabb = new AABB();
        aabb.union(0, 0, 0);
        aabb.union(512, 512, 0);
        setAABB(aabb);
    }

    public int getMargin() {
        return margin;
    }

    public void setMargin(int margin) {
        this.margin = margin;
        increaseVersion();
    }

    @Override
    protected void childAdded(Node child) {
        updateStatus();
        increaseVersion();
    }

    @Override
    protected void childRemoved(Node child) {
        super.childRemoved(child);
        increaseVersion();
    }

    private static void collectImages(Node node, List<String> images) {
        for (Node n : node.getChildren()) {
            if (n instanceof AtlasImageNode) {
                AtlasImageNode atlasImageNode = (AtlasImageNode) n;
                if (!images.contains(atlasImageNode.getImage())) {
                    images.add(atlasImageNode.getImage());
                }
            } else if (n instanceof AtlasAnimationNode) {
                collectImages(n, images);
            }
        }
    }

    public int getVersion() {
        return version;
    }

    public AtlasMap getAtlasMap() {
        if (version != cleanVersion) {
            List<String> imageNames = new ArrayList<String>(64);
            List<BufferedImage> images = new ArrayList<BufferedImage>(64);
            collectImages(this, imageNames);

            boolean ok = true;
            for (String name : imageNames) {
                BufferedImage image = getModel().getImage(name);
                if (image != null) {
                    images.add(image);
                } else {
                    ok = false;
                    break;
                }
            }

            if (ok) {
                atlasMap = AtlasGenerator.generate(images, imageNames, Math.max(0, margin));
                BufferedImage image = atlasMap.getImage();
                AABB aabb = new AABB();
                aabb.union(0, 0, 0);
                aabb.union(image.getWidth(), image.getHeight(), 0);
                setAABB(aabb);
            } else {
                atlasMap = null;
            }

            cleanVersion = version;
        }
        return atlasMap;
    }

    public void increaseVersion() {
        this.version++;
    }

    public void setPlaybackNode(AtlasAnimationNode node) {
        this.playBackNode = node;
    }

    public AtlasAnimationNode getPlayBackNode() {
        // NOTE: We check parent in order to determine dangling and removed nodes
        if (playBackNode != null && playBackNode.getParent() != null) {
            return playBackNode;
        } else {
            return null;
        }
    }
}
