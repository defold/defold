package com.dynamo.cr.contenteditor.manipulator;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import javax.media.opengl.GL;

import org.eclipse.swt.events.MouseEvent;

import com.dynamo.cr.contenteditor.editors.IEditor;
import com.dynamo.cr.contenteditor.manipulator.ManipulatorContext.Pivot;
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
    /**
     * The pivot of the manipulation.
     */
    private ManipulatorContext.Pivot pivot = ManipulatorContext.Pivot.MANIPULATOR;

    Map<String, IManipulator> manipulators = new HashMap<String, IManipulator>();

    public ManipulatorController(IEditor editor)
    {
        this.editor = editor;

        manipulators.put("move", new MoveManipulator());
        manipulators.put("rotate", new RotateManipulator());
    }

    public void setManipulator(String manipulatorName)
    {
        IManipulator manipulator = manipulators.get(manipulatorName);
        if (this.manipulator == null || manipulator != this.manipulator) {
            this.manipulator = manipulator;
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

    public void setManipulatorPivot(String pivotName) {
        if (this.manipulator == null || !this.manipulator.isActive()) {
            if (pivotName.equals("local")) {
                this.pivot = Pivot.LOCAL;
            } else if (pivotName.equals("manipulator")) {
                this.pivot = Pivot.MANIPULATOR;
            } else if (pivotName.equals("next")) {
                if (this.pivot == Pivot.LOCAL) {
                    this.pivot = Pivot.MANIPULATOR;
                } else {
                    this.pivot = Pivot.LOCAL;
                }
            }
        }
    }

    public String getManipulatorPivotName() {
        switch (this.pivot) {
        case LOCAL:
            return "local";
        case MANIPULATOR:
            return "manipulator";
        default:
            return null;
        }
    }

    public void setSelected(Node[] selected_nodes)
    {
        // Filter out all nodes that aren't "transformable"
        // Selection from outline could be nodes that aren't transformable
        // Quick fix for an obsolete editor :-)
        List<Node> nodes = new ArrayList<Node>();
        for (Node node : selected_nodes) {
            if ((node.getFlags() & Node.FLAG_TRANSFORMABLE) == Node.FLAG_TRANSFORMABLE) {
                nodes.add(node);
            }
        }

        this.selected = nodes.toArray(new Node[nodes.size()]);
    }

    public void setActiveHandle(int handle)
    {
        this.manipulatorHandle = handle;
    }

    public void mouseDown(MouseEvent e)
    {
        if (this.manipulatorHandle != -1 && this.selected.length > 0)
        {
            ManipulatorContext ctx = new ManipulatorContext(this.editor, e, this.selected, this.manipulatorHandle, this.orientation, this.pivot);
            this.manipulator.mouseDown(ctx);
        }
    }

    public void mouseUp(MouseEvent e)
    {
        if (this.manipulatorHandle != -1 && this.selected.length > 0)
        {
            ManipulatorContext ctx = new ManipulatorContext(this.editor, e, this.selected, this.manipulatorHandle, this.orientation, this.pivot);
            this.manipulator.mouseUp(ctx);
        }
    }

    public void mouseMove(MouseEvent e)
    {
        if (this.manipulatorHandle != -1 && this.selected.length > 0 && this.manipulator.isActive()) {
            ManipulatorContext ctx = new ManipulatorContext(this.editor, e, this.selected, this.manipulatorHandle, this.orientation, this.pivot);
            this.manipulator.mouseMove(ctx);
        }
    }

    public void draw(GL gl, boolean select)
    {
        if (this.manipulator != null && this.selected.length > 0)
        {
            ManipulatorDrawContext ctx = new ManipulatorDrawContext(this.editor, gl, this.selected, this.manipulatorHandle, this.orientation, select, this.pivot);
            this.manipulator.draw(ctx);
        }
    }

}
