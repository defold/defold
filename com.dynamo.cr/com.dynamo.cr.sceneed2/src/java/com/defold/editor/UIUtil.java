package com.defold.editor;

import java.io.File;

import javafx.scene.control.TreeCell;
import javafx.scene.control.TreeView;
import javafx.util.Callback;

public class UIUtil {

    public static Callback<TreeView<File>, TreeCell<File>> newFileCellFactory() {

        return new Callback<TreeView<File>, TreeCell<File>>() {
            @Override
            public TreeCell<File> call(TreeView<File> v) {
                return new TreeCell<File>() {
                    @Override
                    protected void updateItem(File item, boolean empty) {
                        super.updateItem(item, empty);
                        if (!empty) {
                            if (item != null) {
                                String name = item.getName();
                                if (name != null) {
                                    setText(name);
                                }
                            }                        	
                        } else {
                        	setGraphic(null);
                        	setText(null);
                        }
                    }
                };
            }
        };
    }
}
