package com.dynamo.cr.guied.ui;

import java.util.EnumSet;

import javax.media.opengl.GL;
import javax.media.opengl.GL2;
import javax.vecmath.Point3d;

import org.eclipse.swt.graphics.RGB;

import com.dynamo.cr.guied.core.BoxNode;
import com.dynamo.cr.sceneed.core.AABB;
import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.RenderContext.Pass;
import com.dynamo.cr.sceneed.core.RenderData;
import com.dynamo.gui.proto.Gui.NodeDesc.Pivot;
import com.jogamp.opengl.util.texture.Texture;

public class BoxNodeRenderer implements INodeRenderer<BoxNode> {

    private static final EnumSet<Pass> passes = EnumSet.of(Pass.OUTLINE, Pass.TRANSPARENT, Pass.SELECTION);

    @Override
    public void dispose(GL2 gl) {
    }

    @Override
    public void setup(RenderContext renderContext, BoxNode node) {
        if (passes.contains(renderContext.getPass())) {
            RenderData<BoxNode> data = renderContext.add(this, node, new Point3d(), null);
            data.setIndex(node.getRenderKey());
        }
    }

    private double pivotOffsetX(BoxNode node, double width) {
        Pivot p = node.getPivot();

        switch (p) {
        case PIVOT_CENTER:
        case PIVOT_S:
        case PIVOT_N:
            return width * 0.5;

        case PIVOT_NE:
        case PIVOT_E:
        case PIVOT_SE:
            return width;

        case PIVOT_SW:
        case PIVOT_W:
        case PIVOT_NW:
            return 0;
        }

        assert false;
        return Double.MAX_VALUE;
    }

    private double pivotOffsetY(BoxNode node, double height) {
        Pivot p = node.getPivot();

        switch (p) {
        case PIVOT_CENTER:
        case PIVOT_E:
        case PIVOT_W:
            return height * 0.5;

        case PIVOT_N:
        case PIVOT_NE:
        case PIVOT_NW:
            return height;

        case PIVOT_S:
        case PIVOT_SW:
        case PIVOT_SE:
            return 0;
        }

        assert false;
        return Double.MAX_VALUE;
    }

    @Override
    public void render(RenderContext renderContext, BoxNode node, RenderData<BoxNode> renderData) {
        GL2 gl = renderContext.getGL();

        Texture texture = null;
        if (node.getTextureHandle() != null) {
            texture = node.getTextureHandle().getTexture(gl);
        }

        boolean transparent = renderData.getPass() == Pass.TRANSPARENT;
        if (transparent) {
            if (texture != null) {
                texture.bind(gl);
                texture.setTexParameteri(gl, GL.GL_TEXTURE_MIN_FILTER, GL.GL_LINEAR);
                texture.setTexParameteri(gl, GL.GL_TEXTURE_MAG_FILTER, GL.GL_NEAREST);
                texture.setTexParameteri(gl, GL.GL_TEXTURE_WRAP_S, GL2.GL_CLAMP);
                texture.setTexParameteri(gl, GL.GL_TEXTURE_WRAP_T, GL2.GL_CLAMP);
                texture.enable(gl);
            }

            switch (node.getBlendMode()) {
            case BLEND_MODE_ALPHA:
                gl.glBlendFunc(GL.GL_SRC_ALPHA, GL.GL_ONE_MINUS_SRC_ALPHA);
                break;
            case BLEND_MODE_ADD:
            case BLEND_MODE_ADD_ALPHA:
                gl.glBlendFunc(GL.GL_SRC_ALPHA, GL.GL_ONE);
                break;
            case BLEND_MODE_MULT:
                gl.glBlendFunc(GL.GL_ZERO, GL.GL_SRC_COLOR);
                break;
            }
        }

        double x0 = -pivotOffsetX(node, node.getSize().x);
        double y0 = -pivotOffsetY(node, node.getSize().y);
        double x1 = x0 + node.getSize().x;
        double y1 = y0 + node.getSize().y;
        float[] color = new float[4];
        float inv = 1.0f / 255.0f;
        RGB rgb = node.getColor();
        color[0] = rgb.red * inv;
        color[1] = rgb.green * inv;
        color[2] = rgb.blue * inv;
        color[3] = (float) node.getAlpha();
        gl.glColor4fv(renderContext.selectColor(node, color), 0);
        gl.glBegin(GL2.GL_QUADS);
        gl.glTexCoord2d(0.0, 0.0);
        gl.glVertex2d(x0, y1);
        gl.glTexCoord2d(1.0, 0.0);
        gl.glVertex2d(x1, y1);
        gl.glTexCoord2d(1.0, 1.0);
        gl.glVertex2d(x1, y0);
        gl.glTexCoord2d(0.0, 1.0);
        gl.glVertex2d(x0, y0);
        gl.glEnd();
        
        // Update AABB
        AABB aabb = new AABB();
        aabb.union(x0, y0, 0.0);
        aabb.union(x1, y1, 0.0);
        node.setAABB(aabb);

        if (transparent) {
            if (texture != null) {
                texture.disable(gl);
            }
            gl.glBlendFunc(GL.GL_SRC_ALPHA, GL.GL_ONE_MINUS_SRC_ALPHA);
        }
    }

}
