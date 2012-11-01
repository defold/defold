package com.dynamo.cr.parted.curve;

import javax.vecmath.Point2d;
import javax.vecmath.Vector2d;

import org.eclipse.jface.viewers.ISelection;

public interface ICurveView {
    public interface IPresenter {
        void onAddPoint(Point2d p);
        void onRemove();

        void onStartDrag(Point2d start, Vector2d screenScale, double screenDragPadding, double screenHitPadding, double screenTangentLength);
        void onDrag(Point2d position);
        void onEndDrag();

        void onSelectAll();
        void onDeselectAll();
        void onPickSelect(Point2d start, Vector2d screenScale, double screenHitPadding);
    }

    void setInput(Object input);
    void setSelection(ISelection selection);
    void setSelectionBox(Point2d boxMin, Point2d boxMax);
    void refresh();
    void frame();
}
