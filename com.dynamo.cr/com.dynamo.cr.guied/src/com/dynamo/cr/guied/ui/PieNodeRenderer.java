package com.dynamo.cr.guied.ui;

import java.util.EnumSet;

import javax.media.opengl.GL;
import javax.media.opengl.GL2;
import javax.vecmath.Point3d;

import com.dynamo.cr.guied.core.ClippingNode;
import com.dynamo.cr.guied.core.GuiTextureNode;
import com.dynamo.cr.guied.core.PieNode;
import com.dynamo.cr.guied.core.ClippingNode.ClippingState;
import com.dynamo.cr.guied.util.Clipping;
import com.dynamo.cr.sceneed.core.AABB;
import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.RenderContext.Pass;
import com.dynamo.cr.sceneed.core.RenderData;
import com.dynamo.gui.proto.Gui.NodeDesc;
import com.dynamo.gui.proto.Gui.NodeDesc.Pivot;
import com.jogamp.opengl.util.texture.Texture;

public class PieNodeRenderer implements INodeRenderer<PieNode> {

    private static final EnumSet<Pass> passes = EnumSet.of(Pass.OUTLINE, Pass.TRANSPARENT, Pass.SELECTION);

    @Override
    public void dispose(GL2 gl) {
    }

    @Override
    public void setup(RenderContext renderContext, PieNode node) {
        if (passes.contains(renderContext.getPass())) {
            if((renderContext.getPass() == Pass.OUTLINE) && (node.isClipping()) && (!Clipping.getShowClippingNodes())) {
                return;
            }
            ClippingState clippingState = node.getClippingState();
            if (clippingState != null) {
                RenderData<PieNode> data = renderContext.add(this, node, new Point3d(), clippingState);
                data.setIndex(node.getClippingKey());
            }
            if (!node.isClipping() || node.getClippingVisible()) {
                ClippingState childState = null;
                ClippingNode clipper = node.getClosestParentClippingNode();
                if (clipper != null) {
                    childState = clipper.getChildClippingState();
                }
                RenderData<PieNode> data = renderContext.add(this, node, new Point3d(), childState);
                data.setIndex(node.getRenderKey());
            }
        }
    }

    private double pivotOffsetX(PieNode node, double width) {
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

    private double pivotOffsetY(PieNode node, double height) {
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
    public void render(RenderContext renderContext, PieNode node, RenderData<PieNode> renderData) {
        GL2 gl = renderContext.getGL();

        Texture texture = null;
        GuiTextureNode guiTextureNode = node.getGuiTextureNode();
        if (guiTextureNode != null) {
            texture = guiTextureNode.getTextureHandle().getTexture(gl);
        }

        boolean clipping = renderData.getUserData() != null;
        if (clipping && renderData.getPass() == Pass.TRANSPARENT) {
            Clipping.beginClipping(gl);
            ClippingState state = (ClippingState)renderData.getUserData();
            Clipping.setupClipping(gl, state);
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

        // these are the perimeter vertices
        int configPerimVertices = node.getPerimeterVertices();

        // Determine minimum allowed for generation.
         if (configPerimVertices < 4)
            configPerimVertices = 4;

        // Step, stop and how many iterations in the vertex generation loop.
        double angleStep = Math.PI * 2 / configPerimVertices;
        double angleSign = 1;

        double stopAngle = Math.PI * node.getPieFillAngle() / 180;
        if (stopAngle < 0)
        {
            angleSign = -1;
            stopAngle = -stopAngle;
        }

        if (stopAngle > 2 * Math.PI)
            stopAngle = 2 * Math.PI;

        int toGenerate = (int)Math.ceil(stopAngle / angleStep);

        // rectangle bounds can generate a few extra.
        int maxPerimeterVertices = (int)((2 * Math.PI / angleStep) + 7);

        double s[] = new double[maxPerimeterVertices];
        double c[] = new double[maxPerimeterVertices];

        int perimeterVertices = 0;
        if (node.getOuterBounds() == NodeDesc.PieBounds.PIEBOUNDS_ELLIPSE)
        {
            for (int i=0;i<=toGenerate;i++)
            {
                double a;
                if (i < toGenerate)
                    a = i * angleStep;
                else
                    a = stopAngle;

                s[i] = Math.sin(angleSign * a);
                c[i] = Math.cos(angleSign * a);
            }
            perimeterVertices = toGenerate + 1;
        }
        else
        {
            // at 45 deg angle.
            double nextCorner = 0.25 * Math.PI;
            double lastA = 0, a = 0;
            for (int i=0;i<=toGenerate;i++)
            {
                if (i == toGenerate)
                    a = stopAngle;

                // detect skipped over rectangle corner and insert a vertex.
                if (lastA < nextCorner && a > nextCorner)
                {
                    s[perimeterVertices] = Math.sin(angleSign * nextCorner);
                    c[perimeterVertices] = Math.cos(angleSign * nextCorner);
                    perimeterVertices++;

                    // pretend this was a part of the original plan
                    lastA = nextCorner;
                    nextCorner += 0.5 * Math.PI;
                    i--;
                    continue;
                }

                s[perimeterVertices] = Math.sin(angleSign * a);
                c[perimeterVertices] = Math.cos(angleSign * a);
                a += angleStep;
                perimeterVertices++;
            }
        }

        NodeDesc.PieBounds pb = node.getOuterBounds();

        // use x dimension for radius.
        double ref = Math.abs(x1-x0);
        double innerRadiusMultiplier = ref > 0 ? 2.0 * (node.getInnerRadius() / ref) : 0;

        if (renderContext.getPass() == Pass.OUTLINE && node.isClipping()) {
            if (renderContext.isSelected(node)) {
                gl.glColor4fv(Clipping.OUTLINE_SELECTED_COLOR, 0);
            } else {
                gl.glColor4fv(Clipping.OUTLINE_COLOR, 0);
            }
        } else {
            float[] color = node.calcNormRGBA();
            gl.glColor4fv(renderContext.selectColor(node, color), 0);
        }
        gl.glBegin(GL2.GL_TRIANGLE_STRIP);

        double u0,sU,v0,sV;
        boolean uvRotated;
        if(node.getGuiTextureNode() != null) {
            GuiTextureNode.UVTransform uv = node.getGuiTextureNode().getUVTransform();
            uvRotated = uv.rotated;
            if((uvRotated ? uv.flipY : uv.flipX)) {
                u0 = uv.translation.x + uv.scale.x;
                sU = -uv.scale.x;
            } else {
                u0 = uv.translation.x;
                sU = uv.scale.x;
            }
            if((uvRotated ? uv.flipX : uv.flipY)) {
                v0 = uv.translation.y + uv.scale.y;
                sV = -uv.scale.y;
            } else {
                v0 = uv.translation.y;
                sV = uv.scale.y;
            }
        } else {
            uvRotated = false;
            u0 = 0;
            sU = 1;
            v0 = 0;
            sV = 1;
        }

        // Generate strip.
        for (int i=0;i<perimeterVertices;i++)
        {
            // Inner vertex.s
            double u = 0.5 + 0.5 * innerRadiusMultiplier * c[i];
            double v = 0.5 + 0.5 * innerRadiusMultiplier * s[i];

            if(uvRotated) {
                gl.glTexCoord2d(u0 + (v*sU), v0 + (sV*(u)));
            } else {
                gl.glTexCoord2d(u0 + (u*sU), v0 + (sV*(1-v)));
            }
            gl.glVertex2d(x0 + u * (x1 - x0), y0 + v * (y1 - y0));

            // Outer vertex
            double d = 1;
            if (pb == NodeDesc.PieBounds.PIEBOUNDS_RECTANGLE)
                d = 1 / Math.max(Math.abs(s[i]), Math.abs(c[i]));

            u = 0.5 + 0.5 * d * c[i];
            v = 0.5 + 0.5 * d * s[i];

            if(uvRotated) {
                gl.glTexCoord2d(u0 + (v*sU), v0 + (sV*(u)));
            } else {
                gl.glTexCoord2d(u0 + (u*sU), v0 + (sV*(1-v)));
            }
            gl.glVertex2d(x0 + u * (x1 - x0), y0 + v * (y1 - y0));
        }

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

        if (clipping && renderData.getPass() == Pass.TRANSPARENT) {
            Clipping.endClipping(gl);
        }
    }

}
