package com.dynamo.cr.tileeditor.scene;

import java.nio.FloatBuffer;
import java.util.EnumSet;
import java.util.List;

import javax.media.opengl.GL;
import javax.vecmath.Vector3d;

import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.RenderContext.Pass;
import com.dynamo.cr.sceneed.core.RenderData;
import com.dynamo.tile.TileSetUtil;
import com.sun.opengl.util.BufferUtil;
import com.sun.opengl.util.texture.Texture;
import com.sun.opengl.util.texture.TextureIO;

public class Sprite2Renderer implements INodeRenderer<Sprite2Node> {

    private static final float COLOR[] = new float[] { 1.0f, 1.0f, 1.0f, 1.0f };
    private static final EnumSet<Pass> passes = EnumSet.of(Pass.OUTLINE, Pass.TRANSPARENT, Pass.SELECTION);

    private static class UserData {
        private final Texture texture;
        private final FloatBuffer vertices;

        public UserData(Texture texture, FloatBuffer vertices) {
            this.texture = texture;
            this.vertices = vertices;
        }

        public Texture getTexture() {
            return this.texture;
        }

        public FloatBuffer getVertices() {
            return this.vertices;
        }
    }

    @Override
    public void setup(RenderContext renderContext, Sprite2Node node) {
        if (passes.contains(renderContext.getPass())) {
            // TODO Fix resource handling instead of creating the render data every frame
            UserData userData = createUserData(node);
            renderContext.add(this, node, new Vector3d(), userData);
        }
    }

    @Override
    public void render(RenderContext renderContext, Sprite2Node node,
            RenderData<Sprite2Node> renderData) {
        UserData userData = (UserData)renderData.getUserData();
        if (userData != null) {

            boolean useTexture = renderData.getPass() == Pass.TRANSPARENT;
            if (useTexture) {
                Texture texture = userData.getTexture();

                texture.bind();
                texture.setTexParameteri(GL.GL_TEXTURE_MIN_FILTER, GL.GL_NEAREST);
                texture.setTexParameteri(GL.GL_TEXTURE_MAG_FILTER, GL.GL_NEAREST);
                texture.setTexParameteri(GL.GL_TEXTURE_WRAP_S, GL.GL_CLAMP);
                texture.setTexParameteri(GL.GL_TEXTURE_WRAP_T, GL.GL_CLAMP);
                texture.enable();
            }

            GL gl = renderContext.getGL();
            gl.glColor4fv(renderContext.selectColor(node, COLOR), 0);

            gl.glEnableClientState(GL.GL_VERTEX_ARRAY);
            gl.glEnableClientState(GL.GL_TEXTURE_COORD_ARRAY);

            gl.glInterleavedArrays(GL.GL_T2F_V3F, 0, userData.getVertices());

            gl.glDrawArrays(GL.GL_QUADS, 0, 4);

            gl.glDisableClientState(GL.GL_VERTEX_ARRAY);
            gl.glDisableClientState(GL.GL_TEXTURE_COORD_ARRAY);

            if (useTexture) {
                Texture texture = userData.getTexture();
                texture.disable();
                // TODO dispose since we currently create the texture every frame, see above
                texture.dispose();
            }
        }
    }

    private static UserData createUserData(Sprite2Node node) {
        TileSetNode tileSet = node.getTileSetNode();
        if (tileSet == null || tileSet.getLoadedImage() == null || !node.validate().isOK() || !tileSet.validate().isOK()) {
            return null;
        }
        Texture texture = TextureIO.newTexture(tileSet.getLoadedImage(), false);
        if (texture == null) {
            return null;
        }

        TileSetUtil.Metrics metrics = calculateMetrics(tileSet);
        if (metrics == null) {
            return null;
        }

        int tile = 0;
        List<AnimationNode> animations = tileSet.getAnimations();
        for (AnimationNode animation : animations) {
            if (animation.getId().equals(node.getDefaultAnimation())) {
                tile = animation.getStartTile() - 1;
                break;
            }
        }

        float recipImageWidth = 1.0f / metrics.tileSetWidth;
        float recipImageHeight = 1.0f / metrics.tileSetHeight;

        int tileWidth = tileSet.getTileWidth();
        int tileHeight = tileSet.getTileHeight();

        float f = 0.5f;
        float x0 = -f * tileWidth;
        float x1 = f * tileWidth;
        float y0 = -f * tileHeight;
        float y1 = f * tileHeight;

        int tileMargin = tileSet.getTileMargin();
        int tileSpacing = tileSet.getTileSpacing();
        int x = tile % metrics.tilesPerRow;
        int y = tile / metrics.tilesPerRow;
        float u0 = (x * (tileSpacing + 2*tileMargin + tileWidth) + tileMargin) * recipImageWidth;
        float u1 = u0 + tileWidth * recipImageWidth;
        float v0 = (y * (tileSpacing + 2*tileMargin + tileHeight) + tileMargin) * recipImageHeight;
        float v1 = v0 + tileHeight * recipImageHeight;


        final int vertexCount = 4;
        final int componentCount = 5;
        FloatBuffer v = BufferUtil.newFloatBuffer(vertexCount * componentCount);
        float z = 0.0f;
        v.put(u0); v.put(v1); v.put(x0); v.put(y0); v.put(z);
        v.put(u0); v.put(v0); v.put(x0); v.put(y1); v.put(z);
        v.put(u1); v.put(v0); v.put(x1); v.put(y1); v.put(z);
        v.put(u1); v.put(v1); v.put(x1); v.put(y0); v.put(z);
        v.flip();

        return new UserData(texture, v);
    }

    private static TileSetUtil.Metrics calculateMetrics(TileSetNode node) {
        return TileSetUtil.calculateMetrics(node.getLoadedImage(), node.getTileWidth(), node.getTileHeight(), node.getTileMargin(), node.getTileSpacing(), null, 1.0f, 0.0f);
    }

}
