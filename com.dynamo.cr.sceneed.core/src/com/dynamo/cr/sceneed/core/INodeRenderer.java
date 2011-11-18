package com.dynamo.cr.sceneed.core;

import javax.media.opengl.GL;
import javax.media.opengl.glu.GLU;

public interface INodeRenderer {
    void render(Node node, GL gl, GLU glu);
}
