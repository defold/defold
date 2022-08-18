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
            [editor.collection-data :as collection-data]
            [editor.game-object :as game-object]
            [editor.defold-project :as project]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [support.test-support :as test-support])
  (:import [com.dynamo.gameobject.proto GameObject$PrototypeDesc]
           [com.dynamo.proto DdfMath$Vector3]
           [java.io StringReader]))

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
          save-data (partial save-data project)
          add-component! (partial test-util/add-embedded-component! app-view)
          go-id (project/get-resource-node project "/game_object/test.go")
          go-resource (g/node-value go-id :resource)]
      (doseq [resource-type resource-types]
        (testing (:label resource-type)
          (with-open [_ (test-util/make-graph-reverter (project/graph project))]
            (add-component! resource-type go-id)
            (let [saved-string (:content (save-data go-resource))
                  saved-embedded-components (g/node-value go-id :embed-ddf)
                  loaded-embedded-components (:embedded-components (protobuf/read-text GameObject$PrototypeDesc (StringReader. saved-string)))
                  [only-in-saved only-in-loaded] (data/diff saved-embedded-components loaded-embedded-components)]
              (is (nil? only-in-saved))
              (is (nil? only-in-loaded))
              (when (or (some? only-in-saved)
                        (some? only-in-loaded))
                (println "When comparing" (:label resource-type))
                (when-not (nil? only-in-saved)
                  (prn 'only-in-saved only-in-saved))
                (when-not (nil? only-in-loaded)
                  (prn 'only-in-loaded only-in-loaded))))))))))

(defn- vector3-pb
  ^DdfMath$Vector3 [^double x ^double y ^double z]
  (-> (DdfMath$Vector3/newBuilder)
      (.setX x)
      (.setY y)
      (.setZ z)
      (.build)))

(deftest component-ddf-scale
  (test-support/with-clean-system
    (let [workspace (test-util/setup-scratch-workspace! world "test/resources/small_project")
          atlas-resource (workspace/find-resource workspace "/main/logo.atlas")
          atlas-proj-path (resource/proj-path atlas-resource)
          game-object-resource (test-util/make-resource! workspace "/test.go" {})
          sprite-resource (test-util/make-resource! workspace "/test.sprite" {:tile-set atlas-proj-path :default-animation "logo"})
          sprite-resource-type (resource/resource-type sprite-resource)]
      (workspace/resource-sync! workspace)
      (let [project (test-util/setup-project! workspace)
            app-view (test-util/setup-app-view! project)
            game-object (project/get-resource-node project game-object-resource)
            embedded-component (test-util/add-embedded-component! app-view sprite-resource-type game-object)
            referenced-component (test-util/add-referenced-component! app-view sprite-resource game-object)]
        (test-util/prop! embedded-component :image atlas-resource)
        (test-util/prop! embedded-component :default-animation "logo")

        (testing "Unscaled components do not include scale in save data."
          (is (= [1.0 1.0 1.0] (g/node-value embedded-component :scale)))
          (is (= [1.0 1.0 1.0] (g/node-value referenced-component :scale)))
          (let [game-object-saved-pb (test-util/saved-pb game-object GameObject$PrototypeDesc)]
            (is (= 1 (.getEmbeddedComponentsCount game-object-saved-pb)))
            (is (= 1 (.getComponentsCount game-object-saved-pb)))
            (let [embedded-component-saved-pb (.getEmbeddedComponents game-object-saved-pb 0)
                  referenced-component-saved-pb (.getComponents game-object-saved-pb 0)]
              (is (not (.hasScale embedded-component-saved-pb)))
              (is (not (.hasScale referenced-component-saved-pb))))))

        (testing "Unscaled components include default scale in built binaries."
          (with-open [_ (test-util/build! game-object)]
            (let [game-object-built-pb (test-util/built-pb game-object GameObject$PrototypeDesc)]
              (is (= 2 (.getComponentsCount game-object-built-pb)))
              (let [embedded-component-built-pb (.getComponents game-object-built-pb 0)
                    referenced-component-built-pb (.getComponents game-object-built-pb 1)]
                (is (.hasScale embedded-component-built-pb))
                (is (.hasScale referenced-component-built-pb))
                (is (= (vector3-pb 1.0 1.0 1.0) (.getScale embedded-component-built-pb)))
                (is (= (vector3-pb 1.0 1.0 1.0) (.getScale referenced-component-built-pb)))))))

        (testing "Scaled components include scale in save data."
          (g/transact
            [(g/set-property embedded-component :scale [2.0 3.0 4.0])
             (g/set-property referenced-component :scale [5.0 6.0 7.0])])
          (let [game-object-saved-pb (test-util/saved-pb game-object GameObject$PrototypeDesc)]
            (is (= 1 (.getEmbeddedComponentsCount game-object-saved-pb)))
            (is (= 1 (.getComponentsCount game-object-saved-pb)))
            (let [embedded-component-saved-pb (.getEmbeddedComponents game-object-saved-pb 0)
                  referenced-component-saved-pb (.getComponents game-object-saved-pb 0)]
              (is (.hasScale embedded-component-saved-pb))
              (is (.hasScale referenced-component-saved-pb))
              (is (= (vector3-pb 2.0 3.0 4.0) (.getScale embedded-component-saved-pb)))
              (is (= (vector3-pb 5.0 6.0 7.0) (.getScale referenced-component-saved-pb))))))

        (testing "Scaled components include scale in built binaries."
          (with-open [_ (test-util/build! game-object)]
            (let [game-object-built-pb (test-util/built-pb game-object GameObject$PrototypeDesc)]
              (is (= 2 (.getComponentsCount game-object-built-pb)))
              (let [embedded-component-built-pb (.getComponents game-object-built-pb 0)
                    referenced-component-built-pb (.getComponents game-object-built-pb 1)]
                (is (.hasScale embedded-component-built-pb))
                (is (.hasScale referenced-component-built-pb))
                (is (= (vector3-pb 2.0 3.0 4.0) (.getScale embedded-component-built-pb)))
                (is (= (vector3-pb 5.0 6.0 7.0) (.getScale referenced-component-built-pb)))))))))))
