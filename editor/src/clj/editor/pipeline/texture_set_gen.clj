(ns editor.pipeline.texture-set-gen
  (:require [editor.protobuf :as protobuf])
  (:import [com.defold.editor.pipeline TextureSetGenerator TextureSetGenerator$AnimIterator TextureSetGenerator$AnimDesc
            TextureSetGenerator$UVTransform TextureSetGenerator$TextureSetResult TileSetUtil TileSetUtil$Metrics TextureUtil
            TextureSetLayout$Grid ConvexHull]
           [com.dynamo.tile.proto Tile$Playback Tile$TileSet Tile$ConvexHull]
           [com.dynamo.textureset.proto TextureSetProto$TextureSet$Builder]
           [java.util ArrayList]
           [java.awt.image BufferedImage]))

(set! *warn-on-reflection* true)

(defn- anim->AnimDesc [anim]
  (when anim
    (TextureSetGenerator$AnimDesc. (:id anim) (protobuf/val->pb-enum Tile$Playback (:playback anim)) (:fps anim)
               (:flip-horizontal anim) (:flip-vertical anim))))

(defn- TextureSetResult->result [^TextureSetGenerator$TextureSetResult tex-set-result]
  {:texture-set (protobuf/pb->map (.build (.builder tex-set-result)))
   :image (.image tex-set-result)
   :uv-transforms (vec (.uvTransforms tex-set-result))})

(defn ->texture-set-data [animations images margin inner-padding extrude-borders]
  (let [img-to-index (into {} (map-indexed #(do [%2 (Integer. ^int %1)]) images))
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
                 (ArrayList. ^java.util.Collection (map :contents images)) anim-iterator margin inner-padding extrude-borders
                 false false true false nil)]
    ; This will be supplied later when producing the byte data for the pipeline
    (.setTexture (.builder result) "unknown")
    (TextureSetResult->result result)))

(defn- calc-tile-start [tile size tile-index]
  (let [actual-tile-size (+ size (:spacing tile) (* 2 (:margin tile)))]
    (+ (:margin tile) (* tile-index actual-tile-size))))

(defn- sub-image [tile tile-x tile-y image type]
  (let [w (:width tile)
        h (:height tile)
        tgt (BufferedImage. w h type)
        g (.getGraphics tgt)
        sx (calc-tile-start tile w tile-x)
        sy (calc-tile-start tile h tile-y)]
    (.drawImage g image 0 0 w h sx sy (+ sx w) (+ sy h) nil)
    (.dispose g)
    tgt))

(defn- split [image tile ^TileSetUtil$Metrics metrics]
  (let [type (TextureUtil/getImageType image)]
    (for [tile-y (range (.tilesPerRow metrics))
          tile-x (range (.tilesPerColumn metrics))]
      (sub-image tile tile-x tile-y image type))))

(defn- tile-anim->AnimDesc [anim]
  (when anim
    (TextureSetGenerator$AnimDesc. (:id anim) (protobuf/val->pb-enum Tile$Playback (:playback anim)) (:fps anim)
               (not= 0 (:flip-horizontal anim)) (not= 0 (:flip-vertical anim)))))

(defn- build-convex-hulls! [tile-source ^BufferedImage collision ^TextureSetProto$TextureSet$Builder builder]
  (.addAllCollisionGroups builder (:collision-groups tile-source))
  (when collision
    (let [hulls (TileSetUtil/calculateConvexHulls (.getAlphaRaster collision) 16 (.getWidth collision) (.getHeight collision)
                                                  (:tile-width tile-source) (:tile-height tile-source) (:tile-margin tile-source) (:tile-spacing tile-source))
          items (map-indexed (fn [i hull] [hull (:collision-group (get (:convex-hulls tile-source) i) "")]) (.hulls hulls))]
      (doseq [[^ConvexHull hull group] items
              :let [hull-builder (Tile$ConvexHull/newBuilder)]]
        (-> hull-builder
          (.setCollisionGroup group)
          (.setCount (.getCount hull))
          (.setIndex (.getIndex hull)))
        (.addConvexHulls builder hull-builder))
      (doseq [^float p (.points hulls)]
        (.addConvexHullPoints builder p)))))

(defn tile-source->texture-set-data [tile-source image collision]
  (let [tile {:width (:tile-width tile-source)
              :height (:tile-height tile-source)
              :margin (:tile-margin tile-source)
              :spacing (:tile-spacing tile-source)}
        metrics (TileSetUtil/calculateMetrics image (:width tile) (:height tile) (:margin tile) (:spacing tile) collision 1 0)
        images (split image tile metrics)
        anims-atom (atom (:animations tile-source))
        anim-indices-atom (atom [])
        anim-iterator (reify TextureSetGenerator$AnimIterator
                        (nextAnim [this]
                          (let [anim (first @anims-atom)]
                            (reset! anim-indices-atom (if anim
                                                        (vec (map int (range (dec (:start-tile anim)) (:end-tile anim))))
                                                        []))
                            (swap! anims-atom rest)
                            (tile-anim->AnimDesc anim)))
                        (nextFrameIndex [this]
                          (let [index (first @anim-indices-atom)]
                            (swap! anim-indices-atom rest)
                            index))
                        (rewind [this]
                          (reset! anims-atom (:animations tile-source))
                          (reset! anim-indices-atom [])))
        grid (TextureSetLayout$Grid. (.tilesPerRow metrics) (.tilesPerColumn metrics))
        result (TextureSetGenerator/generate
                 (ArrayList. ^java.util.Collection images) anim-iterator (:tile-margin tile-source) (:inner-padding tile-source)
                 (:extrude-borders tile-source) false false false true grid)]
    (-> (.builder result)
      (.setTileWidth (:width tile))
      (.setTileHeight (:height tile)))
    (build-convex-hulls! tile-source collision (.builder result))
    ; This will be supplied later when producing the byte data for the pipeline
    (.setTexture (.builder result) "unknown")
    (TextureSetResult->result result)))
