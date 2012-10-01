package com.dynamo.cr.sceneed.core;

import org.eclipse.swt.events.MouseEvent;
import org.eclipse.swt.events.MouseListener;
import org.eclipse.swt.events.MouseMoveListener;

@SuppressWarnings("serial")
public abstract class Manipulator extends Node implements MouseListener, MouseMoveListener {

    private static float[] DISABLED_COLOR = new float[] { 0.5f, 0.5f, 0.5f, 0.5f };

    private ManipulatorController controller;
    private boolean enabled = true;
    private float[] color;

    public abstract boolean match(Object[] selection);

    public Manipulator(float[] color) {
        this.color = color;
    }

    public ManipulatorController getController() {
        return controller;
    }

    public final float[] getColor() {
        if (isEnabled())
            return color;
        else
            return DISABLED_COLOR;
    }

    public final void setController(ManipulatorController controller) {
        this.controller = controller;
        for (Node c : getChildren()) {
            Manipulator m = (Manipulator) c;
            m.setController(controller);
        }
    }

    public boolean isEnabled() {
        return enabled;
    }

    public void setEnabled(boolean enabled) {
        this.enabled = enabled;
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
