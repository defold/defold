package com.dynamo.cr.ddfeditor.scene;

import java.nio.FloatBuffer;
import java.util.EnumSet;

import javax.media.opengl.GL2;
import javax.vecmath.Point3d;

import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.TextureHandle;
import com.dynamo.cr.sceneed.core.RenderContext.Pass;
import com.dynamo.cr.sceneed.core.RenderData;

import com.jogamp.opengl.util.texture.Texture;

public class MeshCompRenderer implements INodeRenderer<MeshCompNode> {

    private static final float COLOR[] = new float[] { 1.0f, 1.0f, 1.0f, 1.0f };
    private static final EnumSet<Pass> passes = EnumSet.of(Pass.OUTLINE, Pass.TRANSPARENT, Pass.SELECTION);

    public MeshCompRenderer() {
    }

    @Override
    public void dispose(GL2 gl) {
    }

    @Override
    public void setup(RenderContext renderContext, MeshCompNode node) {
        // MeshNode mesh = node.getMeshNode();
        // if (mesh != null && passes.contains(renderContext.getPass())) {
        //     renderContext.add(this, node, new Point3d(), mesh);
        // }
    }

    @Override
    public void render(RenderContext renderContext, MeshCompNode node,
            RenderData<MeshCompNode> renderData) {

        // MeshNode mesh = node.getMeshNode();
        // FloatBuffer pos = mesh.getPositions();
        // FloatBuffer texCoords0 = mesh.getTexCoords0();

        // if(pos == null || texCoords0 == null) {
        //     return;
        // }

        // GL2 gl = renderContext.getGL();
        // gl.glColor4fv(renderContext.selectColor(node, COLOR), 0);

        // Texture texture = null;
        // boolean transparent = renderData.getPass() == Pass.TRANSPARENT;
        // if (transparent) {
        //     TextureHandle textureHandle = node.getTextureHandle();
        //     if(textureHandle != null) {
        //         texture = textureHandle.getTexture(gl);
        //         if(texture != null) {
        //             texture.bind(gl);
        //             texture.setTexParameteri(gl, GL2.GL_TEXTURE_MIN_FILTER, GL2.GL_LINEAR);
        //             texture.setTexParameteri(gl, GL2.GL_TEXTURE_MAG_FILTER, GL2.GL_LINEAR);
        //             texture.setTexParameteri(gl, GL2.GL_TEXTURE_WRAP_S, GL2.GL_REPEAT);
        //             texture.setTexParameteri(gl, GL2.GL_TEXTURE_WRAP_T, GL2.GL_REPEAT);
        //             texture.enable(gl);
        //         }
        //     }
        //     gl.glEnable(GL2.GL_DEPTH_TEST);
        //     gl.glDepthMask(true);
        //     gl.glDepthFunc(GL2.GL_LEQUAL);
        // }

        // gl.glClientActiveTexture(GL2.GL_TEXTURE0);
        // gl.glEnableClientState(GL2.GL_VERTEX_ARRAY);
        // gl.glEnableClientState (GL2.GL_TEXTURE_COORD_ARRAY);

        // gl.glVertexPointer(3, GL2.GL_FLOAT, 0, pos);
        // gl.glTexCoordPointer(2, GL2.GL_FLOAT, 0, texCoords0);

        // gl.glDrawArrays(GL2.GL_TRIANGLES, 0, pos.capacity()/3);

        // gl.glDisableClientState(GL2.GL_TEXTURE_COORD_ARRAY);
        // gl.glDisableClientState(GL2.GL_VERTEX_ARRAY);

        // if (texture != null) {
        //     texture.disable(gl);
        // }
        // if(transparent) {
        //     gl.glDisable(GL2.GL_DEPTH_TEST);
        //     gl.glDepthMask(false);
        // }
    }

}
