package com.dynamo.cr.sceneed.core.test;

import org.eclipse.swt.events.MouseEvent;

import com.dynamo.cr.sceneed.core.Manipulator;
import com.dynamo.cr.sceneed.core.ManipulatorController;
import com.dynamo.cr.sceneed.core.Node;

public abstract class AbstractTestManipulator extends Manipulator {

    protected Node[] selection;
    protected ManipulatorController controller;

    public AbstractTestManipulator() {
        super();
    }

    @Override
    public void setController(ManipulatorController controller) {
        this.controller = controller;
    }

    @Override
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