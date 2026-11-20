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

(ns integration.bob-test
  (:require [clojure.java.io :as io]
            [clojure.string :as str]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.build-errors-view :as build-errors-view]
            [editor.code.data :as data]
            [editor.engine.build-errors :as build-errors]
            [editor.pipeline.bob :as bob]
            [integration.test-util :as test-util])
  (:import [java.io ByteArrayOutputStream]))

(defn- bob-build-error [project]
  (g/with-auto-evaluation-context evaluation-context
    (let [{:keys [error exception]} (bob/invoke! project
                                                 {"build-input-file" "build.inputs"
                                                  "texture-compression" true
                                                  "verbose" false}
                                                 ["build"]
                                                 :evaluation-context evaluation-context
                                                 :log-output-stream (ByteArrayOutputStream.))]
      (if exception
        (build-errors/exception->error-value exception project evaluation-context)
        error))))

;; Missing texture compressors and presets from the real Bob adapter remain
;; clickable resource errors.
(deftest texture-compression-errors-are-clickable
  (doseq [[compressor preset message]
          [["MISSING_COMPRESSOR" "UNCOMPRESSED" "Texture compressor 'MISSING_COMPRESSOR' not found in texture profile 'Default'."]
           ["BasisU" "MISSING_PRESET" "Texture compressor preset 'MISSING_PRESET' not found in texture profile 'Default'."]]]
    (testing message
      (test-util/with-temp-project-content
        {"/test.texture_profiles" {:profiles [{:name "Default"
                                               :platforms [{:os :os-id-generic
                                                            :mipmaps false
                                                            :formats [{:format :texture-format-rgba
                                                                       :compressor compressor
                                                                       :compressor-preset preset}]}]}]}}
        (spit (io/file project-path "build.inputs") "/test.texture_profiles\n")
        (let [error (bob-build-error project)
              tree (build-errors-view/build-resource-tree error)
              item (get-in tree [:children 0 :children 0])]
          (is (g/error? error))
          (is (= message (:message item)))
          (is (= :fatal (:severity item)))
          (is (= {:type :resource
                  :args [(test-util/resource workspace "/test.texture_profiles")
                         (test-util/resource-node project "/test.texture_profiles")
                         {}]}
                 (build-errors-view/error-item-open-info item))))))))

;; Unrelated Lua parse failures keep their diagnosis and clickable source line
;; through the Bob adapter.
(deftest lua-parse-errors-keep-source-location
  (test-util/with-temp-project-content
    {"/syntax_error.script" ["-- A syntax error on line three."
                             ""
                             "local value = )"]}
    (spit (io/file project-path "build.inputs") "/syntax_error.script\n")
    (let [error (bob-build-error project)
          tree (build-errors-view/build-resource-tree error)
          item (get-in tree [:children 0 :children 0])
          cursor-range (data/line-number->CursorRange 3)]
      (is (g/error? error))
      (is (str/includes? (:message item "") "mismatched input ')'"))
      (is (= :fatal (:severity item)))
      (is (= cursor-range (:cursor-range item)))
      (is (= {:type :resource
              :args [(test-util/resource workspace "/syntax_error.script")
                     (test-util/resource-node project "/syntax_error.script")
                     {:cursor-range cursor-range}]}
             (build-errors-view/error-item-open-info item))))))
