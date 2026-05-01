/*
 * Copyright (C) 2016 Muhammad Tayyab Akram
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

#include <cstdint>
#include <iomanip>
#include <ostream>
#include <string>

#include "StreamBuilder.h"

using namespace std;
using namespace SheenBidi::Generator::Utilities;

const size_t StreamBuilder::TAB_LENGTH = 4;
const size_t StreamBuilder::LINE_LENGTH = 100;

StreamBuilder::StreamBuilder(ostream *stream)
    : m_lineStart(0)
{
    m_stream = stream;
}

StreamBuilder &StreamBuilder::append(const string &str) {
    *m_stream << str;
    return *this;
}

StreamBuilder &StreamBuilder::append(const StreamBuilder &builder) {
    builder.appendOnStream(*m_stream);
    return *this;
}

StreamBuilder &StreamBuilder::newLine() {
    *m_stream << endl;
    m_lineStart = (size_t)m_stream->tellp();
    return *this;
}

StreamBuilder &StreamBuilder::appendTab() {
    appendTabs(1);
    return *this;
}

StreamBuilder &StreamBuilder::appendTabs(size_t tabCount) {
    size_t tabLength = tabCount * TAB_LENGTH;
    size_t oldLength = lineLength();
    size_t newLength = oldLength + tabLength;
    size_t spaces = tabLength - (newLength % tabCount);
    *m_stream << setfill(' ') << setw(spaces) << ' ';
    return *this;
}

size_t StreamBuilder::lineStart() {
    return m_lineStart;
}

size_t StreamBuilder::lineLength() {
    return ((size_t)m_stream->tellp() - m_lineStart);
}
