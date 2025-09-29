package com.defold.control;

import clojure.lang.IFn;
import javafx.scene.control.ListCell;

public class UpdatableListCell extends ListCell<Object> {

    private final IFn update;
    private final IFn clear;

    public UpdatableListCell(IFn update) {
        this.update = update;
        this.clear = null;
    }

    public UpdatableListCell(IFn update, IFn clear) {
        this.update = update;
        this.clear = clear;
    }

    @Override
    protected void updateItem(Object item, boolean empty) {
        super.updateItem(item, empty);
        if (item == null || empty) {
            if (clear != null) {
                clear.invoke(this);
            }
        } else {
            update.invoke(this, item);
        }
    }
}
