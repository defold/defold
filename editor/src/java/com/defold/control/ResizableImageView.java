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

import javafx.geometry.Orientation;
import javafx.scene.image.Image;
import javafx.scene.image.ImageView;

public class ResizableImageView extends ImageView {
    public ResizableImageView() {
        setPreserveRatio(true);
    }

    private double aspectRatio() {
        Image image = getImage();
        if (image == null) {
            return 1.0;
        } else {
            double height = image.getHeight();
            if (height == 0.0) {
                return 1.0;
            } else {
                return image.getWidth() / height;
            }
        }
    }

    @Override
    public double minWidth(double v) {
        return 0.0;
    }

    @Override
    public double minHeight(double v) {
        return 0.0;
    }

    @Override
    public double prefWidth(double height) {
        if (height > 0.0) {
            return height * aspectRatio();
        } else {
            Image image = getImage();
            return image == null ? 0.0 : image.getWidth();
        }
    }

    @Override
    public double prefHeight(double width) {
        if (width > 0.0) {
            return width / aspectRatio();
        } else {
            Image image = getImage();
            return image == null ? 0.0 : image.getHeight();
        }
    }

    @Override
    public boolean isResizable() {
        return true;
    }

    @Override
    public void resize(double width, double height) {
        setFitWidth(width);
        setFitHeight(height);
    }

    @Override
    public Orientation getContentBias() {
        return Orientation.HORIZONTAL;
    }
}
