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

(ns editor.protobuf-types-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.editor-extensions :as extensions]
            [editor.progress :as progress]
            [editor.resource :as resource]
            [integration.test-util :as test-util]
            [service.log :as log]
            [support.test-support :refer [with-clean-system]]))

(def ^:private project-path "test/resources/all_types_project")

(deftest test-load
  (with-clean-system
    (is (let [workspace (test-util/setup-workspace! world project-path)
              project (test-util/setup-project! workspace)]
          true))))

(def expected-dependencies
  {"/test.animationset" ["/test2.animationset"]
   "/test.atlas" ["/builtins/graphics/particle_blob.png"]
   "/test.camera" []
   "/test.collection" []
   "/test_embedded_gos.collection" ["/test.collection"
                                    "/main.collection"
                                    "/test.tilemap"
                                    "/test2.go"
                                    "/test.font"
                                    "/test.material"
                                    "/test.dae"
                                    "/builtins/materials/model.material"
                                    "/test.wav"
                                    "/test.atlas"
                                    "/builtins/materials/sprite.material"]
   "/test_embedded_gos_referenced_components.collection" ["/test.camera"
                                                          "/test.collectionfactory"
                                                          "/test.collectionproxy"
                                                          "/test.collisionobject"
                                                          "/test.factory"
                                                          "/test.gui"
                                                          "/test.label"
                                                          "/test.model"
                                                          "/test.particlefx"
                                                          "/test.script"
                                                          "/test.sound"
                                                          "/test.sprite"
                                                          "/test.tilemap"
                                                          "/test.wav"]
   "/test.collectionfactory" ["/test.collection"]
   "/test.collectionproxy" ["/test.collection"]   
   "/test.collisionobject" ["/test.tilemap"]
   "/test.cubemap" ["/builtins/graphics/particle_blob.png"]
   "/test.dae" []
   "/test.display_profiles" []
   "/test.factory" ["/test2.go"]
   "/test.font" ["/builtins/fonts/vera_mo_bd.ttf"
                 "/builtins/fonts/font.material"]
   "/test.fp" []
   "/test.gamepads" []
   "/test.go" ["/test.camera"
               "/test.collectionfactory"
               "/test.collectionproxy"
               "/test.collisionobject"
               "/test.factory"
               "/test.gui"
               "/test.label"
               "/test.model"
               "/test.particlefx"
               "/test.script"
               "/test.sound"
               "/test.sprite"
               "/test.tilemap"
               "/test.wav"]
   "/test2.go" []
   "/test_embedded_components.go" ["/test.collection"
                                   "/main.collection"
                                   "/test.tilemap"
                                   "/test2.go"
                                   "/test.font"
                                   "/test.material"
                                   "/test.dae"
                                   "/builtins/materials/model.material"
                                   "/test.wav"
                                   "/test.atlas"
                                   "/builtins/materials/sprite.material"]
   "/test.gui" ["/test.gui_script"
                "/test2.gui"
                "/test.atlas"
                "/test.particlefx"
                "/test.material"
                "/test.font"]
   "/test.gui_script" []
   "/test.input_binding" []
   "/test.json" []
   "/test.label" ["/test.font"
                  "/test.material"]
   "/test.lua" []
   "/test.material" ["/test.vp"
                     "/test.fp"]
   "/test.model" ["/test.dae"
                  "/test.material"
                  "/test.animationset"]
   "/test.particlefx" ["/test.tilesource"
                       "/test.material"]
   "/test.render" ["/test.render_script"
                   "/test.material"]
   "/test.render_script" []
   "/test.script" []
   "/test.sound" ["/test.wav"]
   "/test.sprite" ["/test.atlas"
                   "/test.material"]
   "/test.texture_profiles" []
   "/test.tilemap" ["/test.tilesource"
                    "/test.material"]
   "/test.tilesource" ["/builtins/graphics/particle_blob.png"]
   "/test.vp" []
   "/test.wav" []
   "/test2.animationset" []
   "/test2.gui" ["/test.material"]})

(defn fallback-dependencies-fn [resource-type]
  (when (#{"vp" "fp" "lua" "script" "gui_script" "wav" "json" "render_script" "dae"} (:ext resource-type))
    (constantly [])))

(deftest dependencies
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world project-path)
          project (test-util/setup-project! workspace)
          resource-nodes (g/node-value project :nodes-by-resource-path)]
      (doseq [[resource-path node-id] resource-nodes
              :when (.startsWith resource-path "/test")]
        (let [resource (g/node-value node-id :resource)
              resource-type (resource/resource-type resource)
              dependencies-fn (or (:dependencies-fn resource-type) (fallback-dependencies-fn resource-type))
              source-value (g/node-value node-id :source-value)]
          (is (some? dependencies-fn) (format "%s has no dependencies-fn" resource-path))
          (is (some? (expected-dependencies resource-path)) resource-path)
          (is (= (sort (expected-dependencies resource-path))
                 (sort (dependencies-fn source-value))) resource-path))))))

(deftest load-order-sanity
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world project-path)
          proj-graph (g/make-graph! :history true :volatility 1)
          extensions (extensions/make proj-graph)
          project (project/make-project proj-graph workspace extensions)]
      (let [node-id+resource-pairs
            (project/make-node-id+resource-pairs proj-graph (g/node-value project :resources))

            node-load-infos
            (project/read-nodes node-id+resource-pairs)

            load-order
            (into {}
                  (map-indexed (fn [node-index {:keys [resource]}]
                                 [(resource/proj-path resource) node-index]))
                  node-load-infos)]
        (doseq [[resource-path dependencies] expected-dependencies
                dependency dependencies]
          (is (< (load-order dependency) (load-order resource-path)) (format "%s before %s" dependency resource-path)))))))

(def non-broken-dependencies
  {"/broken_embedded_gos.collection" [] ; embedded instance broken, so no dependencies
   "/broken_embedded_components.go" [#_"/test.collection" ; clobbered data field
                                     "/main.collection"
                                     "/test.tilemap"
                                     "/test2.go"
                                     "/test.font"
                                     "/test.material"
                                     "/test.dae"
                                     "/builtins/materials/model.material"
                                     "/test.wav"
                                     "/test.atlas"
                                     "/builtins/materials/sprite.material"]})

(deftest broken-embedded-data-gives-no-dependencies
  (log/without-logging ; skip warnings about <<<<<<<< in game.project, BORK in go/collection
    (with-clean-system
      (let [workspace (test-util/setup-workspace! world "test/resources/broken_project")
            project (test-util/setup-project! workspace)
            resource-nodes (g/node-value project :nodes-by-resource-path)]
        (let [broken-go (resource-nodes "/broken_embedded_components.go")
              broken-collection (resource-nodes "/broken_embedded_gos.collection")]
          (doseq [node-id [broken-go broken-collection]]
            (let [resource (g/node-value node-id :resource)
                  resource-path (resource/resource->proj-path resource)
                  resource-type (resource/resource-type resource)
                  dependencies-fn (or (:dependencies-fn resource-type) (fallback-dependencies-fn resource-type))
                  source-value (g/node-value node-id :source-value)]
              (is (some? dependencies-fn) (format "%s has no dependencies-fn" resource-path))
              (is (= (sort (non-broken-dependencies resource-path))
                     (sort (dependencies-fn source-value)))
                  resource-path))))))))
