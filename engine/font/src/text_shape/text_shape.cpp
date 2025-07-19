// Copyright 2020-2025 The Defold Foundation
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

#include <font/text_shape/text_shape.h>


TextShapeResult TextGetMetrics(HFont font,
                                uint32_t* codepoints, uint32_t num_codepoints,
                                TextMetricsSettings* settings, TextMetrics* metrics)
{
    TextShapeInfo info;

    TextShapeResult r = TextShapeText(font, codepoints, num_codepoints, &info);
    if (TEXT_SHAPE_RESULT_OK != r)
        return r;

    return TextLayout(settings, &info, 0, 0, metrics);
}
