package com.dynamo.cr.sceneed.ui;

import javax.vecmath.Point2i;

import org.eclipse.swt.events.MouseEvent;

import com.dynamo.cr.sceneed.core.CameraController;
import com.dynamo.cr.sceneed.core.Manipulator;
import com.dynamo.cr.sceneed.core.Node;

public class SelectManipulator extends RootManipulator {

    private Point2i start;
    private Point2i current;

    public SelectManipulator() {
        super();
    }

    public Point2i getStart() {
        return this.start;
    }

    public Point2i getCurrent() {
        return this.current;
    }

    @Override
    public boolean match(Object[] selection) {
        if (selection.length == 0)
            return true;
        for (Object object : selection) {
            if (object instanceof Node) {
                return true;
            }
        }
        return false;
    }

    @Override
    protected void transformChanged() {
    }

    @Override
    public void manipulatorChanged(Manipulator manipulator) {
    }

    @Override
    protected void selectionChanged() {
    }

    @Override
    public void refresh() {
    }

    @Override
    public void mouseDown(MouseEvent e) {
        if (!CameraController.hasCameraControlModifiers(e)) {
            this.start = new Point2i(e.x, e.y);
            this.current = new Point2i(this.start);
            select(true);
        }
    }

    @Override
    public void mouseUp(MouseEvent e) {
        if (!CameraController.hasCameraControlModifiers(e)) {
            this.start = null;
            this.current = null;
        }
    }

    @Override
    public void mouseMove(MouseEvent e) {
        if (!CameraController.hasCameraControlModifiers(e)) {
            if (this.start != null) {
                this.current.set(e.x, e.y);
                select(false);
            }
        }
    }

    private void select(boolean single) {
        Point2i dims = new Point2i(this.current.x, this.current.y);
        dims.sub(this.start);
        Point2i center = new Point2i((int)Math.round(dims.x * 0.5), (int)Math.round(dims.y * 0.5));
        center.add(this.start);
        dims.absolute();
        dims.set(Math.max(16, dims.x), Math.max(16, dims.y));
        getController().getRenderView().boxSelect(center.x, center.y, dims.x, dims.y, single);
    }
}
