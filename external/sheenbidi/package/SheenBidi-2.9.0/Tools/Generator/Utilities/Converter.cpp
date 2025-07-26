/*
 * Copyright (C) 2018 Muhammad Tayyab Akram
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
#include <sstream>
#include <string>

#include "Converter.h"

using namespace std;
using namespace SheenBidi::Generator::Utilities;

string Converter::toHex(size_t number, size_t length) {
    stringstream stream;
    stream << hex << uppercase << setfill('0') << setw(length) << number;

    return stream.str();
}

string Converter::toString(int number) {
    stringstream stream;
    stream << number;

    return stream.str();
}

void Converter::toUpper(string &str) {
    auto begin = str.begin();
    auto end = str.end();

    for (; begin < end; begin++) {
        *begin = toupper(*begin);
    }
}
