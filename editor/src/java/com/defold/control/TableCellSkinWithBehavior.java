package com.defold.control;

import javafx.beans.property.ReadOnlyObjectProperty;
import javafx.scene.control.TableCell;
import javafx.scene.control.TableColumnBase;
import javafx.scene.control.skin.TableCellSkinBase;

// Part of workaround for https://bugs.openjdk.java.net/browse/JDK-8089514
// This is a reimplementation of the JavaFX TableCellSkin class with
// the sole purpose of allowing us to pass a custom TableCellBehavior.
public class TableCellSkinWithBehavior extends TableCellSkinBase<Object, Object, TableCell<Object, Object>> {

    private final TableCellBehavior behavior;

    public TableCellSkinWithBehavior(TableCell<Object, Object> tableCell, TableCellBehavior behavior) {
        super(tableCell);
        this.behavior = behavior;
    }

    @Override
    public void dispose() {
        super.dispose();
        behavior.dispose();
    }

    @Override
    public ReadOnlyObjectProperty<? extends TableColumnBase<Object, Object>> tableColumnProperty() {
        return getSkinnable().tableColumnProperty();
    }
}
