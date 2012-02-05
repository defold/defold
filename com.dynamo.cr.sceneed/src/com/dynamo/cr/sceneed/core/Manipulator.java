package com.dynamo.cr.sceneed.core;

import org.eclipse.swt.events.MouseEvent;
import org.eclipse.swt.events.MouseListener;
import org.eclipse.swt.events.MouseMoveListener;

public abstract class Manipulator extends Node implements MouseListener, MouseMoveListener {

    private ManipulatorController controller;
    private IManipulatorMode mode;

    public abstract boolean match(Object[] selection);

    public ManipulatorController getController() {
        return controller;
    }

    public final void setController(ManipulatorController controller) {
        this.controller = controller;
        for (Node c : getChildren()) {
            Manipulator m = (Manipulator) c;
            m.setController(controller);
        }
    }

    public IManipulatorMode getMode() {
        return this.mode;
    }

    public void setMode(IManipulatorMode mode) {
        this.mode = mode;
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
