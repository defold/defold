package com.dynamo.cr.parted.curve;

import javax.vecmath.Point2d;

public interface ICurveView {
    public interface IPresenter {
        void onAddPoint(Point2d p);
        void onRemove();

        void onStartMove(Point2d start);
        void onMove(Point2d position);
        void onEndMove();
        void onCancelMove();

        void onStartMoveTangent(Point2d start);
        void onMoveTangent(Point2d position);
        void onEndMoveTangent();
        void onCancelMoveTangent();

        void onStartSelect(Point2d start, double padding);
        void onSelect(Point2d position);
        void onEndSelect();
        void onCancelSelect();

        void onSelectAll();
    }

    void setInput(Object object);
    void refresh();
}
