package com.dynamo.cr.scene.graph;

import java.io.IOException;

import javax.media.opengl.GL;

import org.eclipse.core.runtime.CoreException;

import com.dynamo.cr.scene.resource.ModelResource;
import com.dynamo.cr.scene.resource.Resource;
import com.dynamo.cr.scene.util.Constants;
import com.dynamo.cr.scene.util.Mesh;
import com.sun.opengl.util.texture.Texture;


public class ModelNode extends ComponentNode<ModelResource> {

    public static INodeCreator getCreator() {
        return new INodeCreator() {

            @Override
            public Node create(String identifier, Resource resource, Node parent,
                    Scene scene, INodeFactory factory) throws IOException,
                    CreateException, CoreException {
                return new ModelNode(identifier, (ModelResource)resource, scene);
            }
        };
    }

    public ModelNode(String identifier, ModelResource resource, Scene scene) {
        super(identifier, resource, scene);

        Mesh mesh = resource.getMeshResource().getMesh();
        if (mesh != null) {
            float[] p = mesh.positions;

            for (int i = 0; i < p.length/3; i++)
            {
                int j = i * 3;
                m_AABB.union(p[j], p[j+1], p[j+2]);
            }
        }
    }

    @Override
    public void draw(DrawContext context) {
        GL gl = context.m_GL;

        Texture texture = null;
        if (this.resource.getTextureResources().size() > 0) {
            texture = this.resource.getTextureResources().get(0).getTexture();
        }

        Mesh mesh = this.resource.getMeshResource().getMesh();
        if (texture != null && mesh.texCoords != null) {
            texture.enable();
            gl.glTexEnvf( GL.GL_TEXTURE_ENV, GL.GL_TEXTURE_ENV_MODE, GL.GL_MODULATE );
            texture.bind();
            gl.glEnable(GL.GL_BLEND);
        }

        if (context.isSelected(this))
            gl.glColor3fv(Constants.SELECTED_COLOR, 0);
        else
            gl.glColor3fv(Constants.OBJECT_COLOR, 0);

        gl.glBegin(GL.GL_TRIANGLES);
        float[] p = mesh.positions;
        float[] n = mesh.normals;
        float[] tex = mesh.texCoords;
        for (int i = 0; i < p.length/3; i++)
        {
            int j = i * 3;
            gl.glNormal3f(n[j], n[j+1], n[j+2]);
            if (tex != null) {
                gl.glTexCoord2f(tex[i*2], tex[i*2+1]);
            }
            gl.glVertex3f(p[j], p[j+1], p[j+2]);

        }
        gl.glEnd();

        if (texture != null && mesh.texCoords != null) {
            texture.disable();
            gl.glDisable(GL.GL_BLEND);
        }
    }
}
