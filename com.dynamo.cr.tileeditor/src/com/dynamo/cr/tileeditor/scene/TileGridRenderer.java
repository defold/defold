package com.dynamo.cr.tileeditor.scene;

import java.util.EnumSet;

import javax.vecmath.Vector3d;

import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.RenderContext.Pass;
import com.dynamo.cr.sceneed.core.RenderData;

public class TileGridRenderer implements INodeRenderer<TileGridNode> {

    private static final float COLOR[] = new float[] { 1.0f, 1.0f, 1.0f, 1.0f };
    private static final EnumSet<Pass> passes = EnumSet.of(Pass.OUTLINE, Pass.TRANSPARENT, Pass.SELECTION);

    @Override
    public void setup(RenderContext renderContext, TileGridNode node) {
        if (passes.contains(renderContext.getPass())) {
            TileSetNode tileSet = node.getTileSetNode();
            if (tileSet != null && tileSet.getTextureHandle().getTexture() != null && node.getVertexData() != null) {
                renderContext.add(this, node, new Vector3d(), null);
            }
        }
    }

    @Override
    public void render(RenderContext renderContext, TileGridNode node, RenderData<TileGridNode> renderData) {
        // TODO Auto-generated method stub

    }

}
