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

#include <cstdint>
#include <string>
#include <iomanip>

#include "ArrayBuilder.h"

using namespace std;
using namespace SheenBidi::Generator::Utilities;

static const string ELEMENT_SEPARATOR = ",";
static const size_t ELEMENT_SEPARATOR_LENGTH = ELEMENT_SEPARATOR.length();

ArrayBuilder::ArrayBuilder()
    : m_size(0)
    , m_elementSpace(0)
    , m_appendedElements(0)
    , m_lastElementLength(0)
{
}

void ArrayBuilder::appendOnStream(ostream &stream) const {
    stream << m_dataType << ' '
           << m_name << '[' << m_sizeDescriptor << ']'
           << " = {" << endl
           << m_stream.rdbuf() << endl
           << "};" << endl;
}

const string &ArrayBuilder::dataType() const {
    return m_dataType;
}

void ArrayBuilder::setDataType(const string &dataType) {
    m_dataType = dataType;
}

const string &ArrayBuilder::name() const {
    return m_name;
}

void ArrayBuilder::setName(const string &name) {
    m_name = name;
}

size_t ArrayBuilder::size() const {
    return m_size;
}

void ArrayBuilder::setSize(size_t size) {
    m_size = size;
}

const string &ArrayBuilder::sizeDescriptor() const {
    return m_sizeDescriptor;
}

void ArrayBuilder::setSizeDescriptor(const string &sizeDescriptor) {
    m_sizeDescriptor = sizeDescriptor;
}

size_t ArrayBuilder::elementSpace() const {
    return m_elementSpace;
}

void ArrayBuilder::setElementSpace(size_t elementSpace) {
    m_elementSpace = elementSpace;
}

void ArrayBuilder::appendElement(const string &element) {
    size_t spaces = m_elementSpace + ELEMENT_SEPARATOR_LENGTH;
    
    if (m_lastElementLength <= spaces) {
        spaces -= m_lastElementLength;
    } else {
        spaces = 0;
    }

    size_t newLength = lineLength() + spaces + element.length() + ELEMENT_SEPARATOR_LENGTH;
    if (newLength > LINE_LENGTH) {
        newLine();
    }

    if (lineStart() == m_stream.tellp()) {
        appendTab();
    } else {
        m_stream << setfill(' ') << setw(spaces) << ' ';
    }

    m_stream << element;

    m_lastElementLength = element.length();
    m_appendedElements++;
}

void ArrayBuilder::newElement() {
    m_stream << ',';
}

void ArrayBuilder::fillWithElement(const string& element) {
    size_t remaining = m_size - m_appendedElements;
    for (size_t i = 0; i < remaining; i++) {
        appendElement(element);
    }
}
