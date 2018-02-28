(ns editor.protobuf-types-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :refer [undo-stack write-until-new-mtime spit-until-new-mtime touch-until-new-mtime with-clean-system]]
            [editor.defold-project :as project]
            [editor.resource :as resource]
            [service.log :as log]
            [integration.test-util :as test-util]))

(def ^:private project-path "test/resources/all_types_project")

(deftest test-load
  (with-clean-system
    (is (let [workspace (test-util/setup-workspace! world project-path)
              project (test-util/setup-project! workspace)]
          true))))

(def expected-dependencies
  {
   "/test.animationset" ["/test2.animationset"]
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
                                    "/test.spinescene"
                                    "/builtins/materials/spine.material"
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
                                                          "/test.spinemodel"
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
               "/test.spinemodel"
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
                                   "/test.spinescene"
                                   "/builtins/materials/spine.material"
                                   "/test.atlas"
                                   "/builtins/materials/sprite.material"]
   "/test.gui" ["/test.gui_script"
                "/test2.gui"
                "/test.atlas"
                "/test.spinescene"
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
   "/test.spinemodel" ["/test.spinescene"
                       "/test.material"]
   "/test.spinescene" ["/test.json"
                       "/test.atlas"]
   "/test.sprite" ["/test.atlas"
                   "/test.material"]
   "/test.texture_profiles" []
   "/test.tilemap" ["/test.tilesource"
                    "/test.material"]
   "/test.tilesource" ["/builtins/graphics/particle_blob.png"]
   "/test.vp" []
   "/test.wav" []
   "/test2.animationset" []
   "/test2.gui" ["/test.material"]
   })

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
          (is (= (sort (dependencies-fn source-value))
                 (sort (expected-dependencies resource-path))) resource-path))))))

(deftest load-order-sanity
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world project-path)
          proj-graph (g/make-graph! :history true :volatility 1)
          project (project/make-project proj-graph workspace)]
      (with-bindings {#'project/*load-cache* (atom #{})}
        (let [nodes (#'project/make-nodes! project (g/node-value project :resources))
              loaded-nodes-by-resource-path (into {} (map (fn [node-id]
                                                            [(resource/proj-path (g/node-value node-id :resource)) node-id]))
                                                  nodes)
              evaluation-context (g/make-evaluation-context)
              node-deps (fn [node-id] (#'project/node-load-dependencies node-id (set nodes) loaded-nodes-by-resource-path {} evaluation-context))
              load-order (into {} (map-indexed
                                    (fn [ix node-id]
                                      [(resource/resource->proj-path (g/node-value node-id :resource)) ix])
                                    (#'project/sort-nodes-for-loading nodes node-deps)))]
          (doseq [[resource-path dependencies] expected-dependencies
                  dependency dependencies]
            (is (< (load-order dependency) (load-order resource-path)) (format "%s before %s" dependency resource-path))))))))

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
                                     "/test.spinescene"
                                     "/builtins/materials/spine.material"
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
              (is (not (g/error? source-value)))
              (is (= (sort (dependencies-fn source-value))
                     (sort (non-broken-dependencies resource-path))) resource-path))))))))
