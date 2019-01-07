package com.defold.control;

import javafx.beans.property.ReadOnlyObjectProperty;
import javafx.beans.property.ReadOnlyObjectWrapper;
import javafx.scene.control.TableCell;
import javafx.scene.control.TableColumn;
import javafx.scene.control.TableColumnBase;
import javafx.scene.control.skin.TableCellSkinBase;

// Part of workaround for https://bugs.openjdk.java.net/browse/JDK-8089514
// This is a reimplementation of the JavaFX TableCellSkin class with
// the sole purpose of allowing us to pass a custom TableCellBehavior.
public class TableCellSkinWithBehavior extends TableCellSkinBase<Object, Object, TableCell<Object, Object>> {
    private final TableColumn<Object, Object> tableColumn;

    public TableCellSkinWithBehavior(javafx.scene.control.TableCell<Object, Object> tableCell) {
        super(tableCell);
        this.tableColumn = tableCell.getTableColumn();
    }

    @Override
    public ReadOnlyObjectProperty<? extends TableColumnBase<Object, Object>> tableColumnProperty() {
        return new ReadOnlyObjectWrapper<>(tableColumn);
    }
}
