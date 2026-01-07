;; Copyright 2020-2026 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;;
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;;
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns integration.extension-lua-preprocessor-test
  (:require [clojure.test :refer :all]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.settings-core :as settings-core]
            [integration.test-util :as tu]
            [support.test-support :refer [with-clean-system]]
            [util.murmur :as murmur])
  (:import [com.dynamo.lua.proto Lua$LuaModule]))

(set! *warn-on-reflection* true)

(defonce ^:private extension-lua-preprocessor-url (settings-core/inject-jvm-properties "{{defold.extension.lua-preprocessor.url}}"))

(deftest extension-lua-preprocessor-test
  (with-clean-system
    (let [workspace (tu/setup-scratch-workspace! world "test/resources/empty_project")
          project (tu/setup-project! workspace)
          build-resource (partial tu/build-resource project)
          build-resource-path (comp resource/proj-path build-resource)
          build-resource-path-hash (comp murmur/hash64 build-resource-path)
          texture-build-resource (partial tu/texture-build-resource project)]
      (tu/make-code-resource-node! project "/system.lua" "return {}")
      (tu/make-code-resource-node! project "/debug-utils.lua" "return {}")
      (tu/make-code-resource-node! project "/release-utils.lua" "return {}")
      (tu/make-atlas-resource-node! project "/from-script.atlas")
      (tu/make-atlas-resource-node! project "/from-script-debug-variant.atlas")
      (tu/make-atlas-resource-node! project "/from-script-release-variant.atlas")
      (let [;; The unprocessed lines that the user sees in the editor.
            script-lines
            ["local system = require 'system'"
             ""
             "--#IF DEBUG"
             "    local utils = require 'debug-utils'"
             "--#ENDIF"
             ""
             "--#IF RELEASE"
             "    local utils = require 'release-utils'"
             "--#ENDIF"
             ""
             "go.property('atlas', resource.atlas('/from-script.atlas'))"
             ""
             "--#IF DEBUG"
             "    go.property('debug-atlas', resource.atlas('/from-script-debug-variant.atlas'))"
             "--#ENDIF"
             ""
             "--#IF RELEASE"
             "    go.property('release-atlas', resource.atlas('/from-script-release-variant.atlas'))"
             "--#ENDIF"]

            ;; The expected output without the Lua preprocessor plugin. Note that
            ;; go.property declarations are stripped out regardless.
            expected-built-lines-before-preprocessing
            ["local system = require 'system'"
             ""
             ""
             "    local utils = require 'debug-utils'"
             ""
             ""
             ""
             "    local utils = require 'release-utils'"
             ""
             ""
             "                                                          "
             ""
             ""
             "                                                                                  "
             ""
             ""
             ""
             "                                                                                      "
             ""]

            ;; The expected output with the Lua preprocessor plugin. Since the
            ;; go.property declarations are stripped out regardless, only the
            ;; release-mode require line changes.
            expected-built-lines-after-preprocessing
            ["local system = require 'system'"
             ""
             ""
             "    local utils = require 'debug-utils'"
             ""
             ""
             ""
             "                                         "
             ""
             ""
             "                                                          "
             ""
             ""
             "                                                                                  "
             ""
             ""
             ""
             "                                                                                      "
             ""]

            script (tu/make-code-resource-node! project "/script.script" script-lines)]

        (testing "Without Lua preprocessing"
          (when (is (= #{(build-resource         "/script.script")
                         (build-resource         "/system.lua")
                         (build-resource         "/debug-utils.lua")
                         (build-resource         "/release-utils.lua")
                         (build-resource         "/from-script.atlas")
                         (texture-build-resource "/from-script.atlas")
                         (build-resource         "/from-script-debug-variant.atlas")
                         (texture-build-resource "/from-script-debug-variant.atlas")
                         (build-resource         "/from-script-release-variant.atlas")
                         (texture-build-resource "/from-script-release-variant.atlas")}
                       (tu/node-built-build-resources script)))
            (with-open [_ (tu/build! script)]
              (let [built-script (protobuf/bytes->map-with-defaults Lua$LuaModule (tu/node-build-output script))
                    built-lines (tu/lua-module-lines built-script)]
                (is (= expected-built-lines-before-preprocessing built-lines))
                (is (= {"atlas"         (build-resource-path-hash "/from-script.atlas")
                        "debug-atlas"   (build-resource-path-hash "/from-script-debug-variant.atlas")
                        "release-atlas" (build-resource-path-hash "/from-script-release-variant.atlas")}
                       (tu/unpack-property-declarations (:properties built-script))))
                (is (= (sort ["system"
                              "debug-utils"
                              "release-utils"])
                       (sort (:modules built-script))))
                (is (= (sort [(build-resource-path "/from-script.atlas")
                              (build-resource-path "/from-script-debug-variant.atlas")
                              (build-resource-path "/from-script-release-variant.atlas")])
                       (sort (:property-resources built-script))))))))

        (tu/set-libraries! workspace [extension-lua-preprocessor-url])

        (testing "With Lua preprocessing"
          (when (is (= #{(build-resource         "/script.script")
                         (build-resource         "/system.lua")
                         (build-resource         "/debug-utils.lua")
                         (build-resource         "/from-script.atlas")
                         (texture-build-resource "/from-script.atlas")
                         (build-resource         "/from-script-debug-variant.atlas")
                         (texture-build-resource "/from-script-debug-variant.atlas")}
                       (tu/node-built-build-resources script)))
            (with-open [_ (tu/build! script)]
              (let [built-script (protobuf/bytes->map-with-defaults Lua$LuaModule (tu/node-build-output script))
                    built-lines (tu/lua-module-lines built-script)]
                (is (= expected-built-lines-after-preprocessing built-lines))
                (is (= {"atlas" (build-resource-path-hash "/from-script.atlas")
                        "debug-atlas" (build-resource-path-hash "/from-script-debug-variant.atlas")}
                       (tu/unpack-property-declarations (:properties built-script))))
                (is (= (sort ["system"
                              "debug-utils"])
                       (sort (:modules built-script))))
                (is (= (sort [(build-resource-path "/from-script.atlas")
                              (build-resource-path "/from-script-debug-variant.atlas")])
                       (sort (:property-resources built-script))))))))))))
