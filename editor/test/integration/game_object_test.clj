;; Copyright 2020-2022 The Defold Foundation
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

(ns integration.game-object-test
  (:require [clojure.data :as data]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.game-object :as game-object]
            [editor.defold-project :as project]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util])
  (:import [java.io StringReader]))

(set! *warn-on-reflection* true)

(defn- build-error? [node-id]
  (g/error? (g/node-value node-id :build-targets)))

(deftest validation
  (test-util/with-loaded-project
    (let [go-id   (test-util/resource-node project "/game_object/props.go")
          comp-id   (:node-id (test-util/outline go-id [0]))]
      (testing "component ref instance"
               (is (not (build-error? go-id)))
               (is (nil? (test-util/prop-error comp-id :path)))
               (test-util/with-prop [comp-id :path {:resource nil :overrides []}]
                 (is (g/error? (test-util/prop-error comp-id :path))))
               (let [not-found (workspace/resolve-workspace-resource workspace "/not_found.script")]
                 (test-util/with-prop [comp-id :path {:resource not-found :overrides []}]
                   (is (g/error? (test-util/prop-error comp-id :path)))))
               (let [unknown-resource (workspace/resolve-workspace-resource workspace "/unknown-resource.gurka")]
                 (test-util/with-prop [comp-id :path {:resource unknown-resource :overrides []}]
                   (is (g/error? (test-util/prop-error comp-id :path)))
                   (is (build-error? go-id)))))
      (testing "component embedded instance"
               (let [r-type (workspace/get-resource-type workspace "factory")]
                 (game-object/add-embedded-component-handler {:_node-id go-id :resource-type r-type} nil)
                 (let [factory (:node-id (test-util/outline go-id [0]))]
                   (test-util/with-prop [factory :id "script"]
                     (is (g/error? (test-util/prop-error factory :id)))
                     (is (g/error? (test-util/prop-error comp-id :id)))
                     (is (build-error? go-id)))))))))

(defn- save-data
  [project resource]
  (first (filter #(= resource (:resource %))
                 (project/all-save-data project))))

(deftest embedded-components
  (test-util/with-loaded-project
    (let [resource-types (game-object/embeddable-component-resource-types workspace)
          add-component! (partial test-util/add-embedded-component! app-view)
          go-id (project/get-resource-node project "/game_object/test.go")
          go-resource (g/node-value go-id :resource)
          go-read-fn (:read-fn (resource/resource-type go-resource))]
      (doseq [resource-type resource-types]
        (testing (:label resource-type)
          (with-open [_ (test-util/make-graph-reverter (project/graph project))]
            (add-component! resource-type go-id)
            (let [save-value (g/node-value go-id :save-value)
                  load-value (with-open [reader (StringReader. (:content (g/node-value go-id :save-data)))]
                               (go-read-fn reader))
                  saved-embedded-components (:embedded-components save-value)
                  loaded-embedded-components (:embedded-components load-value)
                  [only-in-saved only-in-loaded] (data/diff saved-embedded-components loaded-embedded-components)]
              (is (nil? only-in-saved))
              (is (nil? only-in-loaded))
              (when (or (some? only-in-saved)
                        (some? only-in-loaded))
                (println "When comparing" (:label resource-type))
                (prn 'disk only-in-loaded)
                (prn 'save only-in-saved)))))))))
