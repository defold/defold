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

(ns integration.extension-simpledata-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]))

(set! *warn-on-reflection* true)

(def ^:private project-path "test/resources/simpledata_project")

(deftest registered-resource-types-test
  (test-util/with-loaded-project project-path
    (is (= #{} (test-util/protobuf-resource-exts-that-read-defaults workspace)))))

(deftest dirty-save-data-test
  (test-util/with-loaded-project project-path
    (test-util/clear-cached-save-data! project)
    (is (= #{} (test-util/dirty-proj-paths project)))
    (test-util/edit-proj-path! project "/main/fields_assigned.simpledata")
    (is (= #{"/main/fields_assigned.simpledata"} (test-util/dirty-proj-paths project)))
    (test-util/edit-proj-path! project "/main/fields_unassigned.simpledata")
    (is (= #{"/main/fields_assigned.simpledata" "/main/fields_unassigned.simpledata"} (test-util/dirty-proj-paths project)))))

(deftest outputs-test
  (test-util/with-loaded-project project-path
    (let [fields-assigned (test-util/resource-node project "/main/fields_assigned.simpledata")
          fields-unassigned (test-util/resource-node project "/main/fields_unassigned.simpledata")]

      (testing "build-targets"
        (is (not (g/error? (g/node-value fields-assigned :build-targets))))
        (is (not (g/error? (g/node-value fields-unassigned :build-targets)))))

      (testing "form-data"
        (is (= {:sections [{:fields [{:label "Name"
                                      :path [:name]
                                      :type :string}
                                     {:label "F32"
                                      :path [:f32]
                                      :type :number}
                                     {:label "U32"
                                      :path [:u32]
                                      :type :integer}
                                     {:label "I32"
                                      :path [:i32]
                                      :type :integer}
                                     {:label "U64"
                                      :path [:u64]
                                      :type :integer}
                                     {:label "I64"
                                      :path [:i64]
                                      :type :integer}
                                     {:label "V3"
                                      :path [:v3]
                                      :type :vec4}]
                            :title "SimpleData"}]
                :values {[:name] "simple-data-test"
                         [:f32] (float 1.0)
                         [:u32] (int 1)
                         [:i32] (int 1)
                         [:u64] (long 1)
                         [:i64] (long 1)
                         [:v3] (vector-of :float 0.1 0.2 0.3)}}
               (-> fields-assigned
                   (g/valid-node-value :form-data)
                   (select-keys [:sections :values])))))

      (testing "node-outline"
        (let [node-outline (g/valid-node-value fields-assigned :node-outline)]
          (is (= "Simple Data" (:label node-outline)))
          (is (empty? (:children node-outline)))))

      (testing "save-value"
        (is (= {:name "simple-data-test"
                :f32 (float 1.0)
                :u32 (int 1)
                :i32 (int 1)
                :u64 (long 1)
                :i64 (long 1)
                :v3 (vector-of :float 0.1 0.2 0.3)
                :array-f32 (vector-of :float 0.1 0.2 0.3 0.4)}
               (g/node-value fields-assigned :save-value)))
        (is (= {:name "untitled"}
               (g/node-value fields-unassigned :save-value)))))))

(defn- built-pb-map [project proj-path]
  (let [workspace (project/workspace project)
        resource (workspace/find-resource workspace proj-path)
        pb-class (-> resource resource/resource-type :test-info :ddf-type)]
    (->> resource
         (test-util/build-output project)
         (protobuf/bytes->map-without-defaults pb-class))))

(deftest build-test
  (test-util/with-scratch-project project-path
    (test-util/build! (test-util/resource-node project "/main/main.collection"))
    (is (= {:name "simple-data-test"
            :f32 (float 1.0)
            :u32 (int 1)
            :i32 (int 1)
            :u64 (long 1)
            :i64 (long 1)
            :v3 (vector-of :float 0.1 0.2 0.3)
            :array-f32 (vector-of :float 0.1 0.2 0.3 0.4)}
           (built-pb-map project "/main/fields_assigned.simpledata")))
    (is (= {:name "untitled"}
           (built-pb-map project "/main/fields_unassigned.simpledata")))))

(deftest collection-usage-test
  (test-util/with-loaded-project project-path
    (let [main-collection (test-util/resource-node project "/main/main.collection")]
      (is (not (g/error? (g/node-value main-collection :build-targets))))
      (is (not (g/error? (g/node-value main-collection :scene))))
      (is (not (g/error? (g/node-value main-collection :node-outline)))))))
