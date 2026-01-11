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
