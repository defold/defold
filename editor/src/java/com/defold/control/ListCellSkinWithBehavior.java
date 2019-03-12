package com.defold.control;

import com.sun.javafx.scene.control.behavior.ListCellBehavior;
import com.sun.javafx.scene.control.skin.CellSkinBase;
import javafx.geometry.Orientation;
import javafx.scene.control.ListCell;
import javafx.scene.control.ListView;

// Part of workaround for https://bugs.openjdk.java.net/browse/JDK-8089514
// This is a reimplementation of the JavaFX ListCellSkin class with
// the sole purpose of allowing us to pass a custom ListCellBehavior.
public class ListCellSkinWithBehavior extends CellSkinBase<javafx.scene.control.ListCell<Object>, ListCellBehavior<Object>> {
    private static final double DEFAULT_CELL_SIZE = 24.0;

    private double fixedCellSize;
    private boolean fixedCellSizeEnabled;

    public ListCellSkinWithBehavior(ListCell<Object> control, ListCellBehavior<Object> behavior) {
        super(control, behavior);
        this.fixedCellSize = control.getListView().getFixedCellSize();
        this.fixedCellSizeEnabled = fixedCellSize > 0;
        registerChangeListener(control.getListView().fixedCellSizeProperty(), "FIXED_CELL_SIZE");
    }

    @Override
    protected void handleControlPropertyChanged(String p) {
        super.handleControlPropertyChanged(p);

        if ("FIXED_CELL_SIZE".equals(p)) {
            this.fixedCellSize = getSkinnable().getListView().getFixedCellSize();
            this.fixedCellSizeEnabled = fixedCellSize > 0;
        }
    }

    @Override
    protected double computePrefWidth(double height, double topInset, double rightInset, double bottomInset, double leftInset) {
        double pref = super.computePrefWidth(height, topInset, rightInset, bottomInset, leftInset);
        ListView<Object> listView = getSkinnable().getListView();
        return listView == null ? 0 : listView.getOrientation() == Orientation.VERTICAL ? pref : Math.max(pref, getCellSize());
    }

    @Override
    protected double computePrefHeight(double width, double topInset, double rightInset, double bottomInset, double leftInset) {
        if (fixedCellSizeEnabled) {
            return fixedCellSize;
        }

        final double cellSize = getCellSize();
        return cellSize == DEFAULT_CELL_SIZE ? super.computePrefHeight(width, topInset, rightInset, bottomInset, leftInset) : cellSize;
    }

    @Override
    protected double computeMinHeight(double width, double topInset, double rightInset, double bottomInset, double leftInset) {
        return fixedCellSizeEnabled ? fixedCellSize : super.computeMinHeight(width, topInset, rightInset, bottomInset, leftInset);
    }

    @Override
    protected double computeMaxHeight(double width, double topInset, double rightInset, double bottomInset, double leftInset) {
        return fixedCellSizeEnabled ? fixedCellSize : super.computeMaxHeight(width, topInset, rightInset, bottomInset, leftInset);
    }
}
