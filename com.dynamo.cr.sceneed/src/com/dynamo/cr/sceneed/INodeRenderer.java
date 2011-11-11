package com.dynamo.cr.sceneed;

import javax.media.opengl.GL;
import javax.media.opengl.glu.GLU;

import com.dynamo.cr.sceneed.core.Node;

public interface INodeRenderer {
    void render(Node node, GL gl, GLU glu);
}
