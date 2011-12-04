package com.dynamo.cr.sceneed.core;

import org.eclipse.swt.events.MouseListener;
import org.eclipse.swt.events.MouseMoveListener;

public abstract class Manipulator extends Node implements MouseListener, MouseMoveListener {

    public abstract boolean match(Object[] selection);

    public abstract void setSelection(Node[] selection);

    public abstract void setController(ManipulatorController controller);

}
