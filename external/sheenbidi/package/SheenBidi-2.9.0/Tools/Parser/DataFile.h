/*
 * Copyright (C) 2025 Muhammad Tayyab Akram
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

#ifndef SHEENBIDI_PARSER_DATA_FILE_H
#define SHEENBIDI_PARSER_DATA_FILE_H

#include <fstream>
#include <cstdint>
#include <string>
#include <utility>

#include "UnicodeVersion.h"

namespace SheenBidi {
namespace Parser {

class DataFile {
protected:
    static constexpr uint32_t CodePointCount = 0x110000;

    static const std::string Whitespace;
    static const std::string Semicolon;
    static const std::string WhitespaceOrSemicolon;

    enum class SkipMode {
        Whitespaces,    // Skip whitespaces
        Field           // Skip field ending at semicolon
    };

    enum class MatchRule {
        Strict,         // Match exactly at the current index
        Trimmed         // Skip leading spaces before match and trailing spaces after match
    };

    using CodePointRange = std::pair<uint32_t, uint32_t>;

    class Line {
    public:
        Line() = default;
        Line(const std::string &data) : m_data(data) { }

        UnicodeVersion scanVersion();

        void skip(SkipMode skipMode);
        bool match(char character);
        bool match(const std::string &matchString, MatchRule matchRule = MatchRule::Strict);

        bool getUntil(const std::string &separators, std::string &text);
        bool getField(std::string &field);

        uint32_t parseNumber(int base = 10);
        uint32_t parseSingleCodePoint();
        CodePointRange parseCodePointRange();

        std::string data() const { return m_data; }
        bool isEmpty() const { return m_data.empty(); }
        size_t position() const { return m_position; }
        bool hasMore() const { return m_position < m_data.length(); }

    private:
        std::string m_data;
        size_t m_position = 0;
    };

    DataFile(const std::string &directory, const std::string &fileName);
    virtual ~DataFile();

    bool readLine(Line &line);
    bool reset();

private:
    std::ifstream m_stream;
    std::string m_data;
};

}
}

#endif
