package com.dynamo.cr.sceneed.core;

import org.eclipse.swt.events.MouseEvent;
import org.eclipse.swt.events.MouseListener;
import org.eclipse.swt.events.MouseMoveListener;

public abstract class Manipulator extends Node implements MouseListener, MouseMoveListener {

    protected ManipulatorController controller;
    protected Node[] selection;

    public abstract boolean match(Object[] selection);

    public void setController(ManipulatorController controller) {
        this.controller = controller;
    }

    public void setSelection(Node[] selection) {
        this.selection = selection;
    }

    @Override
    public void mouseDoubleClick(MouseEvent e) {
    }

    @Override
    public void mouseDown(MouseEvent e) {
    }

    @Override
    public void mouseUp(MouseEvent e) {
    }

    @Override
    public void mouseMove(MouseEvent e) {
    }

}
