;; Copyright 2020-2025 The Defold Foundation
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

(ns editor.pipeline.texture-set-gen
  (:require [dynamo.graph :as g]
            [editor.image-util :as image-util]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-io :as resource-io]
            [util.coll :refer [pair]])
  (:import [com.dynamo.bob.pipeline AtlasUtil]
           [com.dynamo.bob.textureset TextureSetGenerator TextureSetGenerator$AnimDesc TextureSetGenerator$AnimIterator TextureSetGenerator$LayoutResult TextureSetGenerator$TextureSetResult TextureSetLayout$Grid TextureSetLayout$Layout TextureSetLayout$Rect]
           [com.dynamo.bob.tile ConvexHull TileSetUtil TileSetUtil$Metrics]
           [com.dynamo.bob.util TextureUtil]
           [com.dynamo.gamesys.proto TextureSetProto$TextureSet$Builder]
           [com.dynamo.gamesys.proto TextureSetProto$SpriteGeometry Tile$Animation Tile$ConvexHull Tile$Playback Tile$SpriteTrimmingMode]
           [editor.types Image]
           [java.awt.image BufferedImage]))

(set! *warn-on-reflection* true)

(defn- anim->AnimDesc [anim]
  (when anim
    (TextureSetGenerator$AnimDesc. (:id anim) (protobuf/val->pb-enum Tile$Playback (:playback anim)) (:fps anim)
                                   (:flip-horizontal anim) (:flip-vertical anim))))

(defn- Rect->map
  [^TextureSetLayout$Rect rect]
  {:path (.getId rect)
   :index (.getIndex rect)
   :x (.getX rect)
   :y (.getY rect)
   :page (.getPage rect)
   :width (.getWidth rect)
   :height (.getHeight rect)
   :pivot (.getPivot rect)
   :rotated (.getRotated rect)})

(defn- Metrics->map
  [^TileSetUtil$Metrics metrics]
  (when metrics
    {:tiles-per-row (.tilesPerRow metrics)
     :tiles-per-column (.tilesPerColumn metrics)
     :tile-set-width (.tileSetWidth metrics)
     :tile-set-height (.tileSetHeight metrics)
     :visual-width (.visualWidth metrics)
     :visual-height (.visualHeight metrics)}))

(defn- get-all-rects [layouts]
  (let [rect-list (map (fn [^TextureSetLayout$Layout layout] (.getRectangles layout)) layouts)
        rect-list-array (mapcat identity rect-list)]
    (mapv Rect->map rect-list-array)))

(defn- TextureSetResult->result
  [^TextureSetGenerator$TextureSetResult tex-set-result]
  (let [layouts (.. tex-set-result layoutResult layouts)
        ^TextureSetLayout$Layout layout-first (.get layouts 0)
        all-rects (get-all-rects layouts)]
    {:texture-set (protobuf/pb->map-with-defaults (.build (.builder tex-set-result)))
     :uv-transforms (vec (.uvTransforms tex-set-result))
     :layout (.layoutResult tex-set-result)
     :size [(.getWidth layout-first) (.getHeight layout-first)]
     :rects all-rects}))

(defn layout-atlas-pages
  [^TextureSetGenerator$LayoutResult layout-result id->image]
  (let [inner-padding (.-innerPadding layout-result)
        extrude-borders (.-extrudeBorders layout-result)]
    (mapv (fn [^TextureSetLayout$Layout layout]
            (TextureSetGenerator/layoutImages layout inner-padding extrude-borders id->image))
          (.-layouts layout-result))))

(def sprite-trim-mode-edit-type
  ;; Excludes runtime-only values from the selectable options.
  (let [selectable-value? (complement #{:sprite-trim-polygons})]
    {:type :choicebox
     :options (into []
                    (keep (fn [[value opts]]
                            (when (selectable-value? value)
                              (pair value (:display-name opts)))))
                    (protobuf/enum-values Tile$SpriteTrimmingMode))}))

(defn- sprite-trim-mode->enum
  [sprite-trim-mode]
  (protobuf/val->pb-enum Tile$SpriteTrimmingMode sprite-trim-mode))

(defn resource-id
  ([resource rename-patterns]
   (resource-id resource nil rename-patterns))
  ([resource animation-name rename-patterns]
   (let [id (cond->> (resource/base-name resource) animation-name (str animation-name "/"))]
     (if rename-patterns
       (try (AtlasUtil/replaceStrings rename-patterns id) (catch Exception _ id))
       id))))

(defn- texture-set-layout-rect
  ^TextureSetLayout$Rect [{:keys [path width height]}]
  (let [id (resource/proj-path path)]
    (TextureSetLayout$Rect. id -1 (int width) (int height))))

(defonce ^:private ^TextureSetProto$SpriteGeometry rect-sprite-geometry-template
  (let [points (vector-of :float -0.5 -0.5 -0.5 0.5 0.5 0.5 0.5 -0.5)
        uvs (vector-of :float 0.0 0.0 0.0 0.0 0.0 0.0 0.0 0.0)
        indices (vector-of :int 0 1 2 0 2 3)]
    (-> (TextureSetProto$SpriteGeometry/newBuilder)
        (.addAllVertices points)
        (.addAllUvs uvs)
        (.addAllIndices indices)
        (.buildPartial))))

(defn- make-rect-sprite-geometry
  ^TextureSetProto$SpriteGeometry [^long width ^long height ^double pivot-x ^double pivot-y]
  (-> (TextureSetProto$SpriteGeometry/newBuilder rect-sprite-geometry-template)
      (.setWidth width)
      (.setHeight height)
      (.setCenterX 0)
      (.setCenterY 0)
      (.setPivotX pivot-x)
      (.setPivotY pivot-y)
      (.setRotated false)
      (.setTrimMode Tile$SpriteTrimmingMode/SPRITE_TRIM_MODE_OFF)
      (.build)))

(defn- make-image-sprite-geometry
  ^TextureSetProto$SpriteGeometry [^Image image]
  (let [error-node-id (:error-node-id (meta image))
        resource (.path image)
        sprite-trim-mode (.sprite-trim-mode image)
        ; The SpriteGeometry defined (0,0) as being the center of the image
        pivot-x (- (.pivot-x image) 0.5)
        pivot-y (- (.pivot-y image) 0.5)
        ; Flip Y, as the SpriteGeometry uses positive Y as up for vertices
        pivot-y (- pivot-y)]
    (assert (g/node-id? error-node-id))
    (assert (resource/resource? resource))
    (case sprite-trim-mode
      :sprite-trim-mode-off (make-rect-sprite-geometry (.width image) (.height image) pivot-x pivot-y)
      (let [buffered-image (resource-io/with-error-translation resource error-node-id :image
                             (image-util/read-image resource))]
        (g/precluding-errors buffered-image
          (TextureSetGenerator/buildConvexHull buffered-image pivot-x pivot-y (sprite-trim-mode->enum sprite-trim-mode)))))))

(defn atlas->texture-set-data
  [animations images margin inner-padding extrude-borders max-page-size]
  ;; NOTE: Images order matters when generating the layouts, especially if they are of the same size.
  ;; Since the SHA1 for the packed-page-images-generator is insensitive to order, things could get out of sync
  (let [images (sort-by #(-> % :path resource/proj-path) images)
        sprite-geometries (mapv make-image-sprite-geometry images)]
    (g/precluding-errors sprite-geometries
      (let [img-to-index (into {}
                               (map-indexed #(pair %2 (Integer/valueOf ^int %1)))
                               images)
            anims-atom (atom animations)
            anim-imgs-atom (atom [])
            anim-iterator (reify TextureSetGenerator$AnimIterator
                            (nextAnim [_this]
                              (let [anim (first @anims-atom)]
                                (reset! anim-imgs-atom (or (:images anim) []))
                                (swap! anims-atom rest)
                                (anim->AnimDesc anim)))
                            (nextFrameIndex [_this]
                              (let [img (first @anim-imgs-atom)]
                                (swap! anim-imgs-atom rest)
                                (img-to-index img)))
                            ; This generator is run with fake animation (i.e. no valid id's)
                            ; See atlas.clj produce-texture-set-data for the patchup details
                            (getFrameId [_this] "")
                            (rewind [_this]
                              (reset! anims-atom animations)
                              (reset! anim-imgs-atom [])))
            rects (mapv texture-set-layout-rect images)
            use-geometries (if (every? #(= :sprite-trim-mode-off (:sprite-trim-mode %)) images) 0 1)
            result (TextureSetGenerator/calculateLayout
                     rects sprite-geometries use-geometries anim-iterator margin inner-padding extrude-borders
                     true false nil (get max-page-size 0) (get max-page-size 1))]
        (doto (.builder result)
          (.setTexture "unknown"))
        (TextureSetResult->result result)))))

(defn- calc-tile-start [{:keys [spacing margin]} size tile-index]
  (let [actual-tile-size (+ size spacing (* 2 margin))]
    (+ margin (* tile-index actual-tile-size))))

(defn- sub-image [tile-source-attributes tile-x tile-y image type]
  (let [w (:width tile-source-attributes)
        h (:height tile-source-attributes)
        tgt (BufferedImage. w h type)
        g (.getGraphics tgt)
        sx (calc-tile-start tile-source-attributes w tile-x)
        sy (calc-tile-start tile-source-attributes h tile-y)]
    (.drawImage g image 0 0 w h sx sy (+ sx w) (+ sy h) nil)
    (.dispose g)
    tgt))

(defn- split-image
  [image {:keys [tiles-per-column tiles-per-row] :as tile-source-attributes}]
  (let [type (TextureUtil/getImageType image)]
    (for [tile-y (range tiles-per-column)
          tile-x (range tiles-per-row)]
      (sub-image tile-source-attributes tile-x tile-y image type))))

(defn- split-rects
  [{:keys [width height tiles-per-column tiles-per-row] :as _tile-source-attributes}]
  (for [tile-y (range tiles-per-column)
        tile-x (range tiles-per-row)
        :let [index (+ tile-x (* tile-y tiles-per-row))
              name (format "tile%d" index)]]
    (TextureSetLayout$Rect. name
                            index
                            (* tile-x width)
                            (* tile-y height)
                            (int width)
                            (int height))))

(def ^:private default-tile-animation-playback (protobuf/default Tile$Animation :playback))
(def ^:private default-tile-animation-fps (protobuf/default Tile$Animation :fps))
(def ^:private default-tile-animation-flip-horizontal (protobuf/default Tile$Animation :flip-horizontal))
(def ^:private default-tile-animation-flip-vertical (protobuf/default Tile$Animation :flip-vertical))

(defn- tile-animation->AnimDesc
  ^TextureSetGenerator$AnimDesc [tile-animation]
  ;; Tile$Animation in map format.
  (when tile-animation
    (TextureSetGenerator$AnimDesc.
      (:id tile-animation) ; Required protobuf field.
      (protobuf/val->pb-enum Tile$Playback (:playback tile-animation default-tile-animation-playback))
      (:fps tile-animation default-tile-animation-fps)
      (not= 0 (:flip-horizontal tile-animation default-tile-animation-flip-horizontal))
      (not= 0 (:flip-vertical tile-animation default-tile-animation-flip-vertical)))))

(defn calculate-convex-hulls
  [^BufferedImage collision {:keys [width height margin spacing] :as _tile-properties}]
  (let [convex-hulls (TileSetUtil/calculateConvexHulls (.getAlphaRaster collision) 16 (.getWidth collision) (.getHeight collision)
                                                       width height margin spacing)
        points (vec (.points convex-hulls))]
    (mapv (fn [^ConvexHull hull]
            (let [index (.getIndex hull)
                  count (.getCount hull)]
              {:index index
               :count count
               :points (subvec points (* 2 index) (+ (* 2 index) (* 2 count)))}))
          (.hulls convex-hulls))))

(defn- metrics-rect [{:keys [width height]}]
  ;; The other attributes do not matter for metrics calculation.
  (TextureSetLayout$Rect. nil -1 (int width) (int height)))

(defn calculate-tile-metrics
  [image-size {:keys [width height margin spacing] :as _tile-properties} collision-size]
  (let [image-size-rect (metrics-rect image-size)
        collision-size-rect (when collision-size
                              (metrics-rect collision-size))
        metrics (TileSetUtil/calculateMetrics image-size-rect width height margin spacing collision-size-rect 1 0)]
    (Metrics->map metrics)))

(defn- add-collision-hulls!
  [^TextureSetProto$TextureSet$Builder builder convex-hulls collision-groups]
  (.addAllCollisionGroups builder collision-groups)
  (when convex-hulls
    (run! (fn [{:keys [index count points collision-group]}]
            (.addConvexHulls builder (doto (Tile$ConvexHull/newBuilder)
                                       (.setIndex index)
                                       (.setCount count)
                                       (.setCollisionGroup (or collision-group ""))))

            (run! #(.addCollisionHullPoints builder %) points))
          convex-hulls)))

(defn tile-source->texture-set-data [layout-result tile-source-attributes ^BufferedImage buffered-image convex-hulls collision-groups animations]
  (let [image-rects (split-rects tile-source-attributes)
        anims-atom (atom animations)
        anim-indices-atom (atom [])
        anim-iterator (reify TextureSetGenerator$AnimIterator
                        (nextAnim [_this]
                          (let [anim (first @anims-atom)]
                            (reset! anim-indices-atom (if anim
                                                        (vec (map int (range (dec (:start-tile anim)) (:end-tile anim))))
                                                        []))
                            (swap! anims-atom rest)
                            (tile-animation->AnimDesc anim)))
                        (nextFrameIndex [_this]
                          (let [index (first @anim-indices-atom)]
                            (swap! anim-indices-atom rest)
                            index))
                        ; The tilesets don't support lookup by image name hash, so we default to ""==0
                        (getFrameId [_this] "")
                        (rewind [_this]
                          (reset! anims-atom animations)
                          (reset! anim-indices-atom [])))
        sprite-trim-mode (sprite-trim-mode->enum (:sprite-trim-mode tile-source-attributes))
        sprite-geometries (map (fn [^TextureSetLayout$Rect image-rect]
                                 (let [sub-image (.getSubimage buffered-image (.getX image-rect) (.getY image-rect) (.getWidth image-rect) (.getHeight image-rect))]
                                   (TextureSetGenerator/buildConvexHull sub-image 0.0 0.0 sprite-trim-mode)))
                               image-rects)
        use-geometries (if (not= :sprite-trim-mode-off (:sprite-trim-mode tile-source-attributes)) 1 0)
        result (TextureSetGenerator/calculateTextureSetResult
                 layout-result
                 sprite-geometries
                 use-geometries
                 anim-iterator)]
    (doto (.builder result)
      (.setTileWidth (:width tile-source-attributes))
      (.setTileHeight (:height tile-source-attributes))
      (add-collision-hulls! convex-hulls collision-groups)
      ;; "This will be supplied later when producing the byte data for the pipeline"
      ;; TODO: check what that means and if it's true
      (.setTexture "unknown"))
    (TextureSetResult->result result)))

(defn layout-tile-source
  ^BufferedImage [^TextureSetGenerator$LayoutResult layout-result ^BufferedImage image tile-source-attributes]
  (let [layout (first (.-layouts layout-result))
        inner-padding (.-innerPadding layout-result)
        extrude-borders (.-extrudeBorders layout-result)
        id->image (zipmap (map (fn [x] (format "tile%d" x)) (range)) (split-image image tile-source-attributes))]
    (TextureSetGenerator/layoutImages layout inner-padding extrude-borders id->image)))

(defn calculate-layout-result
  [tile-source-attributes]
  (let [image-rects (split-rects tile-source-attributes)
        grid (TextureSetLayout$Grid. (:tiles-per-row tile-source-attributes)
                                     (:tiles-per-column tile-source-attributes))]
    (TextureSetGenerator/calculateLayoutResult
     image-rects
     (:margin tile-source-attributes)
     (:inner-padding tile-source-attributes)
     (:extrude-borders tile-source-attributes)
     false true grid 0.0 0.0)))
