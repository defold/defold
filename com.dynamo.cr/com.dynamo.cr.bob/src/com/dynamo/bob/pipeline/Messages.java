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

package com.dynamo.bob.pipeline;

import com.dynamo.bob.util.BobNLS;

public class Messages extends BobNLS {
    private static final String BUNDLE_NAME = "com.dynamo.bob.pipeline.messages"; //$NON-NLS-1$

    public static String BuilderUtil_EMPTY_RESOURCE;
    public static String BuilderUtil_MISSING_RESOURCE;
    public static String BuilderUtil_DUPLICATE_RESOURCE;
    public static String BuilderUtil_WRONG_RESOURCE_TYPE;

    public static String GuiBuilder_MISSING_TEXTURE;
    public static String GuiBuilder_MISSING_MATERIAL;
    public static String GuiBuilder_MISSING_FONT;
    public static String GuiBuilder_MISSING_PARTICLEFX;
    public static String GuiBuilder_MISSING_LAYER;

    public static String GuiBuilder_DUPLICATED_TEXTURE;
    public static String GuiBuilder_DUPLICATED_MATERIAL;
    public static String GuiBuilder_DUPLICATED_FONT;
    public static String GuiBuilder_DUPLICATED_PARTICLEFX;
    public static String GuiBuilder_DUPLICATED_LAYER;

    public static String TileSetBuilder_MISSING_IMAGE_AND_COLLISION;

    public static String CollisionObjectBuilder_MISMATCHING_SHAPE_PHYSICS_TYPE;

    static {
        // initialize resource bundle
        BobNLS.initializeMessages(BUNDLE_NAME, Messages.class);
    }

    private Messages() {
    }
}
