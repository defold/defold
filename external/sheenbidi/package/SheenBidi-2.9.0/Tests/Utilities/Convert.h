/*
 * Copyright (C) 2015-2025 Muhammad Tayyab Akram
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

#ifndef _SHEENBIDI__UTILITIES__CONVERT_H
#define _SHEENBIDI__UTILITIES__CONVERT_H

#include <cstdint>
#include <string>

#include <SheenBidi/SBBidiType.h>
#include <SheenBidi/SBGeneralCategory.h>
#include <SheenBidi/SBScript.h>

namespace SheenBidi {
namespace Utilities {

class Convert {
public:
    static const std::string &bidiTypeToString(SBBidiType bidiType);
    static const std::string &generalCategoryToString(SBGeneralCategory generalCategory);
    static const std::string &scriptToString(SBScript script);
    static uint32_t stringToTag(const std::string &str);
    static uint32_t toCodePoint(const std::string &bidiType);
    static void toLowercase(std::string &str);
};

}
}

#endif
