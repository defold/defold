(ns editor.pipeline.texture-set-gen
  (:require [dynamo.file.protobuf :as protobuf])
  (:import [com.defold.editor.pipeline TextureSetGenerator TextureSetGenerator$AnimIterator TextureSetGenerator$AnimDesc]
           [com.dynamo.tile.proto Tile$Playback]
           [java.util ArrayList]))

(defn- anim->AnimDesc [anim]
  (when anim
    (TextureSetGenerator$AnimDesc. (:id anim) (protobuf/val->pb-enum Tile$Playback (:playback anim)) (:fps anim)
               (boolean (:flip-horizontal anim)) (boolean (:flip-vertical anim)))))

(defn- UVTransform->uv-transform [t]
  {:translation (.translation t)
   :rotated (.rotated t)
   :scale (.scale t)})

(defn- TextureSetResult->result [tex-set-result]
  {:texture-set (protobuf/pb->map (.build (.builder tex-set-result)))
   :image (.image tex-set-result)
   :uv-transforms (map UVTransform->uv-transform (.uvTransforms tex-set-result))})

(defn ->texture-set-data [animations images margin inner-padding extrude-borders]
  (let [img-to-index (into {} (map-indexed #(do [%2 (Integer. %1)]) images))
        anims-atom (atom animations)
        anim-imgs-atom (atom [])
        anim-iterator (reify TextureSetGenerator$AnimIterator
                        (nextAnim [this]
                          (let [anim (first @anims-atom)]
                            (reset! anim-imgs-atom (or (:images anim) []))
                            (swap! anims-atom rest)
                            (anim->AnimDesc anim)))
                        (nextFrameIndex [this]
                          (let [img (first @anim-imgs-atom)]
                            (swap! anim-imgs-atom rest)
                            (img-to-index img)))
                        (rewind [this]
                          (reset! anims-atom animations)
                          (reset! anim-imgs-atom [])))
        result (TextureSetGenerator/generate
                 (ArrayList. (map :contents images)) anim-iterator margin inner-padding extrude-borders
                 false false true false nil)]
    ; This will be supplied later when producing the byte data for the pipeline
    (.setTexture (.builder result) "unknown")
    (TextureSetResult->result result)))
