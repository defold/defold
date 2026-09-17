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

#include <iostream>
#include <sstream>
#include <string>

#include "UnicodeVersion.h"

using namespace std;
using namespace SheenBidi::Parser;

UnicodeVersion::UnicodeVersion(const string &versionLine) {
    stringstream stream(versionLine);
    stream.ignore(32, '-');
    stream >> m_major;
    stream.ignore(1, '.');
    stream >> m_minor;
    stream.ignore(1, '.');
    stream >> m_micro;

    stringstream versionStream;
    versionStream << m_major << '.' << m_minor << '.' << m_micro;
    m_versionString = versionStream.str();
}

int UnicodeVersion::major() const {
    return m_major;
}

int UnicodeVersion::minor() const {
    return m_minor;
}

int UnicodeVersion::micro() const {
    return m_micro;
}

const string &UnicodeVersion::versionString() const {
    return m_versionString;
}
