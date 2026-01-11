// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

package com.defold.control;

import com.sun.javafx.scene.control.behavior.ListCellBehavior;
import javafx.geometry.Orientation;
import javafx.scene.control.ListCell;
import javafx.scene.control.ListView;
import javafx.scene.control.skin.CellSkinBase;

// Part of workaround for https://bugs.openjdk.java.net/browse/JDK-8089514
// This is a reimplementation of the JavaFX ListCellSkin class with
// the sole purpose of allowing us to pass a custom ListCellBehavior.
public class ListCellSkinWithBehavior extends CellSkinBase<ListCell<Object>> {
    private static final double DEFAULT_CELL_SIZE = 24.0;

    private double fixedCellSize;
    private final ListCellBehavior<Object> behavior;
    private boolean fixedCellSizeEnabled;

    public ListCellSkinWithBehavior(ListCell<Object> control, ListCellBehavior<Object> behavior) {
        super(control);
        this.fixedCellSize = control.getListView().getFixedCellSize();
        this.behavior = behavior;
        this.fixedCellSizeEnabled = fixedCellSize > 0;
        registerChangeListener(control.getListView().fixedCellSizeProperty(), (x) -> {
            this.fixedCellSize = getSkinnable().getListView().getFixedCellSize();
            this.fixedCellSizeEnabled = fixedCellSize > 0;
        });
    }

    @Override
    public void dispose() {
        super.dispose();
        behavior.dispose();
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
