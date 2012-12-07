package com.dynamo.cr.ddfeditor.scene;

import java.net.URL;

import javax.media.opengl.GL;
import javax.vecmath.Matrix4d;
import javax.vecmath.Point3d;
import javax.vecmath.Vector4d;

import com.dynamo.cr.sceneed.core.Camera;
import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.RenderContext.Pass;
import com.dynamo.cr.sceneed.core.RenderData;
import com.sun.opengl.util.texture.Texture;

public abstract class IconRenderer<T extends Node> implements INodeRenderer<T> {

    private static final float[] white = new float[] { 1, 1, 1, 1 };
    private boolean iconLoaded = false;
    private Texture icon;

    public IconRenderer() {
    }

    @Override
    public void dispose(GL gl) {
    }

    @Override
    public void setup(RenderContext renderContext, T node) {
        Pass pass = renderContext.getPass();
        if (pass == Pass.ICON_SELECTION || pass == Pass.ICON || pass == Pass.ICON_OUTLINE) {
            renderContext.add(this, node, new Point3d(), null);
        }
    }

    private void loadIcon(RenderContext renderContext) {
        if (iconLoaded)
            return;

        iconLoaded = true;

        URL url = getClass().getResource(getIconPath());
        icon = renderContext.getTextureRegistry().getResourceTexture(url);
    }

    @Override
    public void render(RenderContext renderContext, T node, RenderData<T> renderData) {
        loadIcon(renderContext);

        GL gl = renderContext.getGL();
        Matrix4d m = new Matrix4d();
        node.getWorldTransform(m);
        Vector4d t = new Vector4d();
        m.getColumn(3, t);
        Camera camera = renderContext.getRenderView().getCamera();
        Point3d pt = camera.project(t.getX(), t.getY(), t.getZ());

        double iconSize = 16;

        Pass pass = renderContext.getPass();
        if (icon != null && pass != Pass.ICON_OUTLINE) {
            icon.bind();
            icon.enable();
        }

        if (pass == Pass.ICON_OUTLINE) {
            float[] color = renderContext.selectColor(node, white);
            gl.glColor4fv(color, 0);
            iconSize = 20;
        } else {
            gl.glColor4f(1, 1, 1, 1.0f);
        }

        gl.glBegin(GL.GL_QUADS);
        gl.glTexCoord2f(0, 0);
        gl.glVertex3d(pt.getX() - 0.5 * iconSize, pt.getY() - 0.5 * iconSize, pt.getZ());
        gl.glTexCoord2f(1, 0);
        gl.glVertex3d(pt.getX() + 0.5 * iconSize, pt.getY() - 0.5 * iconSize, pt.getZ());
        gl.glTexCoord2f(1, 1);
        gl.glVertex3d(pt.getX() + 0.5 * iconSize, pt.getY() + 0.5 * iconSize, pt.getZ());
        gl.glTexCoord2f(0, 1);
        gl.glVertex3d(pt.getX() - 0.5 * iconSize, pt.getY() + 0.5 * iconSize, pt.getZ());
        gl.glEnd();

        if (icon != null && pass != Pass.ICON_OUTLINE) {
            icon.disable();
        }
    }

    protected abstract String getIconPath();

}