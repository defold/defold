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

#ifndef _SHEENBIDI__SCRIPT_LOOKUP_TESTS_H
#define _SHEENBIDI__SCRIPT_LOOKUP_TESTS_H

#include <Parser/PropertyValueAliases.h>
#include <Parser/Scripts.h>

namespace SheenBidi {

class ScriptLookupTests {
public:
    ScriptLookupTests(const Parser::Scripts &scripts, const Parser::PropertyValueAliases &propertyValueAliases);

    void run();

private:
    const Parser::Scripts &m_scripts;
    const Parser::PropertyValueAliases &m_propertyValueAliases;
};

}

#endif
