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

# A failed assertion must not suppress other configurations or their report.
set(images "${FONT_ROOT}/build/font-test-images")
set(report "${FONT_ROOT}/build/font-render-report")
file(REMOVE_RECURSE "${images}" "${report}")
file(MAKE_DIRECTORY "${images}")
set(report_args)
foreach(configuration IN ITEMS legacy-rich full-rich legacy-plain full-plain)
  string(TOUPPER "${configuration}" executable_key)
  string(REPLACE "-" "_" executable_key "${executable_key}")
  execute_process(COMMAND "${${executable_key}}" --output "${images}"
    WORKING_DIRECTORY "${FONT_ROOT}" RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 120)
  file(WRITE "${images}/${configuration}.log" "${output}${error}")
  file(WRITE "${images}/${configuration}.exit-code" "${result}")
  list(APPEND report_args --executable "${configuration}=${${executable_key}}")
endforeach()
execute_process(COMMAND "${PYTHON}" "${FONT_ROOT}/src/test/make_report.py"
    --images "${images}" --output "${report}" --generation-results ${report_args}
  WORKING_DIRECTORY "${FONT_ROOT}" RESULT_VARIABLE result)
if(NOT "${result}" STREQUAL "0")
  message(FATAL_ERROR "Font image validation failed; see the capture/comparison breakdown above. Inspect ${report}/index.html")
endif()
