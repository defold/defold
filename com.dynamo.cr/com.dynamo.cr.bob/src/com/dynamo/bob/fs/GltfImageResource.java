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

package com.dynamo.bob.fs;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Iterator;
import java.util.List;

import javax.imageio.ImageIO;
import javax.imageio.ImageReader;
import javax.imageio.stream.ImageInputStream;

/** A virtual image resource backed by the exact encoded glTF image bytes. */
public class GltfImageResource extends GltfResource {

    private final String uri;
    private final String mimeType;
    private final String sourceKind;
    private final List<GltfContainer.TextureMetadata> textures;
    private boolean dimensionsRead;
    private int width = -1;
    private int height = -1;

    GltfImageResource(IFileSystem fileSystem, IResource sourceResource, String path, int index, String name,
                      String uri, String mimeType, String sourceKind, byte[] content,
                      List<GltfContainer.TextureMetadata> textures) {
        super(fileSystem, sourceResource, path, Kind.IMAGE, index, name, content);
        this.uri = uri;
        this.mimeType = mimeType;
        this.sourceKind = sourceKind;
        this.textures = Collections.unmodifiableList(
                new ArrayList<GltfContainer.TextureMetadata>(textures));
    }

    public String getUri() {
        return uri;
    }

    public String getMimeType() {
        return mimeType;
    }

    public String getSourceKind() {
        return sourceKind;
    }

    /** Textures selecting this image; each extracted texture belongs to one image. */
    public List<GltfContainer.TextureMetadata> getTextures() {
        return textures;
    }

    public int getWidth() {
        readDimensions();
        return width;
    }

    public int getHeight() {
        readDimensions();
        return height;
    }

    private synchronized void readDimensions() {
        if (dimensionsRead) {
            return;
        }
        dimensionsRead = true;
        ImageReader reader = null;
        try (ImageInputStream input = ImageIO.createImageInputStream(new ByteArrayInputStream(getContent()))) {
            if (input == null) {
                return;
            }

            Iterator<ImageReader> readers = ImageIO.getImageReaders(input);
            if (!readers.hasNext()) {
                return;
            }

            reader = readers.next();
            reader.setInput(input, true, true);
            width = reader.getWidth(0);
            height = reader.getHeight(0);
        } catch (IOException | RuntimeException ignored) {
            width = -1;
            height = -1;
        } finally {
            if (reader != null) {
                reader.dispose();
            }
        }
    }
}
