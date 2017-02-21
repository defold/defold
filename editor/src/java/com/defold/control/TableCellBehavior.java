package com.defold.control;

import javafx.scene.input.MouseButton;

// Part of workaround for https://bugs.openjdk.java.net/browse/JDK-8089514
// Makes handleClicks public to avoid having to call it using reflection.
public class TableCellBehavior extends com.sun.javafx.scene.control.behavior.TableCellBehavior<Object, Object> {
    public TableCellBehavior(javafx.scene.control.TableCell<Object, Object> control) {
        super(control);
    }

    @Override
    public void handleClicks(MouseButton button, int clickCount, boolean isAlreadySelected) {
        super.handleClicks(button, clickCount, isAlreadySelected);
    }
}
