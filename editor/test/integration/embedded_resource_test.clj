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

(ns integration.embedded-resource-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.fs :as fs]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [support.test-support :refer [with-clean-system]])
  (:import [com.google.protobuf ByteString]))

(deftest expansion-is-independent-of-workspace-resource-types
  (let [project-path (test-util/make-temp-project-copy! "test/resources/empty_project")
        extension "embedded-test"]
    (.addMethod resource/expand extension
                (fn [source stream]
                  (assoc source :children [(resource/make-resource-entry source {:path "text" :ext "txt" :content (ByteString/readFrom stream)})])))
    (try
      (with-open [_deleter (test-util/make-directory-deleter project-path)]
        (doseq [index (range 16)]
          (fs/create-file! (io/file project-path (str "example" index ".embedded-test")) "hello"))
        (with-clean-system
          (let [workspace (test-util/setup-workspace! project-path)]
            ;; No workspace resource type was registered for the container.
            (doseq [index (range 16)]
              (let [child (workspace/find-resource workspace (str "/example" index ".embedded-test/text"))]
                (is (= "txt" (resource/type-ext child)))
                (is (= "hello" (slurp child)))))
            (let [child-path "/example0.embedded-test/text"]
              (workspace/resource-sync! workspace)
              (is (= "hello" (slurp (workspace/find-resource workspace child-path))))
              (let [project (test-util/setup-project! workspace)]
                (is (= ["hello"] (g/node-value (test-util/resource-node project child-path) :lines))))))))
      (finally
        (remove-method resource/expand extension)))))
