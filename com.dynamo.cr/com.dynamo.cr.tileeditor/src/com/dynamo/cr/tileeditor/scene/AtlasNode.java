package com.dynamo.cr.tileeditor.scene;

import java.awt.image.BufferedImage;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import javax.media.opengl.GL2;

import org.eclipse.core.resources.IFile;

import com.dynamo.bob.pipeline.AtlasUtil.MappedAnimIterator;
import com.dynamo.bob.pipeline.AtlasUtil;
import com.dynamo.bob.textureset.TextureSetGenerator;
import com.dynamo.bob.textureset.TextureSetGenerator.TextureSetResult;
import com.dynamo.cr.properties.GreaterEqualThanZero;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.sceneed.core.AABB;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.TextureHandle;
import com.dynamo.textureset.proto.TextureSetProto.TextureSet;
import com.dynamo.tile.proto.Tile.Playback;

@SuppressWarnings("serial")
public class AtlasNode extends TextureSetNode {
    private int version = 0;
    private int cleanVersion = -1;

    @Property
    @GreaterEqualThanZero
    private int margin;

    @Property
    @GreaterEqualThanZero
    private int extrudeBorders;

    @Property
    @GreaterEqualThanZero
    private int innerPadding;

    private transient AtlasAnimationNode playBackNode;
    private transient TextureHandle textureHandle = new TextureHandle();
    private transient RuntimeTextureSet runtimeTextureSet = new RuntimeTextureSet();

    public AtlasNode() {
        AABB aabb = new AABB();
        aabb.union(0, 0, 0);
        aabb.union(512, 512, 0);
        setAABB(aabb);
    }

    @Override
    public void dispose(GL2 gl) {
        super.dispose(gl);
        textureHandle.clear(gl);
        runtimeTextureSet.dispose(gl);
    }

    public int getMargin() {
        return margin;
    }

    public void setMargin(int margin) {
        this.margin = margin;
        increaseVersion();
    }

    public int getExtrudeBorders() {
        return extrudeBorders;
    }

    public void setExtrudeBorders(int extrudeBorders) {
        this.extrudeBorders = extrudeBorders;
        increaseVersion();
    }

    public int getInnerPadding() {
        return innerPadding;
    }

    public void setInnerPadding(int innerPadding) {
        this.innerPadding = innerPadding;
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

    private static void collectAnimations(Node node, List<AtlasUtil.MappedAnimDesc> animations) {
        for (Node n : node.getChildren()) {
            if (n instanceof AtlasAnimationNode) {
                AtlasAnimationNode animNode = (AtlasAnimationNode) n;
                List<Node> children = n.getChildren();
                List<String> ids = new ArrayList<String>(children.size());
                for (Node n2 : children) {
                    AtlasImageNode imageNode = (AtlasImageNode) n2;
                    ids.add(imageNode.getImage());
                }
                animations.add(new AtlasUtil.MappedAnimDesc(animNode.getId(), ids, animNode.getPlayback(), animNode
                        .getFps(), animNode.isFlipHorizontally(), animNode.isFlipVertically()));
            } else if (n instanceof AtlasImageNode) {
                AtlasImageNode imageNode = (AtlasImageNode) n;
                String id = imageNode.getId();
                boolean found = false;
                for (AtlasUtil.MappedAnimDesc animDesc : animations) {
                    if (animDesc.getId().equals(id)) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    List<String> ids = Collections.singletonList(imageNode.getImage());
                    animations.add(new AtlasUtil.MappedAnimDesc(id, ids, Playback.PLAYBACK_NONE, 30, false, false));
                }
            }
        }
    }

    public int getVersion() {
        return version;
    }

    public RuntimeTextureSet getRuntimeTextureSet() {
        if (version != cleanVersion) {
            List<String> imageNames = new ArrayList<String>(64);
            List<String> ids = new ArrayList<String>(64);
            List<BufferedImage> images = new ArrayList<BufferedImage>(64);
            List<AtlasUtil.MappedAnimDesc> animations = new ArrayList<AtlasUtil.MappedAnimDesc>(32);
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
                MappedAnimIterator iterator = new MappedAnimIterator(animations, imageNames);
                TextureSetResult result = TextureSetGenerator.generate(images, iterator,
                        Math.max(0, margin),
                        Math.max(0, innerPadding),
                        Math.max(0, extrudeBorders), true, true, true, false, null);
                TextureSet textureSet = result.builder.setTexture("").build();
                runtimeTextureSet.update(textureSet, result.uvTransforms);

                AABB aabb = new AABB();
                aabb.union(0, 0, 0);
                BufferedImage image = result.image;
                aabb.union(image.getWidth(), image.getHeight(), 0);
                this.textureHandle.setImage(image);
                setAABB(aabb);
            }

            cleanVersion = version;
        }
        return runtimeTextureSet;
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

    @Override
    public List<String> getAnimationIds() {
        List<Node> children = getChildren();
        List<String> animationIds = new ArrayList<String>(children.size());
        for (Node child : children) {
            if (child instanceof AtlasAnimationNode) {
                animationIds.add(((AtlasAnimationNode)child).getId());
            } else if (child instanceof AtlasImageNode) {
                animationIds.add(((AtlasImageNode)child).getId());
            }
        }
        return animationIds;
    }

    @Override
    public boolean handleReload(IFile file, boolean childWasReloaded) {
        boolean reloaded = false;
        List<String> images = new ArrayList<String>();
        List<String> ids = new ArrayList<String>();
        collectImages(this, images, ids);
        for (String image : images) {
            IFile imageFile = getModel().getFile(image);
            if (imageFile.equals(file)) {
                increaseVersion();
                reloaded = true;
                break;
            }
        }
        return reloaded;
    }
}
