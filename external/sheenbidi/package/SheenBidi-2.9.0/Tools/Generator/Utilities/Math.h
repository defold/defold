/*
 * Copyright (C) 2018 Muhammad Tayyab Akram
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SHEENBIDI_GENERATOR_UTILITIES_MATH_H
#define SHEENBIDI_GENERATOR_UTILITIES_MATH_H

#include <cstddef>

namespace SheenBidi {
namespace Generator {
namespace Utilities {

class Math {
public:
    static int FastFloor(size_t x, size_t y);
    static int FastCeil(size_t x, size_t y);
};

}
}
}

#endif
