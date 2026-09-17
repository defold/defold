/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef VKQUALITY_GLES_UTIL_H_
#define VKQUALITY_GLES_UTIL_H_

#include "vkquality_manager.h"

namespace vkquality {

  class GLESUtil {
  public:
    static void GetGLESStrings(std::string& renderer, std::string& version, std::string& vendor);
  };

} // namespace vkquality

#endif // VKQUALITY_GLES_UTIL_H_
