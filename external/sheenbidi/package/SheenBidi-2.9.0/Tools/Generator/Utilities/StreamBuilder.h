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

#ifndef SHEENBIDI_GENERATOR_UTILITIES_STREAM_BUILDER_H
#define SHEENBIDI_GENERATOR_UTILITIES_STREAM_BUILDER_H

#include <cstdint>
#include <ostream>
#include <string>

namespace SheenBidi {
namespace Generator {
namespace Utilities {

class StreamBuilder {
public:
    StreamBuilder &append(const std::string &);
    StreamBuilder &append(const StreamBuilder &);

    StreamBuilder &appendTab();
    StreamBuilder &appendTabs(size_t);

    StreamBuilder &newLine();

protected:
    static const size_t TAB_LENGTH;
    static const size_t LINE_LENGTH;

    StreamBuilder(std::ostream *);

    size_t lineStart();
    size_t lineLength();

    virtual void appendOnStream(std::ostream &) const = 0;

private:
    std::ostream *m_stream;
    size_t m_lineStart;
};

}
}
}

#endif
