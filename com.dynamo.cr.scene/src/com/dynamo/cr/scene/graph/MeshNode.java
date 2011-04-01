package com.dynamo.cr.scene.graph;

import javax.media.opengl.GL;

import com.dynamo.cr.scene.resource.TextureResource;
import com.dynamo.cr.scene.util.Constants;
import com.sun.opengl.util.texture.Texture;

public class MeshNode extends LeafNode {

    private Mesh mesh;
    public TextureResource textureResource;

    public MeshNode(String identifier, Scene scene, Mesh mesh) {
        super(identifier, scene);
        this.mesh = mesh;
    }

    @Override
    public void draw(DrawContext context) {
        GL gl = context.m_GL;

        Texture texture = null;
        if (textureResource != null)
            texture = textureResource.getTexture();

        if (texture != null && mesh.m_TexCoords != null) {
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
        float[] p = mesh.m_Positions;
        float[] n = mesh.m_Normals;
        float[] tex = mesh.m_TexCoords;
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

        if (texture != null && mesh.m_TexCoords != null) {
            texture.disable();
            gl.glDisable(GL.GL_BLEND);
        }
    }
}
