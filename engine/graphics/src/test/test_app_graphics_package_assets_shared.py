HEADER_TEMPLATE = """// Copyright 2020-2026 The Defold Foundation
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

#ifndef DM_TEST_APP_%s
#define DM_TEST_APP_%s
namespace graphics_assets
{
%s
}
#endif
"""

def get_file_contents(file_path):
    f = open(file_path, "rb")
    src = f.read()
    buf = []
    for s in src:
        buf.append(hex(s))
    return buf
