package com.dynamo.cr.contenteditor.manipulator;

import java.util.HashMap;
import java.util.Map;

import javax.media.opengl.GL;

import org.eclipse.swt.events.MouseEvent;

import com.dynamo.cr.contenteditor.editors.IEditor;
import com.dynamo.cr.scene.graph.Node;

public class ManipulatorController
{
    /**
     * Defines local orientation manipulation.
     */
    protected final static int LOCAL = 0;
    /**
     * Defines global orientation manipulation.
     */
    protected final static int GLOBAL = 1;

    private Node[] selected = new Node[0];
    private IManipulator manipulator;
    private int manipulatorHandle = -1;
    private final IEditor editor;
    /**
     * The space space in which the manipulation takes place.
     */
    private int orientation = LOCAL;

    Map<String, IManipulator> manipulators = new HashMap<String, IManipulator>();

    public ManipulatorController(IEditor editor)
    {
        this.editor = editor;

        manipulators.put("move", new MoveManipulator());
        manipulators.put("rotate", new RotateManipulator());
    }

    public void setManipulator(String manipulator)
    {
        this.manipulator = manipulators.get(manipulator);
        if (this.manipulator == null) {
            this.manipulatorHandle = -1;
        }
    }

    public IManipulator getManipulator() {
        return this.manipulator;
    }

    public void setManipulatorOrientation(String orientationName) {
        if (this.manipulator == null || !this.manipulator.isActive()) {
            if (orientationName.equals("local")) {
                this.orientation = LOCAL;
            } else if (orientationName.equals("global")) {
                this.orientation = GLOBAL;
            } else if (orientationName.equals("next")) {
                if (this.orientation == LOCAL) {
                    this.orientation = GLOBAL;
                } else {
                    this.orientation = LOCAL;
                }
            }
        }
    }

    public String getManipulatorOrientationName() {
        switch (this.orientation) {
        case LOCAL:
            return "local";
        case GLOBAL:
            return "global";
        default:
            return null;
        }
    }

    public void setSelected(Node[] selected_nodes)
    {
        this.selected = selected_nodes;
    }

    public void setActiveHandle(int handle)
    {
        this.manipulatorHandle = handle;
    }

    public void mouseDown(MouseEvent e)
    {
        if (this.manipulatorHandle != -1 && this.selected.length > 0)
        {
            ManipulatorContext ctx = new ManipulatorContext(this.editor, e, this.selected, this.manipulatorHandle, this.orientation);
            this.manipulator.mouseDown(ctx);
        }
    }

    public void mouseUp(MouseEvent e)
    {
        if (this.manipulatorHandle != -1 && this.selected.length > 0)
        {
            ManipulatorContext ctx = new ManipulatorContext(this.editor, e, this.selected, this.manipulatorHandle, this.orientation);
            this.manipulator.mouseUp(ctx);
        }
        this.manipulatorHandle = -1;
    }

    public void mouseMove(MouseEvent e)
    {
        if (this.manipulatorHandle != -1 && this.selected.length > 0)
        {
            ManipulatorContext ctx = new ManipulatorContext(this.editor, e, this.selected, this.manipulatorHandle, this.orientation);
            this.manipulator.mouseMove(ctx);
        }
    }

    public void draw(GL gl, boolean select)
    {
        if (this.manipulator != null && this.selected.length > 0)
        {
            ManipulatorDrawContext ctx = new ManipulatorDrawContext(this.editor, gl, this.selected, this.manipulatorHandle, this.orientation, select);
            this.manipulator.draw(ctx);
        }
    }

}
