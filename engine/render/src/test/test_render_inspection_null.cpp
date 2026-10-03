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

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include "render/render_private.h"

// The release implementation never enables inspection or allocates state, even when explicitly requested.
TEST(dmRenderInspectionNull, Disabled)
{
    dmRender::RenderContext context;
    context.m_Inspection = dmRender::NewInspectionContext();
    ASSERT_EQ((dmRender::InspectionContext*)0, context.m_Inspection);
    dmRender::EnableInspection(&context, true);
    ASSERT_FALSE(dmRender::IsInspectionEnabled(&context));
    dmRender::SetInspectionRenderTarget(&context, 1);
    dmRender::RecordMaterialInspectionProjection(&context, 1);
    dmRender::RecordInspectionProjection(&context, &context);

    dmRender::InspectionProjection projection;
    memset(&projection, 0xff, sizeof(projection));
    dmRender::GetInspectionProjection(&context, 1, &projection);
    ASSERT_EQ(0, projection.m_State);
    ASSERT_EQ(0u, projection.m_ViewportWidth);
    memset(&projection, 0xff, sizeof(projection));
    dmRender::GetComponentInspectionProjection(&context, &context, &projection);
    ASSERT_EQ(0, projection.m_State);
    ASSERT_EQ(0u, projection.m_ViewportWidth);
    dmRender::BeginInspectionFrame(&context);
    dmRender::EnableInspection(&context, false);
    ASSERT_EQ((dmRender::InspectionContext*)0, context.m_Inspection);
    dmRender::DeleteInspectionContext(context.m_Inspection);
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
