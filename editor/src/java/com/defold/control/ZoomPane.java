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

import javafx.geometry.HPos;
import javafx.geometry.Orientation;
import javafx.geometry.VPos;
import javafx.scene.Node;
import javafx.scene.Parent;
import javafx.scene.layout.StackPane;
import javafx.scene.transform.Scale;

/**
 * Implements application zoom by wrapping a window's existing UI. Content is
 * laid out at the window size divided by the zoom factor, then scaled to fill
 * the window. Minimum and preferred sizes are scaled as well so window sizing
 * continues to work.
 *
 * <p>For example, a 900-pixel-wide window at 150% zoom lays out its content at
 * 600 logical pixels, then scales it by 1.5. JavaFX's transform handling keeps
 * mouse coordinates consistent with the content as the window is resized.</p>
 *
 * <p>This class handles layout and scaling. The Clojure code reads the zoom
 * setting and installs the container on editor windows.</p>
 */
public final class ZoomPane extends StackPane {
    private final double zoom;
    private Parent content;

    public ZoomPane(double zoom) {
        if (!Double.isFinite(zoom) || zoom <= 0.0) {
            throw new IllegalArgumentException("UI scale must be finite and positive");
        }
        this.zoom = zoom;
    }

    public double getZoom() {
        return zoom;
    }

    /** Attach the former scene root after it has been detached from the scene. */
    public void setContent(Parent content) {
        this.content = content;
        // Anchor zoom at the origin so resizing never shifts the content's top-left.
        content.getTransforms().add(new Scale(zoom, zoom, 0.0, 0.0));
        // FXML roots can own the stylesheet that defines the .root font and colors.
        getStylesheets().setAll(content.getStylesheets());
        getChildren().setAll(content);
    }

    private double unscale(double size) {
        // JavaFX uses -1 for an unspecified constraint in min/pref size queries.
        return size < 0.0 ? size : size / zoom;
    }

    @Override
    public Orientation getContentBias() {
        // Preserve width-dependent height calculations, such as wrapped text.
        return content == null ? null : content.getContentBias();
    }

    // Sizing queries use zoomed coordinates. Ask content with an unscaled
    // constraint, then convert its answer back to the window's coordinates.
    @Override
    protected double computeMinWidth(double height) {
        return content == null ? 0.0 : content.minWidth(unscale(height)) * zoom;
    }

    @Override
    protected double computeMinHeight(double width) {
        return content == null ? 0.0 : content.minHeight(unscale(width)) * zoom;
    }

    @Override
    protected double computePrefWidth(double height) {
        return content == null ? 0.0 : content.prefWidth(unscale(height)) * zoom;
    }

    @Override
    protected double computePrefHeight(double width) {
        return content == null ? 0.0 : content.prefHeight(unscale(width)) * zoom;
    }

    @Override
    protected void layoutChildren() {
        // The color dropper expects a StackPane root and adds an overlay directly
        // to it. Keep such overlays in scene coordinates; only zoom the content.
        for (Node child : getManagedChildren()) {
            if (child == content) {
                content.resizeRelocate(0.0, 0.0, getWidth() / zoom, getHeight() / zoom);
            } else {
                layoutInArea(child, 0.0, 0.0, getWidth(), getHeight(), 0.0, HPos.CENTER, VPos.CENTER);
            }
        }
    }
}
