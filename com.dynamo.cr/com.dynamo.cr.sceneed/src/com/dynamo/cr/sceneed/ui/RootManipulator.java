package com.dynamo.cr.sceneed.ui;

import java.util.ArrayList;
import java.util.List;

import com.dynamo.cr.sceneed.core.Manipulator;
import com.dynamo.cr.sceneed.core.Node;

@SuppressWarnings("serial")
public abstract class RootManipulator extends Manipulator {

    public RootManipulator() {
        super(new float[] { 1, 1, 1, 1 } );
    }

    private List<Node> selection = new ArrayList<Node>();

    /**
     * Called by child manipulators to indicate that the transform has changed
     */
    protected abstract void transformChanged();

    public void setSelection(List<Node> selection) {
        this.selection.clear();
        this.selection.addAll(selection);
        selectionChanged();
    }

    public List<Node> getSelection() {
        return selection;
    }

    protected abstract void selectionChanged();

    public abstract void manipulatorChanged(Manipulator manipulator);

    /**
     * Called when to synchronize the manipulator with current selection
     * To invoke selectionChanged() is sufficient for many manipulators
     */
    public abstract void refresh();

}
