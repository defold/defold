package com.defold.control;

import com.sun.javafx.scene.control.inputmap.KeyBinding;
import javafx.scene.control.TabPane;
import javafx.scene.input.KeyEvent;

public class TabPaneBehavior extends com.sun.javafx.scene.control.behavior.TabPaneBehavior {
    public TabPaneBehavior(TabPane tabPane) {
        super(tabPane);
    }

    public static boolean isTraversalEvent(KeyEvent event) {

//        for (KeyBinding keyBinding : TAB_PANE_BINDINGS) {
//            if (keyBinding.getSpecificity(event) > 0) {
//                return true;
//            }
//        }

        return false;
    }
}
