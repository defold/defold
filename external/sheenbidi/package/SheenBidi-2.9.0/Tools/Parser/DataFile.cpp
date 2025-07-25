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

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string>

#include "UnicodeVersion.h"
#include "DataFile.h"

using namespace std;
using namespace SheenBidi::Parser;

static const string SEMICOLON_OR_HASH = ";#";

static inline size_t findFirstOf(const string &characters, const string &text, size_t position) {
    auto limit = text.find_first_of(characters, position);
    if (limit == string::npos) {
        limit = text.length();
    }

    return limit;
}

static inline size_t findNonWhitespaceBefore(size_t position, const string &text, size_t start) {
    auto limit = text.find_last_not_of(" \t", position - 1);
    if (limit == string::npos) {
        limit = position;
    } else if (limit < start) {
        limit = start;
    } else {
        limit += 1;
    }

    return limit;
}

const string DataFile::Whitespace = " \t";
const string DataFile::Semicolon = ";";
const string DataFile::WhitespaceOrSemicolon = Whitespace + Semicolon;

DataFile::DataFile(const string &directory, const string &fileName) {
    auto filePath = directory + "/" + fileName;
    m_stream.open(filePath, ios::in);
    if (!m_stream.is_open()) {
        throw runtime_error("Failed to open file: " + filePath);
    }
}

DataFile::~DataFile() {
    if (m_stream.is_open()) {
        m_stream.close();
    }
}

bool DataFile::readLine(Line &line) {
    getline(m_stream, m_data);
    line = Line(m_data);

    return m_stream.good();
}

bool DataFile::reset() {
    if (m_stream.is_open()) {
        m_stream.clear();
        m_stream.seekg(0, ios::beg);
        return true;
    }

    return false;
}

UnicodeVersion DataFile::Line::scanVersion() {
    return UnicodeVersion(m_data);
}

void DataFile::Line::skip(SkipMode skipMode) {
    switch (skipMode) {
        case SkipMode::Whitespaces: {
            auto next = m_data.find_first_not_of(Whitespace, m_position);
            m_position = (next == string::npos ? m_data.length() : next);
            break;
        }

        case SkipMode::Field: {
            auto limit = m_data.find_first_of(SEMICOLON_OR_HASH, m_position);
            m_position = (limit == string::npos ? m_data.length() : limit + 1);
            break;
        }
    }
}

bool DataFile::Line::match(char character) {
    if (m_data.at(m_position) == character) {
        m_position += 1;
        return true;
    }

    return false;
}

bool DataFile::Line::match(const string &matchString, MatchRule matchRule) {
    switch (matchRule) {
        case MatchRule::Strict: {
            if (m_data.compare(m_position, matchString.size(), matchString) == 0) {
                m_position += matchString.size();
                return true;
            }
            break;
        }

        case MatchRule::Trimmed: {
            skip(SkipMode::Whitespaces);
            if (m_data.compare(m_position, matchString.size(), matchString) == 0) {
                m_position += matchString.size();
                skip(SkipMode::Whitespaces);
                return true;
            }
            break;
        }
    }

    return false;
}

bool DataFile::Line::getUntil(const string &separators, std::string &text) {
    if (m_position >= m_data.size()) {
        return false;
    }

    skip(SkipMode::Whitespaces);

    auto end = findFirstOf(separators, m_data, m_position);
    auto limit = findNonWhitespaceBefore(end, m_data, m_position);

    text.assign(m_data, m_position, limit - m_position);
    m_position = end;

    skip(SkipMode::Whitespaces);

    return true;
}

bool DataFile::Line::getField(string &field) {
    if (getUntil(SEMICOLON_OR_HASH, field)) {
        skip(SkipMode::Field);
        return true;
    }

    return false;
}

uint32_t DataFile::Line::parseNumber(int base) {
    if (m_position >= m_data.size()) {
        throw out_of_range("index");
    }

    auto start = m_data.c_str() + m_position;
    char *end = nullptr;

    auto number = static_cast<size_t>(strtoul(start, &end, base));
    auto length = static_cast<size_t>(end - start);
    m_position += length;

    skip(SkipMode::Whitespaces);

    return number;
}

uint32_t DataFile::Line::parseSingleCodePoint() {
    uint32_t codePoint = parseNumber(16);
    skip(SkipMode::Field);

    return codePoint;
}

DataFile::CodePointRange DataFile::Line::parseCodePointRange() {
    uint32_t first = parseNumber(16);
    uint32_t last = first;

    if (m_position < m_data.length() && m_data[m_position] == '.') {
        m_position = m_data.find_first_not_of('.', m_position);
        last = parseNumber(16);
    }

    skip(SkipMode::Field);

    return {first, last};
}
