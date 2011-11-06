package com.dynamo.cr.goprot;

import javax.media.opengl.GL;
import javax.media.opengl.glu.GLU;

import com.dynamo.cr.goprot.core.Node;

public interface INodeRenderer {
    void render(Node node, GL gl, GLU glu);
}
