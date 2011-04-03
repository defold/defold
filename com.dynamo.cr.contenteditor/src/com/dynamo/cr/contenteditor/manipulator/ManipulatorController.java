package com.dynamo.cr.contenteditor.manipulator;

import java.util.HashMap;
import java.util.Map;

import javax.media.opengl.GL;

import org.eclipse.swt.events.KeyEvent;
import org.eclipse.swt.events.MouseEvent;

import com.dynamo.cr.contenteditor.editors.IEditor;
import com.dynamo.cr.scene.graph.Node;

public class ManipulatorController
{
    private Node[] selected = new Node[0];
    private IManipulator manipulator;
    private int manipulatorHandle = -1;
    private final IEditor editor;

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
            ManipulatorContext ctx = new ManipulatorContext(this.editor, e, this.selected, this.manipulatorHandle);
            this.manipulator.mouseDown(ctx);
        }
    }

    public void mouseUp(MouseEvent e)
    {
        if (this.manipulatorHandle != -1 && this.selected.length > 0)
        {
            ManipulatorContext ctx = new ManipulatorContext(this.editor, e, this.selected, this.manipulatorHandle);
            this.manipulator.mouseUp(ctx);
        }
        this.manipulatorHandle = -1;
    }

    public void mouseMove(MouseEvent e)
    {
        if (this.manipulatorHandle != -1 && this.selected.length > 0)
        {
            ManipulatorContext ctx = new ManipulatorContext(this.editor, e, this.selected, this.manipulatorHandle);
            this.manipulator.mouseMove(ctx);
        }
    }

    public void draw(GL gl, boolean select)
    {
        if (this.manipulator != null && this.selected.length > 0)
        {
            ManipulatorDrawContext ctx = new ManipulatorDrawContext(this.editor, gl, this.selected, this.manipulatorHandle, select);
            this.manipulator.draw(ctx);
        }
    }

    public void keyPressed(KeyEvent e)
    {
        if (this.manipulator != null && this.selected.length > 0)
        {
            ManipulatorContext ctx = new ManipulatorContext(this.editor, e, this.selected, this.manipulatorHandle);
            this.manipulator.keyPressed(ctx);
        }
    }
}
