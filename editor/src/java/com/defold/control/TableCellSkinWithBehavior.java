package com.defold.control;

import com.sun.javafx.scene.control.behavior.TableCellBehavior;
import com.sun.javafx.scene.control.skin.TableCellSkinBase;
import javafx.beans.property.BooleanProperty;
import javafx.beans.property.ReadOnlyDoubleProperty;
import javafx.scene.control.TableColumn;

// Part of workaround for https://bugs.openjdk.java.net/browse/JDK-8089514
// This is a reimplementation of the JavaFX TableCellSkin class with
// the sole purpose of allowing us to pass a custom TableCellBehavior.
public class TableCellSkinWithBehavior extends TableCellSkinBase<javafx.scene.control.TableCell<Object, Object>, TableCellBehavior<Object, Object>> {
    private final TableColumn<Object, Object> tableColumn;

    public TableCellSkinWithBehavior(javafx.scene.control.TableCell<Object, Object> tableCell, TableCellBehavior<Object, Object> behavior) {
        super(tableCell, behavior);
        this.tableColumn = tableCell.getTableColumn();
        super.init(tableCell);
    }

    @Override
    protected BooleanProperty columnVisibleProperty() {
        return tableColumn.visibleProperty();
    }

    @Override
    protected ReadOnlyDoubleProperty columnWidthProperty() {
        return tableColumn.widthProperty();
    }
}
