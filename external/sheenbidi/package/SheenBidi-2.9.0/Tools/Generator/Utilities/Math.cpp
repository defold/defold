/*
 * Copyright (C) 2015 Muhammad Tayyab Akram
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

#include "Math.h"

using namespace std;
using namespace SheenBidi::Generator::Utilities;

int Math::FastFloor(size_t x, size_t y) {
    return ((x % y) ? x / y : x / y - 1);
}

int Math::FastCeil(size_t x, size_t y) {
    return ((x % y) ? x / y + 1 : x / y);
}
