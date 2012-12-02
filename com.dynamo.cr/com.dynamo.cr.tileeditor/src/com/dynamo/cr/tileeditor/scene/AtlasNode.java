package com.dynamo.cr.tileeditor.scene;

import java.awt.image.BufferedImage;
import java.util.ArrayList;
import java.util.List;

import com.dynamo.cr.properties.Property;
import com.dynamo.cr.sceneed.core.AABB;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.tileeditor.atlas.AtlasGenerator;
import com.dynamo.cr.tileeditor.atlas.AtlasMap;

@SuppressWarnings("serial")
public class AtlasNode extends Node {
    private boolean atlasDirty = true;
    private AtlasMap atlasMap;

    private int version = 0;

    @Property
    private int margin;

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
        atlasDirty = true;
        ++version;
    }

    @Override
    protected void childAdded(Node child) {
        updateStatus();
        atlasDirty = true;
        ++version;
    }

    @Override
    protected void childRemoved(Node child) {
        super.childRemoved(child);
        atlasDirty = true;
        ++version;
    }

    private static void collectImages(Node node, List<String> images) {
        for (Node n : node.getChildren()) {
            if (n instanceof AtlasImageNode) {
                AtlasImageNode atlasImageNode = (AtlasImageNode) n;
                images.add(atlasImageNode.getImage());
            } else if (n instanceof AtlasAnimationNode) {
                collectImages(n, images);
            }
        }
    }

    public int getVersion() {
        return version;
    }

    public AtlasMap getAtlasMap() {
        if (atlasDirty) {
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
                atlasMap = AtlasGenerator.generate(images, imageNames, margin);
                BufferedImage image = atlasMap.getImage();
                AABB aabb = new AABB();
                aabb.union(0, 0, 0);
                aabb.union(image.getWidth(), image.getHeight(), 0);
                setAABB(aabb);
            } else {
                atlasMap = null;
            }

            atlasDirty = false;
        }
        return atlasMap;
    }
}
