package com.dynamo.cr.scene.graph;

import java.io.IOException;
import java.nio.FloatBuffer;

import javax.media.opengl.GL;
import javax.vecmath.Point3d;

import org.eclipse.core.runtime.CoreException;

import com.dynamo.cr.scene.resource.Resource;
import com.dynamo.cr.scene.resource.TileGridResource;
import com.dynamo.cr.scene.resource.TileSetResource;
import com.dynamo.cr.scene.util.Constants;
import com.dynamo.tile.proto.Tile.TileCell;
import com.dynamo.tile.proto.Tile.TileGrid;
import com.dynamo.tile.proto.Tile.TileLayer;
import com.sun.opengl.util.texture.Texture;

public class TileGridNode extends ComponentNode<TileGridResource> {

    public static INodeCreator getCreator() {
        return new INodeCreator() {

            @Override
            public Node create(String identifier, Resource resource, Node parent,
                    Scene scene, INodeFactory factory) throws IOException,
                    CreateException, CoreException {
                return new TileGridNode(identifier, (TileGridResource)resource, scene);
            }
        };
    }

    public TileGridNode(String identifier, TileGridResource tileGridResource, Scene scene) {
        super(identifier, tileGridResource, scene);
        this.m_AABB = tileGridResource.getAABB();
        if (tileGridResource.getTileSetResource() == null) {
            setError(ERROR_FLAG_RESOURCE_ERROR, "The tile set '" + tileGridResource.getTileGrid().getTileSet() + "' could not be loaded.");
        }
    }

    @Override
    public void draw(DrawContext context) {

        TileGrid tileGrid = this.resource.getTileGrid();

        GL gl = context.m_GL;

        FloatBuffer vb = this.resource.getVertexBuffer();

        if (vb != null) {
            Texture texture = null;
            if (this.resource.getTileSetResource() != null) {
                texture = this.resource.getTileSetResource().getTextureResource().getTexture();
            }

            boolean ghostMode = (getFlags() & FLAG_GHOST) == FLAG_GHOST;
            if (ghostMode) {
                gl.glColor3fv(Constants.GHOST_COLOR, 0);
            } else {
                if (texture != null) {
                    texture.enable();
                    gl.glTexEnvf(GL.GL_TEXTURE_ENV, GL.GL_TEXTURE_ENV_MODE, GL.GL_REPLACE);
                    gl.glTexParameteri(GL.GL_TEXTURE_2D, GL.GL_TEXTURE_MIN_FILTER, GL.GL_NEAREST);
                    gl.glTexParameteri(GL.GL_TEXTURE_2D, GL.GL_TEXTURE_MAG_FILTER, GL.GL_NEAREST);
                    gl.glTexParameteri(GL.GL_TEXTURE_2D, GL.GL_TEXTURE_WRAP_S, GL.GL_CLAMP);
                    gl.glTexParameteri(GL.GL_TEXTURE_2D, GL.GL_TEXTURE_WRAP_T, GL.GL_CLAMP);
                    texture.bind();
                }
                gl.glColor3f(1.0f, 1.0f, 1.0f);
            }

            gl.glPolygonMode(GL.GL_FRONT, GL.GL_FILL);

            gl.glEnable(GL.GL_BLEND);
            gl.glBlendFunc(GL.GL_SRC_ALPHA, GL.GL_ONE_MINUS_SRC_ALPHA);

            gl.glEnableClientState(GL.GL_VERTEX_ARRAY);
            gl.glEnableClientState(GL.GL_TEXTURE_COORD_ARRAY);

            gl.glInterleavedArrays(GL.GL_T2F_V3F, 0, vb);

            gl.glDrawArrays(GL.GL_QUADS, 0, vb.capacity() / 5);

            gl.glDisableClientState(GL.GL_VERTEX_ARRAY);
            gl.glDisableClientState(GL.GL_TEXTURE_COORD_ARRAY);

            gl.glDisable(GL.GL_BLEND);

            if (!ghostMode && texture != null) {
                texture.disable();
            }
        }

        gl.glPolygonMode(GL.GL_FRONT, GL.GL_LINE);

        if (context.isSelected(this)) {
            gl.glColor3fv(Constants.SELECTED_COLOR, 0);
        } else {
            gl.glColor3fv(Constants.OBJECT_COLOR, 0);
        }

        Point3d min = this.m_AABB.getMin();
        Point3d max = this.m_AABB.getMax();
        gl.glBegin(GL.GL_LINE_LOOP);
        gl.glVertex3d(min.getX(), max.getY(), 0);
        gl.glVertex3d(max.getX(), max.getY(), 0);
        gl.glVertex3d(max.getX(), min.getY(), 0);
        gl.glVertex3d(min.getX(), min.getY(), 0);
        gl.glEnd();

        gl.glPolygonMode(GL.GL_FRONT, GL.GL_FILL);
    }

    @Override
    protected boolean verifyChild(Node child) {
        return false;
    }
}
