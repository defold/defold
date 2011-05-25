package com.dynamo.cr.contenteditor.manipulator;

import javax.media.opengl.GL;

import com.dynamo.cr.contenteditor.editors.IEditor;
import com.dynamo.cr.scene.graph.Node;

public class ManipulatorDrawContext
{
    /**
     * GL interface
     */
    public IEditor editor;
    public GL gl;
    public Node[] nodes;
    public int manipulatorHandle;
    public int orientation;

    private final boolean selectMode;

    /**
     * Constructor
     * @param gl GL handle
     * @param select_mode Select mode (OpenGL picking)
     */
    public ManipulatorDrawContext(IEditor editor, GL gl, Node[] selected, int manipulatorHandle, int space, boolean select_mode)
    {
        this.editor = editor;
        this.gl = gl;
        this.nodes = selected;
        this.manipulatorHandle = manipulatorHandle;
        this.orientation = space;
        this.selectMode = select_mode;
    }

    /**
     * Check if select mode (picking)
     * @return True if in select mode
     */
    public boolean selectMode()
    {
        return this.selectMode;
    }

    /**
     * Called before drawing a handle. Used for handle selection (picking).
     */
    public void beginDrawHandle(int handle)
    {
        if (this.selectMode)
            this.gl.glPushName(handle);
    }

    /**
     * Called after drawing of handle is completed.
     */
    public void endDrawHandle()
    {
        if (this.selectMode)
            this.gl.glPopName();
    }
}
