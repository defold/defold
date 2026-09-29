# Copyright 2020-2026 The Defold Foundation
# Copyright 2014-2020 King
# Copyright 2009-2014 Ragnar Svensson, Christian Murray
# Licensed under the Defold License version 1.0 (the "License"); you may not use
# this file except in compliance with the License.
#
# You may obtain a copy of the License, together with FAQs at
# https://www.defold.com/license
#
# Unless required by applicable law or agreed to in writing, software distributed
# under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied. See the License for the
# specific language governing permissions and limitations under the License.

# Host-side test dependencies; this check never creates a graphics context.
_defold_find_python(_likeness_python)
message(STATUS "Image-test Python: ${_likeness_python}")
execute_process(
  COMMAND "${_likeness_python}" "${DEFOLD_HOME}/scripts/likeness.py" --check
  RESULT_VARIABLE _likeness_result
  OUTPUT_VARIABLE _likeness_output
  ERROR_VARIABLE _likeness_error
  OUTPUT_STRIP_TRAILING_WHITESPACE)
message(STATUS "${_likeness_output}")
if(NOT _likeness_result EQUAL 0)
  message(FATAL_ERROR "Image comparison prerequisites failed: ${_likeness_error}")
endif()
