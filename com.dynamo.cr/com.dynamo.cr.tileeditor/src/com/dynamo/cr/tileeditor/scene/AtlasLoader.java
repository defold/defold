package com.dynamo.cr.tileeditor.scene;

import java.io.IOException;
import java.io.InputStream;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.atlas.proto.AtlasProto.Atlas;
import com.dynamo.atlas.proto.AtlasProto.AtlasAnimation;
import com.dynamo.atlas.proto.AtlasProto.AtlasImage;
import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.INodeLoader;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.util.LoaderUtil;
import com.google.protobuf.Message;

public class AtlasLoader implements INodeLoader<AtlasNode> {

    @Override
    public AtlasNode load(ILoaderContext context, InputStream contents)
            throws IOException, CoreException {

        Atlas.Builder atlasBuilder = Atlas.newBuilder();
        LoaderUtil.loadBuilder(atlasBuilder, contents);
        Atlas atlas = atlasBuilder.build();

        AtlasNode node = new AtlasNode();
        node.setMargin(atlas.getMargin());
        node.setExtrudeBorders(atlas.getExtrudeBorders());
        for (AtlasImage image : atlas.getImagesList()) {
            node.addChild(new AtlasImageNode(image.getImage()));
        }

        for (AtlasAnimation anim : atlas.getAnimationsList()) {
            AtlasAnimationNode animNode = new AtlasAnimationNode();
            animNode.setId(anim.getId());
            animNode.setPlayback(anim.getPlayback());
            animNode.setFps(anim.getFps());
            animNode.setFlipHorizontally(anim.getFlipHorizontal() != 0);
            animNode.setFlipVertically(anim.getFlipVertical() != 0);

            for (AtlasImage image : anim.getImagesList()) {
                animNode.addChild(new AtlasImageNode(image.getImage()));
            }
            node.addChild(animNode);
        }

        return node;
    }

    @Override
    public Message buildMessage(ILoaderContext context, AtlasNode node,
            IProgressMonitor monitor) throws IOException, CoreException {

        Atlas.Builder atlasBuilder = Atlas.newBuilder();
        atlasBuilder.setMargin(node.getMargin());
        atlasBuilder.setExtrudeBorders(node.getExtrudeBorders());

        for (Node n : node.getChildren()) {
            if (n instanceof AtlasImageNode) {
                AtlasImageNode imageNode = (AtlasImageNode) n;
                atlasBuilder.addImages(AtlasImage.newBuilder().setImage(imageNode.getImage()));
            } else if (n instanceof AtlasAnimationNode) {
                AtlasAnimationNode animNode = (AtlasAnimationNode) n;
                AtlasAnimation.Builder b = AtlasAnimation.newBuilder();
                for (Node n2 : animNode.getChildren()) {
                    b.addImages(AtlasImage.newBuilder().setImage(((AtlasImageNode) n2).getImage()));
                }
                b.setId(animNode.getId());
                b.setPlayback(animNode.getPlayback());
                b.setFps(animNode.getFps());
                b.setFlipHorizontal(animNode.isFlipHorizontally() ? 1 : 0);
                b.setFlipVertical(animNode.isFlipVertically() ? 1 : 0);
                atlasBuilder.addAnimations(b);
            }
        }

        return atlasBuilder.build();
    }

}
