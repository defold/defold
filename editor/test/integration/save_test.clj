(ns integration.save-test
  (:require [clojure.test :refer :all]
            [clojure.data :as data]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system]]
            [editor.defold-project :as project]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [integration.test-util :as test-util])
  (:import [java.io StringReader]
           [com.dynamo.gameobject.proto GameObject$PrototypeDesc GameObject$CollectionDesc]
           [com.dynamo.gui.proto Gui$SceneDesc]
           [com.dynamo.particle.proto Particle$ParticleFX]
           [com.dynamo.spine.proto Spine$SpineSceneDesc Spine$SpineScene Spine$SpineModelDesc Spine$SpineModelDesc$BlendMode]))

(def ^:private ext->proto {"go" GameObject$PrototypeDesc
                           "collection" GameObject$CollectionDesc
                           "gui" Gui$SceneDesc
                           "particlefx" Particle$ParticleFX
                           "spinescene" Spine$SpineSceneDesc
                           "spinemodel" Spine$SpineModelDesc})

(deftest save-all
  (let [queries ["**/level1.platformer"
                 "**/level01.switcher"
                 "**/env.cubemap"
                 "**/switcher.atlas"
                 "**/atlas_sprite.collection"
                 "**/props.collection"
                 "**/sub_props.collection"
                 "**/sub_sub_props.collection"
                 "**/atlas_sprite.go"
                 "**/atlas.sprite"
                 "**/props.go"
                 "game.project"
                 "**/super_scene.gui"
                 "**/scene.gui"
                 "**/fireworks_big.particlefx"
                 "**/new.collisionobject"
                 "**/three_shapes.collisionobject"
                 "**/tile_map_collision_shape.collisionobject"
                 "**/spineboy.spinescene"
                 "**/spineboy.spinemodel"
                 "**/new.factory"
                 "**/with_prototype.factory"
                 "**/new.sound"
                 "**/tink.sound"
                 "**/new.camera"
                 "**/non_default.camera"]]
    (with-clean-system
      (let [workspace (test-util/setup-workspace! world)
            project   (test-util/setup-project! workspace)
            save-data (group-by :resource (project/save-data project))]
        (doseq [query queries]
          (testing (format "Saving %s" query)
                   (let [[resource _] (first (project/find-resources project query))
                        save (first (get save-data resource))
                        file (slurp resource)
                        pb-class (-> resource resource/resource-type :ext ext->proto)]
                    (if pb-class
                      (let [pb-save (protobuf/read-text pb-class (StringReader. (:content save)))
                            pb-disk (protobuf/read-text pb-class resource)
                            path []
                            [disk save both] (data/diff (get-in pb-disk path) (get-in pb-save path))]
                        (is (nil? disk))
                        (is (nil? save)))
                      (is (= file (:content save)))))))))))

(deftest save-all-literal-equality
  (let [queries ["**/embedded_instances.collection"
                 "**/embedded_components.go"]]
    (with-clean-system
      (let [workspace (test-util/setup-workspace! world)
            project   (test-util/setup-project! workspace)
            save-data (group-by :resource (project/save-data project))]
        (doseq [query queries]
          (testing (format "Saving %s" query)
            (let [[resource _] (first (project/find-resources project query))
                  save (first (get save-data resource))
                  file (slurp resource)]
               (is (= file (:content save))))))))))
