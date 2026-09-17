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

#ifndef DM_VMATH_H
#define DM_VMATH_H

#include <dmsdk/dlib/vmath.h>
#include "math.h"

#include <assert.h>

/**
 * Collection of vector math functions.
 */
namespace dmVMath
{

    struct FloatVector
    {
        int size;
        float* values;

        FloatVector() { size = 0; values = NULL; };
        FloatVector(int new_size) {
            assert(new_size >= 0);
            size = new_size;
            if (new_size > 0)
                values = (float*)malloc(new_size * sizeof(float));
            else
                values = NULL;
        };

        ~FloatVector() {
            if (size > 0 && values)
            {
                free(values);
                values = NULL;
            }
        }
    };

}

#endif
