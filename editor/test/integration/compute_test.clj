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

(ns integration.compute-test
  (:require [clojure.test :refer [deftest is]]
            [dynamo.graph :as g]
            [editor.os :as os]
            [integration.test-util :as test-util]
            [util.coll :as coll])
  (:import [com.dynamo.graphics.proto Graphics$ShaderDesc Graphics$ShaderDesc$Language]))

(deftest compute-build-targets
  (test-util/with-loaded-project "test/resources/all_types_project"
    (let [node-id (test-util/resource-node project "/test.compute")]
      (is (not (g/error? (g/node-value node-id :build-targets)))))))

(deftest storage-buffer-build-targets
  (test-util/with-temp-project-content
    {"/storage.compute" {:compute-program "/storage.cp"}
     "/storage.cp" ["#version 430"
                    "layout(local_size_x = 1) in;"
                    "layout(std430, binding = 0) buffer Output { uint value; };"
                    "void main() { value = gl_GlobalInvocationID.x; }"]}
    (let [node (test-util/resource-node project "/storage.compute")
          targets (g/node-value node :build-targets)]
      (is (not (g/error? targets)))
      (when-not (g/error? targets)
        (let [^Graphics$ShaderDesc shader (get-in targets [0 :deps 0 :user-data :shader-desc])]
          (is (some? shader))
          (when shader
            (is (= (os/is-win32?)
                   (coll/any? #(= Graphics$ShaderDesc$Language/LANGUAGE_HLSL_51 (.getLanguage %)) (.getShadersList shader))))))))))
