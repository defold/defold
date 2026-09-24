// Copyright 2026 The Defold Foundation
// Licensed under the Defold License version 1.0. See https://www.defold.com/license

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
}

#endif
