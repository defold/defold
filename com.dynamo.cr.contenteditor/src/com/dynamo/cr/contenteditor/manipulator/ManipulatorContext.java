package com.dynamo.cr.contenteditor.manipulator;

import org.eclipse.swt.SWT;
import org.eclipse.swt.events.KeyEvent;
import org.eclipse.swt.events.MouseEvent;

import com.dynamo.cr.contenteditor.editors.IEditor;
import com.dynamo.cr.contenteditor.scene.Node;

public class ManipulatorContext
{
    public IEditor editor;
    public int mouseX;
    public int mouseY;
    public KeyEvent keyEvent;
    public Node[] nodes;
    public int manipulatorHandle;
    public boolean snapActive;

    public ManipulatorContext(IEditor editor, MouseEvent event, Node[] nodes,
            int manipulator_handle)
    {
        this.editor = editor;
        this.mouseX = event.x;
        this.mouseY = event.y;
        this.keyEvent = null;
        this.nodes = nodes;
        this.manipulatorHandle = manipulator_handle;
        this.snapActive = (event.stateMask & SWT.SHIFT) != 0;
    }

    public ManipulatorContext(IEditor editor, KeyEvent event, Node[] nodes,
            int manipulator_handle)
    {
        this.editor = editor;
        this.mouseX = -1;
        this.mouseY = -1;
        this.keyEvent = event;
        this.nodes = nodes;
        this.manipulatorHandle = manipulator_handle;
        this.snapActive = (event.stateMask & SWT.SHIFT) != 0;
    }
}
