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

#ifndef SHEENBIDI_GENERATOR_UTILITIES_ARRAY_BUILDER_H
#define SHEENBIDI_GENERATOR_UTILITIES_ARRAY_BUILDER_H

#include <string>
#include <sstream>

#include "TextBuilder.h"

namespace SheenBidi {
namespace Generator {
namespace Utilities {

class ArrayBuilder : public TextBuilder {
public:
    ArrayBuilder();

    const std::string &dataType() const;
    void setDataType(const std::string &);

    const std::string &name() const;
    void setName(const std::string &);

    size_t size() const;
    void setSize(size_t);

    const std::string &sizeDescriptor() const;
    void setSizeDescriptor(const std::string &);

    size_t elementSpace() const;
    void setElementSpace(size_t);

    void appendElement(const std::string &);
    void newElement();

    void fillWithElement(const std::string &element);

protected:
    virtual void appendOnStream(std::ostream &) const;

private:
    std::string m_dataType;
    std::string m_name;
    std::string m_sizeDescriptor;

    size_t m_size;
    size_t m_elementSpace;

    size_t m_appendedElements;
    size_t m_lastElementLength;
};

}
}
}

#endif
