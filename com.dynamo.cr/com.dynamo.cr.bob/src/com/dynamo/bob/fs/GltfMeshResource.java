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

/** A read-only virtual resource representing one glTF mesh. */
public class GltfMeshResource extends GltfResource {

    private final boolean nameGenerated;
    private final int primitiveCount;
    private final int vertexCount;

    GltfMeshResource(IFileSystem fileSystem, IResource sourceResource,
                     GltfContainer.MeshMetadata mesh) {
        super(fileSystem, sourceResource, mesh.resourcePath().substring(1), Kind.MESH,
                mesh.index(), mesh.name(), mesh.content().toByteArray());
        this.nameGenerated = mesh.nameGenerated();
        this.primitiveCount = mesh.primitiveCount();
        this.vertexCount = mesh.vertexCount();
    }

    public boolean isNameGenerated() {
        return nameGenerated;
    }

    public int getPrimitiveCount() {
        return primitiveCount;
    }

    public int getVertexCount() {
        return vertexCount;
    }
}
