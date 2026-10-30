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

#ifndef DM_MODELIMPORTER_TANGENTS_H
#define DM_MODELIMPORTER_TANGENTS_H

namespace dmModelImporter
{
    struct Mesh;

    // Private modelc adapter. Generates tangents from vertex-count float2 UVs.
    // Splits vertices at tangent seams, copying every attribute and morph stream.
    // The mesh owns the result; texcoords are borrowed only during this call.
    // Requires validated vertex streams and unit normals. Returns false when a
    // valid tangent stream cannot be produced within the importer storage limit.
    bool GenerateTangents(Mesh* mesh, const float* texcoords);
} // namespace dmModelImporter

#endif
