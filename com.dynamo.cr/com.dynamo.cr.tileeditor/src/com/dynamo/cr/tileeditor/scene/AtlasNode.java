package com.dynamo.cr.tileeditor.scene;

import java.awt.image.BufferedImage;
import java.nio.FloatBuffer;
import java.util.ArrayList;
import java.util.List;

import com.dynamo.cr.properties.GreaterEqualThanZero;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.sceneed.core.AABB;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.TextureHandle;
import com.dynamo.cr.sceneed.ui.util.VertexBufferObject;
import com.dynamo.cr.tileeditor.atlas.AtlasGenerator;
import com.dynamo.cr.tileeditor.atlas.AtlasMap;
import com.dynamo.textureset.proto.TextureSetProto.TextureSetAnimation;

@SuppressWarnings("serial")
public class AtlasNode extends TextureSetNode {
    private AtlasMap atlasMap;

    private int version = 0;
    private int cleanVersion = 0;

    @Property
    @GreaterEqualThanZero
    private int margin;

    private transient AtlasAnimationNode playBackNode;

    private transient TextureHandle textureHandle = new TextureHandle();

    private transient VertexBufferObject vertexBuffer = new VertexBufferObject();

    private transient VertexBufferObject outlineVertexBuffer = new VertexBufferObject();

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

    private static void collectImages(Node node, List<String> images, List<String> ids) {
        for (Node n : node.getChildren()) {
            if (n instanceof AtlasImageNode) {
                AtlasImageNode atlasImageNode = (AtlasImageNode) n;
                if (!images.contains(atlasImageNode.getImage())) {
                    images.add(atlasImageNode.getImage());
                    ids.add(atlasImageNode.getId());
                }
            } else if (n instanceof AtlasAnimationNode) {
                collectImages(n, images, ids);
            }
        }
    }

    private static void collectAnimations(Node node, List<AtlasGenerator.AnimDesc> animations) {
        for (Node n : node.getChildren()) {
            if (n instanceof AtlasAnimationNode) {
                AtlasAnimationNode animNode = (AtlasAnimationNode) n;
                List<Node> children = n.getChildren();
                List<String> ids = new ArrayList<String>(children.size());
                for (Node n2 : children) {
                    AtlasImageNode imageNode = (AtlasImageNode) n2;
                    ids.add(imageNode.getId());
                }
                animations.add(new AtlasGenerator.AnimDesc(animNode.getId(), ids, animNode.getPlayback(), animNode.getFps(), animNode.isFlipHorizontally(), animNode.isFlipVertically()));
            }
        }
    }

    public int getVersion() {
        return version;
    }

    public AtlasMap getAtlasMap() {
        if (version != cleanVersion) {
            List<String> imageNames = new ArrayList<String>(64);
            List<String> ids = new ArrayList<String>(64);
            List<BufferedImage> images = new ArrayList<BufferedImage>(64);
            List<AtlasGenerator.AnimDesc> animations = new ArrayList<AtlasGenerator.AnimDesc>(32);
            collectImages(this, imageNames, ids);
            collectAnimations(this, animations);

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
                atlasMap = AtlasGenerator.generate(images, ids, animations, Math.max(0, margin));
                this.vertexBuffer.bufferData(atlasMap.getVertexBuffer(), atlasMap.getVertexBuffer().capacity() * 4);
                this.outlineVertexBuffer.bufferData(atlasMap.getOutlineVertexBuffer(), atlasMap.getOutlineVertexBuffer().capacity() * 4);
                BufferedImage image = atlasMap.getImage();
                AABB aabb = new AABB();
                aabb.union(0, 0, 0);
                aabb.union(image.getWidth(), image.getHeight(), 0);
                this.textureHandle.setImage(image);
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

    @Override
    public TextureHandle getTextureHandle() {
        return textureHandle;
    }

    private TextureSetAnimation getTile(String id) {
        AtlasMap am = getAtlasMap();
        if (am != null) {
            List<TextureSetAnimation> tiles = am.getAnimations();
            for (TextureSetAnimation tile : tiles) {
                if (tile.getId().equals(id)) {
                    return tile;
                }
            }
        }
        return null;
    }

    @Override
    public VertexBufferObject getVertexBuffer() {
        return vertexBuffer;
    }

    @Override
    public VertexBufferObject getOutlineVertexBuffer() {
        return outlineVertexBuffer;
    }

    @Override
    public AABB getTextureBounds(String id) {
        AABB aabb = new AABB();
        TextureSetAnimation tile = getTile(id);
        if (tile != null) {
            float w2 = tile.getWidth() * 0.5f;
            float h2 = tile.getHeight() * 0.5f;
            aabb.union(-w2, -h2, 0);
            aabb.union(w2, h2, 0);
        }

        return aabb;
    }

    @Override
    public TextureSetAnimation getAnimation(String id) {
        TextureSetAnimation tile = getTile(id);
        if (tile != null) {
            return tile;
        }
        return null;
    }

    @Override
    public TextureSetAnimation getAnimation(Comparable<String> comparable) {
        AtlasMap am = getAtlasMap();
        if (am != null) {
            List<TextureSetAnimation> tiles = am.getAnimations();
            for (TextureSetAnimation tile : tiles) {
                if (comparable.compareTo(tile.getId()) == 0) {
                    return tile;
                }
            }
        }
        return null;
    }

    @Override
    public FloatBuffer getTexCoords() {
        AtlasMap am = getAtlasMap();
        if (am != null) {
            return am.getTexCoordsBuffer();
        }
        return null;
    }

    @Override
    public int getVertexStart(TextureSetAnimation anim) {
        return getAtlasMap().getVertexStart(anim);
    }

    @Override
    public int getVertexCount(TextureSetAnimation anim) {
        return getAtlasMap().getVertexCount(anim);
    }

    @Override
    public int getOutlineVertexStart(TextureSetAnimation anim) {
        return getAtlasMap().getOutlineVertexStart(anim);
    }

    @Override
    public int getOutlineVertexCount(TextureSetAnimation anim) {
        return getAtlasMap().getOutlineVertexCount(anim);
    }

}
