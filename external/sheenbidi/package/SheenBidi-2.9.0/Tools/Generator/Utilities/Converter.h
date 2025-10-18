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

#ifndef SHEENBIDI_GENERATOR_UTILITIES_CONVERTER_H
#define SHEENBIDI_GENERATOR_UTILITIES_CONVERTER_H

#include <cstdint>
#include <string>

namespace SheenBidi {
namespace Generator {
namespace Utilities {

class Converter {
public:
    static std::string toHex(size_t number, size_t length = SIZE_MAX);
    static std::string toString(int number);
    static void toUpper(std::string &str);
};

}
}
}

#endif
