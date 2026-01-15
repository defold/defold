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

import clojure.lang.*;

import java.util.ArrayList;
import javafx.collections.ObservableList;
import javafx.scene.control.TreeItem;

/// As documented in JavaFX javadocs, it's possible to build TreeItems on-demand
/// in a memory-efficient way by overriding isLeaf and getChildren
///
/// See [TreeItem](https://openjfx.io/javadoc/25/javafx.controls/javafx/scene/control/TreeItem.html)
/// documentation.
public class LazyTreeItem extends TreeItem<Object> {

    // invoked at most once, becomes null when getChildren is first invoked
    private IFn getChildrenIReduceInit;
    // reduced at most twice:
    // - if isLeaf is invoked first, reduced to check if not empty
    // - when getChildren is invoked, reduced to compute children
    // may be null, since null is a valid (empty) reducible in Clojure
    private IReduceInit childrenIReduceInit;
    // since `childrenIReduceInit` may be null, we use this flag to know when it
    // is the expected value
    private boolean getChildrenIReduceInitInvoked;
    private Boolean computedIsLeaf;

    private static final IFn HAS_ANY_CHILD = new AFn() {
        @Override
        public Object invoke(Object acc, Object child) {
            return new Reduced(Boolean.TRUE);
        }
    };

    /**
     * Create a lazily-constructed TreeItem
     * @param value TreeItem's value
     * @param getChildrenIReduceInit a function of value that will be invoked at
     *                               most once per LazyTreeItem, should return
     *                               an IReduceInit (or nil); this IReduceInit
     *                               will be reduced at most twice: first, to
     *                               check if the node is a leaf, then, to build
     *                               an array of children; the same function is
     *                               then will be used on those children to
     *                               continue building the tree
     */
    public LazyTreeItem(Object value, IFn getChildrenIReduceInit) {
        super(value);
        this.getChildrenIReduceInit = getChildrenIReduceInit;
    }

    @Override
    public boolean isLeaf() {
        var f = getChildrenIReduceInit;
        // f becomes null when children are computed, at that point, isLeaf is
        // properly set in the super class
        if (f == null) {
            return super.isLeaf();
        } else {
            var b = computedIsLeaf;
            if (b == null) {
                var r = childrenIReduceInit;
                if (!getChildrenIReduceInitInvoked) {
                    r = ((IReduceInit) f.invoke(getValue()));
                    getChildrenIReduceInitInvoked = true;
                    childrenIReduceInit = r;
                }
                b = r == null || !RT.booleanCast(r.reduce(HAS_ANY_CHILD, Boolean.FALSE));
                computedIsLeaf = b;
            }
            return b;
        }
    }

    @Override
    public ObservableList<TreeItem<Object>> getChildren() {
        var f = getChildrenIReduceInit;
        var children = super.getChildren();
        if (f != null) {
            var r = childrenIReduceInit;
            if (!getChildrenIReduceInitInvoked) {
                r = ((IReduceInit) f.invoke(getValue()));
                getChildrenIReduceInitInvoked = true;
            }
            if (r != null) {
                var items = new ArrayList<LazyTreeItem>();
                r.reduce(new AFn() {
                    @Override
                    public Object invoke(Object _acc, Object child) {
                        items.add(new LazyTreeItem(child, f));
                        return null;
                    }
                }, null);
                children.setAll(items);
            }
            getChildrenIReduceInit = null;
            childrenIReduceInit = null;
        }
        return children;
    }
}
