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

#include "ddf_util.h"

namespace dmDDF
{

    int ScalarTypeSize(uint32_t type)
    {
        switch(type)
        {
        case TYPE_BOOL:
            return sizeof(bool);

        case TYPE_INT32:
        case TYPE_FLOAT:
        case TYPE_FIXED32:
        case TYPE_UINT32:
        case TYPE_ENUM:
        case TYPE_SFIXED32:
        case TYPE_SINT32:
            return 4;

        case TYPE_SFIXED64:
        case TYPE_SINT64:
        case TYPE_DOUBLE:
        case TYPE_INT64:
        case TYPE_UINT64:
        case TYPE_FIXED64:
            return 8;

        /*
          TYPE_STRING
          TYPE_GROUP
          TYPE_MESSAGE
          TYPE_BYTES
          ...
        */
        default:
            assert(false && "Internal error");
            return -1;
        }
    }
}


