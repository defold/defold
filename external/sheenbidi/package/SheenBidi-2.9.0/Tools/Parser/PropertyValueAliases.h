/*
 * Copyright (C) 2018-2025 Muhammad Tayyab Akram
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

#ifndef SHEENBIDI_PARSER_PROPERTY_VALUE_ALIASES_H
#define SHEENBIDI_PARSER_PROPERTY_VALUE_ALIASES_H

#include <string>
#include <map>

#include "DataFile.h"
#include "UnicodeVersion.h"

namespace SheenBidi {
namespace Parser {

class PropertyValueAliases : public DataFile {
public:
    PropertyValueAliases(const std::string &directory);

    const UnicodeVersion &version() const { return m_version; }

    const std::string &abbreviationForBidiClass(const std::string &bidiClass) const;
    const std::string &abbreviationForScript(const std::string &scriptName) const;

private:
    UnicodeVersion m_version;

    std::map<std::string, std::string> m_bidiClassMap;
    std::map<std::string, std::string> m_scriptMap;
};

}
}

#endif
