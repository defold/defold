// Copyright 2020-2025 The Defold Foundation
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
