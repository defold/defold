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

#include "font_render.h"

#include "glyph_vertex.h"

dmGraphics::HVertexDeclaration FontCreateGlyphVertexDeclaration(dmGraphics::HContext context)
{
    dmGraphics::HVertexStreamDeclaration streams = dmGraphics::NewVertexStreamDeclaration(context);
    dmGraphics::AddVertexStream(streams, "position", 3, dmGraphics::TYPE_FLOAT, false);
    dmGraphics::AddVertexStream(streams, "texcoord0", 2, dmGraphics::TYPE_FLOAT, false);
    dmGraphics::AddVertexStream(streams, "face_color", 4, dmGraphics::TYPE_UNSIGNED_BYTE, true);
    dmGraphics::AddVertexStream(streams, "outline_color", 4, dmGraphics::TYPE_UNSIGNED_BYTE, true);
    dmGraphics::AddVertexStream(streams, "shadow_color", 4, dmGraphics::TYPE_UNSIGNED_BYTE, true);
    dmGraphics::AddVertexStream(streams, "sdf_params", 4, dmGraphics::TYPE_FLOAT, false);
    dmGraphics::AddVertexStream(streams, "layer_mask", 3, dmGraphics::TYPE_FLOAT, false);

    dmGraphics::HVertexDeclaration declaration = dmGraphics::NewVertexDeclaration(context, streams, sizeof(FontGlyphVertex));
    dmGraphics::DeleteVertexStreamDeclaration(streams);
    return declaration;
}
