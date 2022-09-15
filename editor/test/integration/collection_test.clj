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

(ns integration.collection-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.collection :as collection]
            [editor.defold-project :as project]
            [editor.fs :as fs]
            [editor.game-object :as game-object]
            [editor.geom :as geom]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [internal.graph.types :as gt]
            [support.test-support :refer [with-clean-system graph-dependencies]])
  (:import [com.dynamo.gameobject.proto GameObject$CollectionDesc GameObject$PrototypeDesc]
           [com.dynamo.proto DdfMath$Vector3]))

(deftest hierarchical-outline
  (testing "Hierarchical outline"
    (test-util/with-loaded-project
      (let [node-id   (test-util/resource-node project "/logic/hierarchy.collection")
            outline   (g/node-value node-id :node-outline)]
        ;; Two game objects under the collection
        (is (= 2 (count (:children outline))))
        ;; One component and game object under the game object
        (is (= 2 (count (:children (second (:children outline)))))))))
  (testing "Deleting hierarchy deletes children"
    (test-util/with-loaded-project
      (let [node-id   (test-util/resource-node project "/logic/hierarchy.collection")
            outline   (g/node-value node-id :node-outline)
            parent    (first (filter #(= "parent_id" (:label %)) (tree-seq :children :children outline)))]
        (is (= #{"parent_id" "child_id" "embedded_id"} (set (g/node-value node-id :ids))))
        (g/delete-node! (:node-id parent))
        (is (= #{"embedded_id"} (set (g/node-value node-id :ids))))))))

(deftest hierarchical-scene
  (testing "Hierarchical scene"
           (test-util/with-loaded-project
             (let [node-id   (test-util/resource-node project "/logic/hierarchy.collection")
                   scene     (g/node-value node-id :scene)]
               ; Two game objects under the collection
               (is (= 2 (count (:children scene))))
               ; One component and game object under the game object
               (is (= 2 (count (:children (first (:children scene))))))))))

(defn- reachable? [source target]
  (contains? (graph-dependencies [source]) target))

(deftest two-instances-are-invalidated
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/logic/two_atlas_sprites.collection")
          scene (g/node-value node-id :scene)
          go-id (test-util/resource-node project "/logic/atlas_sprite.go")
          go-scene (g/node-value go-id :scene)
          sprite (get-in go-scene [:children 0 :node-id])]
      (is (reachable? (gt/endpoint sprite :scene) (gt/endpoint go-id :scene)))
      (is (reachable? (gt/endpoint sprite :scene) (gt/endpoint (get-in scene [:children 0 :node-id]) :scene)))
      (is (reachable? (gt/endpoint go-id :scene) (gt/endpoint (get-in scene [:children 0 :node-id]) :scene)))
      (is (reachable? (gt/endpoint (get-in scene [:children 0 :node-id]) :scene) (gt/endpoint node-id :scene))))))

(deftest add-embedded-instance
  (testing "Hierarchical scene"
           (test-util/with-loaded-project
             (let [node-id   (test-util/resource-node project "/logic/hierarchy.collection")]
               ; Two game objects under the collection
               (is (= 2 (count (:children (g/node-value node-id :node-outline)))))
               ; Select the collection node
               (app-view/select! app-view [node-id])
               ; Run the add handler
               (test-util/handler-run :add [{:name :workbench :env {:workspace workspace :project project :app-view app-view :selection [node-id]}}] {})
               ; Three game objects under the collection
               (is (= 3 (count (:children (g/node-value node-id :node-outline)))))))))

(deftest empty-go
  (testing "Collection with a single empty game object"
           (test-util/with-loaded-project
             (let [node-id   (test-util/resource-node project "/collection/empty_go.collection")
                   outline   (g/node-value node-id :node-outline)
                   scene     (g/node-value node-id :scene)]
               ;; Verify outline labels
               (is (= (list "Collection" "go") (map :label (tree-seq :children :children outline))))
               ;; Verify AABBs
               (is (= [geom/null-aabb geom/empty-bounding-box]
                      (map :aabb (tree-seq :children :children (g/node-value node-id :scene)))))))))

(deftest unknown-components
  (testing "Load a collection with unknown components"
           (test-util/with-loaded-project
             (let [node-id   (test-util/resource-node project "/collection/unknown_components.collection")
                   outline   (g/node-value node-id :node-outline)
                   scene     (g/node-value node-id :scene)]
               ;; Verify outline labels
               (is (= (list "Collection" "my_instance" "unknown")
                      (map :label (tree-seq :children :children outline))))
               ;; Verify AABBs
               (is (= [geom/null-aabb geom/empty-bounding-box geom/empty-bounding-box]
                      (map :aabb (tree-seq :children :children (g/node-value node-id :scene)))))))))

(defn- prop [node-id path prop]
  (-> (test-util/outline node-id path)
    :node-id
    (test-util/prop prop)))

(defn- url-prop [node-id path]
  (prop node-id path :url))

(deftest urls
  (testing "Checks URLs at different levels"
           (test-util/with-loaded-project
             (let [node-id   (test-util/resource-node project "/collection/sub_sub_props.collection")]
               (is (= "/sub_props" (url-prop node-id [0])))
               (is (= "/sub_props/props" (url-prop node-id [0 0])))
               (is (= "/sub_props/props/props" (url-prop node-id [0 0 0])))
               (is (= "/sub_props/props/props#script" (url-prop node-id [0 0 0 0])))))))

(defn- script-prop [node-id name]
  (let [key (properties/user-name->key name)]
    (test-util/prop node-id key)))

(defn- script-prop! [node-id name v]
  (let [key (properties/user-name->key name)]
    (test-util/prop! node-id key v)))

(defn- script-prop-clear! [node-id name]
  (let [key (properties/user-name->key name)]
    (test-util/prop-clear! node-id key)))

(deftest add-script-properties
  (test-util/with-loaded-project
    (let [parent-id (test-util/resource-node project "/collection/parent.collection")
          coll-id   (test-util/resource-node project "/collection/test.collection")
          go-id     (test-util/resource-node project "/game_object/test.go")
          script-id (test-util/resource-node project "/script/props.script")
          select-fn (fn [node-ids] (app-view/select app-view node-ids))]
      (g/transact (collection/add-collection-instance parent-id (test-util/resource workspace "/collection/test.collection") "child" [0 0 0] [0 0 0 1] [1 1 1] []))
      (collection/add-game-object-file coll-id coll-id (test-util/resource workspace "/game_object/test.go") select-fn)
      (is (nil? (test-util/outline coll-id [0 0])))
      (let [inst (first (test-util/selection app-view))]
        (game-object/add-component-file go-id (test-util/resource workspace "/script/props.script") select-fn)
        (let [parent-comp (:node-id (test-util/outline parent-id [0 0 0]))
              coll-comp (:node-id (test-util/outline coll-id [0 0]))
              go-comp (:node-id (test-util/outline go-id [0]))]
          (is (= [coll-comp] (g/overrides go-comp)))
          (let [coll-script (ffirst (g/sources-of coll-comp :source-id))
                go-script (ffirst (g/sources-of go-comp :source-id))]
            (is (= [coll-script] (g/overrides go-script)))
            (is (some #{go-script} (g/overrides script-id))))
          (is (= 1.0 (script-prop go-comp "number")))
          (is (= 1.0 (script-prop coll-comp "number")))
          (is (= 1.0 (script-prop parent-comp "number")))
          (script-prop! go-comp "number" 2.0)
          (is (= 2.0 (script-prop go-comp "number")))
          (is (= 2.0 (script-prop coll-comp "number")))
          (is (= 2.0 (script-prop parent-comp "number")))
          (script-prop! coll-comp "number" 3.0)
          (is (= 2.0 (script-prop go-comp "number")))
          (is (= 3.0 (script-prop coll-comp "number")))
          (is (= 3.0 (script-prop parent-comp "number")))
          (script-prop-clear! coll-comp "number")
          (is (= 2.0 (script-prop coll-comp "number")))
          (is (= 2.0 (script-prop parent-comp "number")))
          (script-prop-clear! go-comp "number")
          (is (= 1.0 (script-prop go-comp "number")))
          (is (= 1.0 (script-prop coll-comp "number")))
          (script-prop! coll-comp "number" 4.0)
          (is (= 1.0 (script-prop go-comp "number")))
          (is (= 4.0 (script-prop coll-comp "number")))
          (is (= 4.0 (script-prop parent-comp "number")))
          (test-util/code-editor-source! script-id "go.property(\"new_value\", 2.0)\n")
          (is (= 2.0 (script-prop coll-comp "new_value"))))))))

(deftest read-only-id-property
  (test-util/with-loaded-project
    (let [parent-id (test-util/resource-node project "/collection/parent.collection")
          coll-id   (test-util/resource-node project "/collection/test.collection")
          go-id     (test-util/resource-node project "/game_object/test.go")
          script-id (test-util/resource-node project "/script/props.script")
          select-fn (fn [node-ids] (app-view/select app-view node-ids))]
      (g/transact (collection/add-collection-instance parent-id (test-util/resource workspace "/collection/test.collection") "child" [0 0 0] [0 0 0 1] [1 1 1] []))
      (collection/add-game-object-file coll-id coll-id (test-util/resource workspace "/game_object/test.go") select-fn)
      (game-object/add-component-file go-id (test-util/resource workspace "/script/props.script") select-fn)
      (testing "component id should only be editable on the game object including the component"
        (let [go-comp (:node-id (test-util/outline go-id [0]))
              coll-comp (:node-id (test-util/outline coll-id [0 0]))
              parent-comp (:node-id (test-util/outline parent-id [0 0 0]))]
          (is (not (test-util/prop-read-only? go-comp :id)))
          (is (test-util/prop-read-only? coll-comp :id))
          (is (test-util/prop-read-only? parent-comp :id))))
      (testing "game object id should only be editable on the collection including the game object"
        (let [coll-go (:node-id (test-util/outline coll-id [0]))
              parent-go (:node-id (test-util/outline parent-id [0 0]))]
          (is (not (test-util/prop-read-only? coll-go :id)))
          (is (test-util/prop-read-only? parent-go :id)))))))

(defn- build-error? [node-id]
  (g/error? (g/node-value node-id :build-targets)))

(deftest validation
  (test-util/with-loaded-project
    (let [coll-id   (test-util/resource-node project "/collection/props.collection")
          inst-id   (:node-id (test-util/outline coll-id [0]))]
      (testing "game object ref instance"
               (is (not (build-error? coll-id)))
               (is (nil? (test-util/prop-error inst-id :path)))
               (test-util/with-prop [inst-id :path {:resource nil :overrides []}]
                 (is (g/error? (test-util/prop-error inst-id :path)))
                 (is (build-error? coll-id)))
               (let [not-found (workspace/resolve-workspace-resource workspace "/not_found.go")]
                 (test-util/with-prop [inst-id :path {:resource not-found :overrides []}]
                   (is (g/error? (test-util/prop-error inst-id :path)))))
               (test-util/with-prop [inst-id :id "props_embedded"]
                 (is (g/error? (test-util/prop-error inst-id :id)))
                 (is (build-error? coll-id))))
      (testing "game object embedded instance"
               (let [inst-id (:node-id (test-util/outline coll-id [1]))]
                 (is (nil? (test-util/prop-error inst-id :id)))
                 (test-util/with-prop [inst-id :id "props"]
                   (is (g/error? (test-util/prop-error inst-id :id))))))
      (testing "collection ref instance"
               (is (not (build-error? coll-id)))
               (let [res (workspace/resolve-workspace-resource workspace "/collection/test.collection")]
                 (g/transact (collection/add-collection-instance coll-id res "coll" [0 0 0] [0 0 0 1] [1 1 1] []))
                 (let [inst-id (:node-id (test-util/outline coll-id [0]))]
                   (is (nil? (test-util/prop-error inst-id :path)))
                   (test-util/with-prop [inst-id :path {:resource nil :overrides []}]
                     (is (g/error? (test-util/prop-error inst-id :path)))
                     (is (build-error? coll-id)))
                   (let [not-found (workspace/resolve-workspace-resource workspace "/not_found.collection")]
                     (test-util/with-prop [inst-id :path {:resource not-found :overrides []}]
                       (is (g/error? (test-util/prop-error inst-id :path)))))
                   (is (nil? (test-util/prop-error inst-id :id)))
                   (test-util/with-prop [inst-id :id "props"]
                     (is (g/error? (test-util/prop-error inst-id :id)))
                     (is (build-error? coll-id)))))))))

(defn- vector3-pb
  ^DdfMath$Vector3 [^double x ^double y ^double z]
  (-> (DdfMath$Vector3/newBuilder)
      (.setX x)
      (.setY y)
      (.setZ z)
      (.build)))

(deftest component-ddf-scale
  (letfn [(make-components! [game-object sprite-resource atlas-resource app-view]
            (let [sprite-resource-type (resource/resource-type sprite-resource)
                  embedded-component (test-util/add-embedded-component! app-view sprite-resource-type game-object)
                  referenced-component (test-util/add-referenced-component! app-view sprite-resource game-object)]
              (test-util/prop! embedded-component :image atlas-resource)
              (test-util/prop! embedded-component :default-animation "logo")
              [embedded-component referenced-component]))

          (test-unscaled [embedded-component referenced-component]
            (testing "Unscaled components have identity scale."
              (is (= [1.0 1.0 1.0] (g/node-value embedded-component :scale)))
              (is (= [1.0 1.0 1.0] (g/node-value referenced-component :scale)))))

          (test-unscaled-saved-pb [game-object-saved-pb]
            (testing "Unscaled components do not include scale in save data."
              (is (= 1 (.getEmbeddedComponentsCount game-object-saved-pb)))
              (is (= 1 (.getComponentsCount game-object-saved-pb)))
              (let [embedded-component-saved-pb (.getEmbeddedComponents game-object-saved-pb 0)
                    referenced-component-saved-pb (.getComponents game-object-saved-pb 0)]
                (is (not (.hasScale embedded-component-saved-pb)))
                (is (not (.hasScale referenced-component-saved-pb))))))

          (test-unscaled-built-pb [game-object-built-pb]
            (testing "Unscaled components include default scale in built binaries."
              (is (= 2 (.getComponentsCount game-object-built-pb)))
              (let [embedded-component-built-pb (.getComponents game-object-built-pb 0)
                    referenced-component-built-pb (.getComponents game-object-built-pb 1)]
                (is (.hasScale embedded-component-built-pb))
                (is (.hasScale referenced-component-built-pb))
                (is (= (vector3-pb 1.0 1.0 1.0) (.getScale embedded-component-built-pb)))
                (is (= (vector3-pb 1.0 1.0 1.0) (.getScale referenced-component-built-pb))))))

          (scale-components! [embedded-component referenced-component]
            (g/transact
              [(g/set-property embedded-component :scale [2.0 3.0 4.0])
               (g/set-property referenced-component :scale [5.0 6.0 7.0])]))

          (test-scaled-saved-pb [game-object-saved-pb]
            (testing "Scaled components include assigned scale in save data."
              (is (= 1 (.getEmbeddedComponentsCount game-object-saved-pb)))
              (is (= 1 (.getComponentsCount game-object-saved-pb)))
              (let [embedded-component-saved-pb (.getEmbeddedComponents game-object-saved-pb 0)
                    referenced-component-saved-pb (.getComponents game-object-saved-pb 0)]
                (is (.hasScale embedded-component-saved-pb))
                (is (.hasScale referenced-component-saved-pb))
                (is (= (vector3-pb 2.0 3.0 4.0) (.getScale embedded-component-saved-pb)))
                (is (= (vector3-pb 5.0 6.0 7.0) (.getScale referenced-component-saved-pb))))))

          (test-scaled-built-pb [game-object-built-pb]
            (testing "Scaled components include assigned scale in built binaries."
              (is (= 2 (.getComponentsCount game-object-built-pb)))
              (let [embedded-component-built-pb (.getComponents game-object-built-pb 0)
                    referenced-component-built-pb (.getComponents game-object-built-pb 1)]
                (is (.hasScale embedded-component-built-pb))
                (is (.hasScale referenced-component-built-pb))
                (is (= (vector3-pb 2.0 3.0 4.0) (.getScale embedded-component-built-pb)))
                (is (= (vector3-pb 5.0 6.0 7.0) (.getScale referenced-component-built-pb))))))

          (game-object-saved-pb [game-object]
            (test-util/saved-pb game-object GameObject$PrototypeDesc))

          (game-object-built-pb [game-object]
            (test-util/built-pb game-object GameObject$PrototypeDesc))

          (collection-game-object-saved-pb [collection game-object-index]
            (let [collection-saved-pb (test-util/saved-pb collection GameObject$CollectionDesc)]
              (is (< game-object-index (.getEmbeddedInstancesCount collection-saved-pb)))
              (let [game-object-embedded-instance-saved-pb (.getEmbeddedInstances collection-saved-pb game-object-index)
                    embedded-game-object-string (.getData game-object-embedded-instance-saved-pb)]
                (protobuf/str->pb GameObject$PrototypeDesc embedded-game-object-string))))

          (collection-game-object-built-pb [collection game-object-index]
            (let [collection-resource (g/node-value collection :resource)
                  workspace (resource/workspace collection-resource)
                  collection-built-pb (test-util/built-pb collection GameObject$CollectionDesc)]
              (is (< game-object-index (.getInstancesCount collection-built-pb)))
              (let [game-object-instance-built-pb (.getInstances collection-built-pb game-object-index)
                    game-object-build-output-file (workspace/build-path workspace (.getPrototype game-object-instance-built-pb))
                    game-object-build-output-bytes (fs/read-bytes game-object-build-output-file)]
                (protobuf/bytes->pb GameObject$PrototypeDesc game-object-build-output-bytes))))]
    (with-clean-system
      (let [workspace (test-util/setup-scratch-workspace! world "test/resources/small_project")
            atlas-resource (workspace/find-resource workspace "/main/logo.atlas")
            atlas-proj-path (resource/proj-path atlas-resource)
            game-object-resource (test-util/make-resource! workspace "/test.go" {})
            collection-resource (test-util/make-resource! workspace "/test.collection" {:name "collection"})
            sprite-resource (test-util/make-resource! workspace "/test.sprite" {:tile-set atlas-proj-path :default-animation "logo"})]
        (workspace/resource-sync! workspace)
        (let [project (test-util/setup-project! workspace)
              app-view (test-util/setup-app-view! project)]

          (testing "Components in game object."
            (let [game-object (project/get-resource-node project game-object-resource)
                  [embedded-component referenced-component] (make-components! game-object sprite-resource atlas-resource app-view)]
              (test-unscaled embedded-component referenced-component)
              (test-unscaled-saved-pb (game-object-saved-pb game-object))
              (with-open [_ (test-util/build! game-object)]
                (test-unscaled-built-pb (game-object-built-pb game-object)))
              (scale-components! embedded-component referenced-component)
              (test-scaled-saved-pb (game-object-saved-pb game-object))
              (with-open [_ (test-util/build! game-object)]
                (test-scaled-built-pb (game-object-built-pb game-object)))))

          (testing "Components in game object embedded inside collection."
            (let [collection (project/get-resource-node project collection-resource)
                  game-object-instance (test-util/add-embedded-game-object! app-view project collection)
                  game-object (test-util/to-game-object-id game-object-instance)
                  [embedded-component referenced-component] (make-components! game-object sprite-resource atlas-resource app-view)]
              (test-unscaled embedded-component referenced-component)
              (test-unscaled-saved-pb (collection-game-object-saved-pb collection 0))
              (with-open [_ (test-util/build! collection)]
                (test-unscaled-built-pb (collection-game-object-built-pb collection 0)))
              (scale-components! embedded-component referenced-component)
              (test-scaled-saved-pb (collection-game-object-saved-pb collection 0))
              (with-open [_ (test-util/build! collection)]
                (test-scaled-built-pb (collection-game-object-built-pb collection 0)))))

          (testing "Components in child game object embedded inside collection."
            (let [collection (project/get-resource-node project collection-resource)
                  game-object-instance (:node-id (test-util/outline collection [0]))
                  child-game-object-instance (test-util/add-embedded-game-object! app-view project collection game-object-instance)
                  child-game-object (test-util/to-game-object-id child-game-object-instance)
                  [embedded-component referenced-component] (make-components! child-game-object sprite-resource atlas-resource app-view)]
              (test-unscaled embedded-component referenced-component)
              (test-unscaled-saved-pb (collection-game-object-saved-pb collection 1))
              (with-open [_ (test-util/build! collection)]
                (test-unscaled-built-pb (collection-game-object-built-pb collection 1)))
              (scale-components! embedded-component referenced-component)
              (test-scaled-saved-pb (collection-game-object-saved-pb collection 1))
              (with-open [_ (test-util/build! collection)]
                (test-scaled-built-pb (collection-game-object-built-pb collection 1))))))))))
