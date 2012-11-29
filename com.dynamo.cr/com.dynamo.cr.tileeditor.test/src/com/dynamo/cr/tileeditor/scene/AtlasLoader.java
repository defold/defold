package com.dynamo.cr.tileeditor.scene;

import java.io.IOException;
import java.io.InputStream;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.atlas.proto.Atlas.AtlasAnimation;
import com.dynamo.atlas.proto.Atlas.ImageAtlas;
import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.INodeLoader;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.util.LoaderUtil;
import com.google.protobuf.Message;

public class AtlasLoader implements INodeLoader<AtlasNode> {

    @Override
    public AtlasNode load(ILoaderContext context, InputStream contents)
            throws IOException, CoreException {

        ImageAtlas.Builder atlasBuilder = ImageAtlas.newBuilder();
        LoaderUtil.loadBuilder(atlasBuilder, contents);
        ImageAtlas atlas = atlasBuilder.build();

        AtlasNode node = new AtlasNode();
        for (String image : atlas.getImagesList()) {
            node.addChild(new AtlasImageNode(image));
        }

        for (AtlasAnimation anim : atlas.getAnimationsList()) {
            AtlasAnimationNode animNode = new AtlasAnimationNode();
            animNode.setPlayback(anim.getPlayback());
            animNode.setFps(anim.getFps());
            animNode.setFlipHorizontally(anim.getFlipHorizontal() != 0);
            animNode.setFlipVertically(anim.getFlipVertical() != 0);

            for (String image : anim.getImagesList()) {
                animNode.addChild(new AtlasImageNode(image));
            }
            node.addChild(animNode);
        }

        return node;
    }

    @Override
    public Message buildMessage(ILoaderContext context, AtlasNode node,
            IProgressMonitor monitor) throws IOException, CoreException {

        ImageAtlas.Builder atlasBuilder = ImageAtlas.newBuilder();

        for (Node n : node.getChildren()) {
            if (n instanceof AtlasImageNode) {
                AtlasImageNode imageNode = (AtlasImageNode) n;
                atlasBuilder.addImages(imageNode.getImage());
            } else if (n instanceof AtlasAnimationNode) {
                AtlasAnimationNode animNode = (AtlasAnimationNode) n;
                AtlasAnimation.Builder b = AtlasAnimation.newBuilder();
                for (Node n2 : animNode.getChildren()) {
                    b.addImages(((AtlasImageNode) n2).getImage());
                }
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
