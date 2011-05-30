package com.dynamo.cr.contenteditor.manipulator;

import org.eclipse.swt.SWT;
import org.eclipse.swt.events.MouseEvent;

import com.dynamo.cr.contenteditor.editors.IEditor;
import com.dynamo.cr.scene.graph.Node;

public class ManipulatorContext
{
    public enum Pivot { LOCAL, MANIPULATOR };

    public IEditor editor;
    public int mouseX;
    public int mouseY;
    public Node[] nodes;
    public int manipulatorHandle;
    public boolean snapActive;
    public int orientation;
    public Pivot pivot;

    public ManipulatorContext(IEditor editor, MouseEvent event, Node[] nodes,
            int manipulator_handle, int space, Pivot pivot)
    {
        this.editor = editor;
        this.mouseX = event.x;
        this.mouseY = event.y;
        this.nodes = nodes;
        this.manipulatorHandle = manipulator_handle;
        this.snapActive = (event.stateMask & SWT.SHIFT) != 0;
        this.orientation = space;
        this.pivot = pivot;
    }

}
