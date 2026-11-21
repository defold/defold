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

import javafx.geometry.Insets;
import javafx.geometry.Orientation;
import javafx.scene.Node;
import javafx.scene.control.ScrollBar;
import javafx.scene.control.ScrollPane;
import javafx.scene.control.skin.ScrollPaneSkin;

/**
 * A ScrollPane whose preferred height already includes the horizontal
 * scrollbar when the content is wider than the available width.
 *
 * ScrollPane only decides to show its horizontal scrollbar during layout,
 * after its preferred height has been measured. When the scrollbar then
 * appears, the pane grows, and the layouts around it resize and flicker.
 * Comparing the content's preferred width with the available width while
 * measuring lets the height account for the scrollbar up front.
 *
 * This assumes the content does not wrap, so its preferred width is its full
 * width, and that the vertical scrollbar is never shown. Before the default
 * skin is created, no scrollbar space is reserved.
 */
public final class CodeScrollPane extends ScrollPane {
    @Override
    public Orientation getContentBias() {
        return Orientation.HORIZONTAL;
    }

    @Override
    protected double computePrefHeight(double width) {
        Node content = getContent();
        if (content == null) {
            return 0.0;
        }
        Insets insets = getInsets();
        double availableWidth = width - insets.getLeft() - insets.getRight();
        double height = insets.getTop() + content.prefHeight(-1.0) + insets.getBottom();
        ScrollBar hbar = getSkin() instanceof ScrollPaneSkin skin ? skin.getHorizontalScrollBar() : null;
        if (hbar != null && availableWidth > 0.0 && content.prefWidth(-1.0) > availableWidth) {
            height += hbar.prefHeight(-1.0);
        }
        return height;
    }
}
