package com.defold.control;

import javafx.scene.control.TreeCell;
import javafx.scene.control.TreeView;
import javafx.scene.control.skin.TreeViewSkin;
import javafx.scene.control.skin.VirtualFlow;

public class OutlineTreeViewSkin<T> extends TreeViewSkin<T> {
    public OutlineTreeViewSkin(TreeView treeView) {
        super(treeView);
    }

    public boolean shouldScrollTo(int index) {
        VirtualFlow<TreeCell<T>> flow = getVirtualFlow();
        TreeCell<T> firstVisibleCell = flow.getFirstVisibleCell();
        if (firstVisibleCell == null) {
            return false;
        }
        TreeCell<T> lastVisibleCell = flow.getLastVisibleCell();
        if (lastVisibleCell == null) {
            return false;
        }
        return firstVisibleCell.getIndex() > index || lastVisibleCell.getIndex() < index;
    }
}
