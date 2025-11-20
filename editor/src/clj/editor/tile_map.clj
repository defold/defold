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

(ns editor.tile-map
  (:require [clojure.data.int-map :as int-map]
            [dynamo.graph :as g]
            [editor.attachment :as attachment]
            [editor.build-target :as bt]
            [editor.core :as core]
            [editor.defold-project :as project]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.pass :as pass]
            [editor.gl.shader :as shader]
            [editor.gl.texture :as texture]
            [editor.gl.vertex2 :as vtx]
            [editor.graph-util :as gu]
            [editor.grid :as grid]
            [editor.handler :as handler]
            [editor.id :as id]
            [editor.localization :as localization]
            [editor.material :as material]
            [editor.math :as math]
            [editor.outline :as outline]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.scene :as scene]
            [editor.scene-picking :as scene-picking]
            [editor.tile-map-common :as tile-map-common]
            [editor.tile-source :as tile-source]
            [editor.validation :as validation]
            [editor.workspace :as workspace])
  (:import [com.dynamo.gamesys.proto Tile$TileCell Tile$TileGrid Tile$TileGrid$BlendMode Tile$TileLayer]
           [com.jogamp.opengl GL2]
           [editor.gl.shader ShaderLifecycle]
           [editor.tile_map_common Tile]
           [editor.types AABB]
           [javax.vecmath Matrix4d Point3d Vector3d]))

(set! *warn-on-reflection* true)

(defn- min-l
  ^long [^long a ^long b]
  (Math/min a b))

(defn- max-l
  ^long [^long a ^long b]
  (Math/max a b))

(defn- max-d
  ^double [^double a ^double b]
  (Math/max a b))

(defn- floor-d
  ^double [^double n]
  (Math/floor n))

(defn single
  [coll]
  (when-not (next coll) (first coll)))

(defn toggler
  [a b]
  (fn [v] (if (= v a) b a)))


(def tile-map-icon "icons/32/Icons_48-Tilemap.png")
(def tile-map-layer-icon "icons/32/Icons_42-Layers.png")


;; manipulating cells

(defn paint-cell!
  [cell-map x y tile h-flip v-flip rotate90]
  (if tile
    (assoc! cell-map (tile-map-common/cell-index x y) (tile-map-common/->Tile x y tile h-flip v-flip rotate90))
    (dissoc! cell-map (tile-map-common/cell-index x y))))

(defn make-cell-map
  [cells]
  (persistent! (reduce (fn [ret {:keys [x y tile h-flip v-flip rotate90] :or {h-flip 0 v-flip 0 rotate90 0} :as _tile-cell}]
                         (paint-cell! ret x y tile (not= 0 h-flip) (not= 0 v-flip) (not= 0 rotate90)))
                       (transient (int-map/int-map))
                       cells)))

(defn paint
  [cell-map [sx sy] brush]
  (let [{:keys [width height tiles]} brush]
    (let [ex (+ sx width)
          ey (+ sy height)]
      (loop [x sx
             y sy
             tiles tiles
             cell-map (transient cell-map)]
        (if (< y ey)
          (if (< x ex)
            (let [{:keys [tile h-flip v-flip rotate90]} (first tiles)]
              (recur (inc x) y (rest tiles) (paint-cell! cell-map x y tile h-flip v-flip rotate90)))
            (recur sx (inc y) tiles cell-map))
          (persistent! cell-map))))))

(defn erase-area
  [cell-map [sx sy] [ex ey]]
  (let [x0 (min sx ex)
        y0 (min sy ey)
        x1 (inc (max sx ex))
        y1 (inc (max sy ey))]
    (loop [x x0
           y y0
           cell-map (transient cell-map)]
      (if (< y y1)
        (if (< x x1)
          (recur (inc x) y (dissoc! cell-map (tile-map-common/cell-index x y)))
          (recur x0 (inc y) cell-map))
        (persistent! cell-map)))))

(defn make-brush
  [tile]
  (if-not tile
    {:width 1 :height 1 :tiles [nil]}
    {:width 1 :height 1 :tiles [{:tile tile :h-flip false :v-flip false :rotate90 false}]}))

(defn make-brush-from-selection
  [cell-map start-cell end-cell]
  (let [[sx sy] start-cell
        [ex ey] end-cell
        x0 (min-l sx ex)
        y0 (min-l sy ey)
        x1 (inc (max-l sx ex))
        y1 (inc (max-l sy ey))
        w (- x1 x0)
        h (- y1 y0)]
    {:width w
     :height h
     :tiles (vec (for [y (range y0 y1)
                       x (range x0 x1)]
                   (get cell-map (tile-map-common/cell-index x y))))}))

(defn palette-x [n tiles-per-row]
  (mod n tiles-per-row))

(defn palette-y [n tiles-per-row]
  (int (/ n tiles-per-row)))

(defn make-brush-from-selection-in-palette
  [start-tile end-tile tile-source-attributes]
  (let [tiles-per-row (:tiles-per-row tile-source-attributes)
        start-x (palette-x start-tile tiles-per-row)
        start-y (palette-y start-tile tiles-per-row)
        end-x (palette-x end-tile tiles-per-row)
        end-y (palette-y end-tile tiles-per-row)
        x0 (min-l start-x end-x)
        y0 (min-l start-y end-y)
        x1 (inc (max-l start-x end-x))
        y1 (inc (max-l start-y end-y))
        width (- x1 x0)
        height (- y1 y0)
        tiles (vec (for [y (range (dec y1) (dec y0) -1)
                         x (range x0 x1)]
                     {:tile (+ x (* y tiles-per-row))
                      :h-flip false
                      :v-flip false
                      :rotate90 false}))]
    {:width width
     :height height
     :tiles tiles}))

(def ^:private empty-brush (make-brush nil))

(defn flip-brush-horizontally
  [brush]
  (let [width (:width brush)
        height (:height brush)
        tiles (:tiles brush)
        ;; Horizontal Flip Mappings
        ;; | current-state      | hflip | vflip | rotate90 |
        ;; | 0 (000) - 0°       | false | false | false    | -> 1 (001) - H
        ;; | 1 (001) - H        | true  | false | false    | -> 0 (000) - R_0
        ;; | 2 (010) - V        | false | true  | false    | -> 3 (011) - H + V
        ;; | 3 (011) - H+V      | true  | true  | false    | -> 2 (010) - V
        ;; | 4 (100) - 90°      | false | false | true     | -> 6 (110) - V + R_90
        ;; | 5 (101) - H+90°    | true  | false | true     | -> 7 (111) - H + V + R_90
        ;; | 6 (110) - V+90°    | false | true  | true     | -> 4 (100) - R_90
        ;; | 7 (111) - H+V+90°  | true  | true  | true     | -> 5 (101) - H + R_90
        flipped-tiles (vec (for [y (range height)
                                 x (range width)]
                             (let [index (+ (* y width) x)
                                   tile (nth tiles index)
                                   hflip (:h-flip tile)
                                   vflip (:v-flip tile)
                                   rotate90 (:rotate90 tile)
                                   current-state (bit-or (if hflip 1 0)
                                                         (if vflip 2 0)
                                                         (if rotate90 4 0))
                                   new-state (case current-state
                                               0 1          ; R_0 -> H
                                               1 0          ; H -> R_0
                                               2 3          ; V -> H + V
                                               3 2          ; H + V -> V
                                               4 6          ; R_90 -> V + R_90
                                               5 7          ; H + R_90 -> H + V + R_90
                                               6 4          ; V + R_90 -> R_90
                                               7 5)         ; H + V + R_90 -> H + R_90
                                   new-hflip (bit-test new-state 0)
                                   new-vflip (bit-test new-state 1)
                                   new-rotate90 (bit-test new-state 2)]
                               {:x (:x tile)
                                :y (:y tile)
                                :tile (:tile tile)
                                :h-flip new-hflip
                                :v-flip new-vflip
                                :rotate90 new-rotate90})))
        reordered-tiles (vec (flatten (for [y (range height)]
                                        (rseq (subvec flipped-tiles (* y width) (* (inc y) width))))))]
    {:width width
     :height height
     :tiles reordered-tiles}))

(defn flip-brush-vertically
  [brush]
  (let [width (:width brush)
        height (:height brush)
        tiles (:tiles brush)
        ;; Vertical Flip Mappings
        ;; | current-state      | hflip | vflip | rotate90 |
        ;; | 0 (000) - 0°       | false | false | false    | -> 2 (010) - V
        ;; | 1 (001) - H        | true  | false | false    | -> 3 (011) - H + V
        ;; | 2 (010) - V        | false | true  | false    | -> 0 (000) - R_0
        ;; | 3 (011) - H+V      | true  | true  | false    | -> 1 (001) - H
        ;; | 4 (100) - 90°      | false | false | true     | -> 5 (101) - H + R_90
        ;; | 5 (101) - H+90°    | true  | false | true     | -> 4 (100) - R_90
        ;; | 6 (110) - V+90°    | false | true  | true     | -> 7 (111) - H + V + R_90
        ;; | 7 (111) - H+V+90°  | true  | true  | true     | -> 6 (110) - V + R_90
        flipped-tiles (vec (for [y (range height)
                                 x (range width)]
                             (let [index (+ (* y width) x)
                                   tile (nth tiles index)
                                   hflip (:h-flip tile)
                                   vflip (:v-flip tile)
                                   rotate90 (:rotate90 tile)
                                   current-state (bit-or (if hflip 1 0)
                                                         (if vflip 2 0)
                                                         (if rotate90 4 0))
                                   new-state (case current-state
                                               0 2          ; R_0 -> V
                                               1 3          ; H -> H + V
                                               2 0          ; V -> R_0
                                               3 1          ; H + V -> H
                                               4 5          ; R_90 -> H + R_90
                                               5 4          ; H + R_90 -> R_90
                                               6 7          ; V + R_90 -> H + V + R_90
                                               7 6)         ; H + V + R_90 -> V + R_90
                                   new-hflip (bit-test new-state 0)
                                   new-vflip (bit-test new-state 1)
                                   new-rotate90 (bit-test new-state 2)]
                               {:x (:x tile)
                                :y (:y tile)
                                :tile (:tile tile)
                                :h-flip new-hflip
                                :v-flip new-vflip
                                :rotate90 new-rotate90})))
        reordered-tiles (vec (flatten (reverse (partition width flipped-tiles))))]
    {:width width
     :height height
     :tiles reordered-tiles}))

(defn rotate-brush-90-degrees
  [brush]
  (let [width (:width brush)
        height (:height brush)
        tiles (:tiles brush)
        ;; 90-degree Rotation Mappings
        ;; | current-state              | hflip | vflip | rotate90 |
        ;; | 0 (000) - 0°               | false | false | false    | -> 4 (100) - R_90
        ;; | 1 (001) - H                | true  | false | false    | -> 5 (101) - H + R_90
        ;; | 2 (010) - V                | false | true  | false    | -> 6 (110) - V + R_90
        ;; | 3 (011) - H+V              | true  | true  | false    | -> 7 (111) - H + V + R_90
        ;; | 4 (100) - 90°              | false | false | true     | -> 3 (011) - H + V (180-degree rotation)
        ;; | 5 (101) - H+90°            | true  | false | true     | -> 2 (010) - V
        ;; | 6 (110) - V+90°            | false | true  | true     | -> 1 (001) - H
        ;; | 7 (111) - H+V+90°          | true  | true  | true     | -> 0 (000) - R_0
        rotated-tiles (mapv (fn [tile]
                              (let [hflip (:h-flip tile)
                                    vflip (:v-flip tile)
                                    rotate90 (:rotate90 tile)
                                    current-state (bit-or (if hflip 1 0)
                                                          (if vflip 2 0)
                                                          (if rotate90 4 0))
                                    new-state (case current-state
                                                0 4         ; R_0 -> R_90
                                                1 5         ; H -> H + R_90
                                                2 6         ; V -> V + R_90
                                                3 7         ; H + V -> H + V + R_90
                                                4 3         ; R_90 -> H + V (180-degree rotation)
                                                5 2         ; H + R_90 -> V
                                                6 1         ; V + R_90 -> H
                                                7 0)        ; H + V + R_90 -> R_0
                                    new-hflip (bit-test new-state 0)
                                    new-vflip (bit-test new-state 1)
                                    new-rotate90 (bit-test new-state 2)]
                                {:x (:x tile)
                                 :y (:y tile)
                                 :tile (:tile tile)
                                 :h-flip new-hflip
                                 :v-flip new-vflip
                                 :rotate90 new-rotate90}))
                            tiles)]
    (let [reordered-tiles (vec (for [y (range width)
                                     x (range height)]
                                 (let [new-x (- width 1 y)
                                       new-y x
                                       index (+ (* new-y width) new-x)]
                                   (nth rotated-tiles index))))]
      {:width height
       :height width
       :tiles reordered-tiles})))

;;--------------------------------------------------------------------
;; rendering

(vtx/defvertex pos-uv-vtx
  (vec3 position)
  (vec2 texcoord0))

(shader/defshader tile-map-id-vertex-shader
  (uniform mat4 view_proj)
  (uniform mat4 world)
  (attribute vec4 position)
  (attribute vec2 texcoord0)
  (varying vec2 var_texcoord0)
  (defn void main []
    (setq mat4 mvp (* view_proj world))
    (setq gl_Position (* mvp (vec4 position.xyz 1.0)))
    (setq var_texcoord0 texcoord0)))

(shader/defshader tile-map-id-fragment-shader
  (varying vec2 var_texcoord0)
  (uniform sampler2D DIFFUSE_TEXTURE)
  (uniform vec4 id)
  (defn void main []
    (setq vec4 color (texture2D DIFFUSE_TEXTURE var_texcoord0))
    (if (> color.a 0.05)
      (setq gl_FragColor id)
      (discard))))

(def tile-map-id-shader (shader/make-shader ::tile-map-id-shader tile-map-id-vertex-shader tile-map-id-fragment-shader {"view_proj" :view-proj "world" :world "id" :id}))

(defn render-layer
  [^GL2 gl render-args renderables n]
  (let [pass (:pass render-args)]
    (condp = pass
      pass/transparent
      (let [{:keys [selected ^Matrix4d world-transform user-data]} (first renderables)
            {:keys [node-id vbuf shader gpu-texture blend-mode]} user-data]
        (when vbuf
          (let [render-args (merge render-args
                                   (math/derive-render-transforms ; TODO(instancing): Can we use the render-args as-is?
                                     world-transform
                                     (:view render-args)
                                     (:projection render-args)
                                     (:texture render-args)))
                ;; In the runtime, vertices are in world space. In the editor we
                ;; prefer to keep the vertices in local space in order to avoid
                ;; unnecessary buffer updates. As a workaround, we trick the
                ;; shader by supplying a world-view-projection matrix for the
                ;; view-projection matrix. If this turns out to be a problem, we
                ;; need to produce world-space buffers here in the render
                ;; function, seeing as we don't have the final world-transform
                ;; until the scene has been flattened.
                render-args (assoc render-args :view-proj (:world-view-proj render-args))
                vertex-binding (vtx/use-with node-id vbuf shader)]
            (gl/with-gl-bindings gl render-args [shader vertex-binding gpu-texture]
              (gl/set-blend-mode gl blend-mode)
              ;; TODO: can't use selected because we also need to know when nothing is selected
              #_(if selected
                  (shader/set-uniform shader gl "tint" (Vector4d. 1.0 1.0 1.0 1.0))
                  (shader/set-uniform shader gl "tint" (Vector4d. 1.0 1.0 1.0 0.5)))
              (gl/gl-draw-arrays gl GL2/GL_QUADS 0 (count vbuf))
              (.glBlendFunc gl GL2/GL_SRC_ALPHA GL2/GL_ONE_MINUS_SRC_ALPHA)))))

      pass/selection
      (let [{:keys [^Matrix4d world-transform user-data]} (first renderables)
            {:keys [node-id vbuf gpu-texture]} user-data]
        (when vbuf
          (let [vertex-binding (vtx/use-with node-id vbuf tile-map-id-shader)]
            (gl/with-gl-bindings gl (assoc render-args :id (scene-picking/renderable-picking-id-uniform (first renderables))) [tile-map-id-shader vertex-binding gpu-texture]
              (gl/gl-push-matrix gl
                (gl/gl-mult-matrix-4d gl world-transform)
                (gl/gl-draw-arrays gl GL2/GL_QUADS 0 (count vbuf))))))))))

(defn make-tile-uv-lookup-cache
  [tile-count uv-transforms]
  (let [uv-cache (object-array tile-count)]
    (fn [tile]
      (if (< tile tile-count)
        (or (aget uv-cache tile)
            (let [[[u0 v0] [u1 v1]] (geom/uv-trans (nth uv-transforms tile) [[0 0] [1 1]])
                  uv-coords (float-array 4 [u0 v0 u1 v1])]
              (aset uv-cache tile uv-coords)
              uv-coords))
        (float-array [0 1 1 0])))))

(defn gen-layer-render-data
  [cell-map texture-set-data]
  (let [{:keys [^long tile-width ^long tile-height ^long tile-count]
         :or {tile-width 0, tile-height 0, tile-count 0}} (:texture-set texture-set-data)
        uv-lookup (make-tile-uv-lookup-cache tile-count (:uv-transforms texture-set-data))]
    (loop [^java.util.Iterator it (.iterator (.values ^java.util.Map cell-map))
           vbuf (->pos-uv-vtx (* 6 (count cell-map)))
           min-x 0 min-y 0 max-x 0 max-y 0]
      (if (.hasNext it)
        (let [^Tile tile (.next it)
              x0 (* (.x tile) tile-width)
              y0 (* (.y tile) tile-height)
              x1 (+ x0 tile-width)
              y1 (+ y0 tile-height)
              ^floats uvs (uv-lookup (.tile tile))
              u0 (aget uvs (if (.h-flip tile) 2 0))
              v0 (aget uvs (if (.v-flip tile) 3 1))
              u1 (aget uvs (if (.h-flip tile) 0 2))
              v1 (aget uvs (if (.v-flip tile) 1 3))]
          (recur it
                 (if (.rotate90 tile)
                   (-> vbuf
                       (pos-uv-vtx-put! x0 y1 0 u0 v1)
                       (pos-uv-vtx-put! x1 y1 0 u0 v0)
                       (pos-uv-vtx-put! x1 y0 0 u1 v0)
                       (pos-uv-vtx-put! x0 y0 0 u1 v1))
                   (-> vbuf
                       (pos-uv-vtx-put! x0 y0 0 u0 v1)
                       (pos-uv-vtx-put! x0 y1 0 u0 v0)
                       (pos-uv-vtx-put! x1 y1 0 u1 v0)
                       (pos-uv-vtx-put! x1 y0 0 u1 v1)))
                 (min-l min-x x0)
                 (min-l min-y y0)
                 (max-l max-x x1)
                 (max-l max-y y1)))
        {:vbuf (vtx/flip! vbuf)
         :aabb (geom/coords->aabb [min-x min-y 0]
                                  [max-x max-y 0])}))))

(g/defnk produce-layer-scene
  [_node-id id cell-map texture-set-data z gpu-texture shader blend-mode visible]
  (when visible
    (let [{:keys [aabb vbuf]} (gen-layer-render-data cell-map texture-set-data)
          transform (doto (Matrix4d.) (.set (Vector3d. 0.0 0.0 z)))

          ;; The visibility-aabb is used to determine the scene extents. We use
          ;; it to adjust the camera near and far clip planes to encompass the
          ;; scene. When editing, layers that do not yet have any tiles on them
          ;; will produce an empty aabb which will not expand the scene extents.
          ;; To ensure empty layers will push the scene extents and not be
          ;; outside the camera clip planes, we give them a small bounding box.
          ;; This makes the brush for yet-to-be-painted tiles visible.
          visibility-aabb
          (if (geom/empty-aabb? aabb)
            geom/minimal-xy-aabb
            aabb)]
      {:node-id _node-id
       :node-outline-key id
       :transform transform
       :aabb aabb
       :visibility-aabb visibility-aabb
       :renderable {:render-fn render-layer
                    :tags #{:tilemap}
                    :user-data {:node-id _node-id
                                :vbuf vbuf
                                :gpu-texture gpu-texture
                                :shader shader
                                :blend-mode blend-mode}
                    :passes [pass/transparent pass/selection]}})))

(g/defnk produce-layer-outline
  [_node-id id z visible]
  {:node-id _node-id
   :node-outline-key id
   :label id
   :z z
   :icon tile-map-layer-icon})

(g/defnk produce-layer-pb-msg
  [id z visible cell-map]
  (protobuf/make-map-without-defaults Tile$TileLayer
    :id id
    :z z
    :is-visible (protobuf/boolean->int visible)
    :cell (mapv (fn [^Tile tile]
                  (protobuf/make-map-without-defaults Tile$TileCell
                    :x (.x tile)
                    :y (.y tile)
                    :tile (.tile tile)
                    :v-flip (protobuf/boolean->int (.v-flip tile))
                    :h-flip (protobuf/boolean->int (.h-flip tile))
                    :rotate90 (protobuf/boolean->int (.rotate90 tile))))
                (vals cell-map))))

(g/defnode LayerNode
  (inherits outline/OutlineNode)

  (input texture-set-data g/Any)
  (input gpu-texture g/Any)
  (input shader ShaderLifecycle)
  (input blend-mode g/Any)

  (property cell-map g/Any (default (int-map/int-map))
            (dynamic visible (g/constantly false)))

  (property id g/Str) ; Required protobuf field.
  (property z g/Num ; Required protobuf field.
            (default protobuf/float-zero) ; Default for nodes constructed by editor scripts
            (dynamic error (validation/prop-error-fnk :warning validation/prop-1-1? z))
            (dynamic label (properties/label-dynamic :tile-map.layer :z))
            (dynamic tooltip (properties/tooltip-dynamic :tile-map.layer :z)))

  (property visible g/Bool (default (protobuf/int->boolean (protobuf/default Tile$TileLayer :is-visible)))
            (dynamic label (properties/label-dynamic :tile-map.layer :visible))
            (dynamic tooltip (properties/tooltip-dynamic :tile-map.layer :visible)))

  (output scene g/Any :cached produce-layer-scene)
  (output node-outline outline/OutlineData :cached produce-layer-outline)
  (output pb-msg g/Any :cached produce-layer-pb-msg))

(defn attach-layer-node
  [parent layer-node]
  (concat
   (g/connect layer-node :_node-id                     parent :nodes)
   (g/connect layer-node :id                           parent :layer-ids)
   (g/connect layer-node :node-outline                 parent :child-outlines)
   (g/connect layer-node :scene                        parent :child-scenes)
   (g/connect layer-node :pb-msg                       parent :layer-msgs)
   (g/connect parent     :texture-set-data             layer-node :texture-set-data)
   (g/connect parent     :material-shader              layer-node :shader)
   (g/connect parent     :gpu-texture                  layer-node :gpu-texture)
   (g/connect parent     :blend-mode                   layer-node :blend-mode)))

(defn make-layer-node
  [parent tile-layer]
  {:pre [(map? tile-layer)]} ; Tile$TileLayer in map format.
  (let [graph-id (g/node-id->graph-id parent)]
    (g/make-nodes
      graph-id
      [layer-node LayerNode]
      (gu/set-properties-from-pb-map layer-node Tile$TileLayer tile-layer
        id :id
        z :z
        visible (protobuf/int->boolean :is-visible)
        cell-map (make-cell-map :cell))
      (attach-layer-node parent layer-node))))

(defn world-pos->tile
  [^Point3d pos tile-width tile-height]
  [(long (Math/floor (/ (.x pos) tile-width)))
   (long (Math/floor (/ (.y pos) tile-height)))])

(def ^:private default-material-proj-path (protobuf/default Tile$TileGrid :material))

(defn- sanitize-tile-map [{:keys [material] :as tile-grid}]
  {:pre [(map? tile-grid)]} ; Tile$TileGrid in map format.
  (cond-> tile-grid
          (nil? material)
          (assoc :material default-material-proj-path)))

(defn- load-tile-map
  [project self resource tile-grid]
  {:pre [(map? tile-grid)]} ; Tile$TileGrid in map format.
  (let [tile-source (workspace/resolve-resource resource (:tile-set tile-grid))
        material (workspace/resolve-resource resource (:material tile-grid))
        resolve-resource #(workspace/resolve-resource resource %)]
    (concat
      (g/connect project :default-tex-params self :default-tex-params)
      (gu/set-properties-from-pb-map self Tile$TileGrid tile-grid
        tile-source (resolve-resource :tile-set)
        material (resolve-resource (:material :or default-material-proj-path))
        blend-mode :blend-mode)
      (for [tile-layer (:layers tile-grid)]
        (make-layer-node self tile-layer)))))

(g/defnk produce-scene
  [_node-id child-scenes scene-aabb]
  {:node-id           _node-id
   :aabb              scene-aabb
   :renderable        {:passes [pass/selection]}
   :children          child-scenes})

(g/defnk produce-node-outline
  [_node-id child-outlines]
  {:node-id          _node-id
   :node-outline-key "Tile Map"
   :label            (localization/message "outline.tile-map")
   :icon             tile-map-icon
   :children         (vec (sort-by :z child-outlines))})

(g/defnk produce-save-value
  [tile-source material blend-mode layer-msgs]
  (protobuf/make-map-without-defaults Tile$TileGrid
    :tile-set (resource/resource->proj-path tile-source)
    :material (resource/resource->proj-path material)
    :blend-mode blend-mode
    :layers (sort-by :z layer-msgs)))

(defn build-tile-map
  [resource dep-resources user-data]
  (let [pb-msg (reduce #(assoc %1 (first %2) (second %2))
                       (:pb-msg user-data)
                       (map (fn [[label res]] [label (resource/proj-path (get dep-resources res))]) (:dep-resources user-data)))]
    {:resource resource
     :content (protobuf/map->bytes Tile$TileGrid pb-msg)}))

(defn- prop-resource-error [nil-severity _node-id prop-kw prop-value prop-name]
  (or (validation/prop-error nil-severity _node-id prop-kw validation/prop-nil? prop-value prop-name)
      (validation/prop-error :fatal _node-id prop-kw validation/prop-resource-not-exists? prop-value prop-name)))

(defn- prop-tile-source-range-error
  [_node-id tile-source tile-count max-tile-index]
  (validation/prop-error :fatal _node-id :tile-source
                         (fn [v name]
                           (when-not (< max-tile-index tile-count)
                             (format "Tile map uses tiles outside the range of this tile source (%d tiles in source, but a tile with index %d is used in tile map)" tile-count max-tile-index))) tile-source "Tile Source"))

(g/defnk produce-build-targets
  [_node-id resource tile-source material save-value dep-build-targets tile-count max-tile-index]
  (g/precluding-errors
    [(prop-resource-error :fatal _node-id :tile-source tile-source "Tile Source")
     (prop-tile-source-range-error _node-id tile-source tile-count max-tile-index)]
    (let [dep-build-targets (flatten dep-build-targets)
          deps-by-resource (into {} (map (juxt (comp :resource :resource) :resource) dep-build-targets))
          dep-resources (map (fn [[label resource]]
                               [label (get deps-by-resource resource)])
                             [[:tile-set tile-source]
                              [:material material]])]
      [(bt/with-content-hash
         {:node-id _node-id
          :resource (workspace/make-build-resource resource)
          :build-fn build-tile-map
          :user-data {:pb-msg save-value
                      :dep-resources dep-resources}
          :deps dep-build-targets})])))

(g/defnode TileMapNode
  (inherits resource-node/ResourceNode)

  (input layer-ids g/Any :array)
  (input layer-msgs g/Any :array)
  (input child-scenes g/Any :array)
  (input dep-build-targets g/Any :array)
  (input tile-source-resource resource/Resource)
  (input tile-source-attributes g/Any)
  (input texture-set-data g/Any)
  (input gpu-texture g/Any)
  (input material-resource resource/Resource)
  (input material-shader ShaderLifecycle)
  (input material-samplers g/Any)
  (input default-tex-params g/Any)

  ;; tile source
  (property tile-source resource/Resource ; Required protobuf field.
            (value (gu/passthrough tile-source-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:resource :tile-source-resource]
                                            [:build-targets :dep-build-targets]
                                            [:tile-source-attributes :tile-source-attributes]
                                            [:texture-set-data :texture-set-data]
                                            [:gpu-texture :gpu-texture])))
            (dynamic error (g/fnk [_node-id tile-source tile-count max-tile-index]
                             (or (prop-resource-error :fatal _node-id :tile-source tile-source "Tile Source")
                                 (prop-tile-source-range-error _node-id tile-source tile-count max-tile-index))))
            (dynamic edit-type (g/constantly {:type resource/Resource :ext "tilesource"}))
            (dynamic label (properties/label-dynamic :tile-map :tile-source))
            (dynamic tooltip (properties/tooltip-dynamic :tile-map :tile-source)))

  ;; material
  (property material resource/Resource ; Default assigned in load-fn.
            (value (gu/passthrough material-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:resource :material-resource]
                                            [:build-targets :dep-build-targets]
                                            [:shader :material-shader]
                                            [:samplers :material-samplers])))
            (dynamic error (g/fnk [_node-id material]
                                  (prop-resource-error :fatal _node-id :material material "Material")))
            (dynamic edit-type (g/constantly {:type resource/Resource :ext "material"})))

  (property blend-mode g/Any (default (protobuf/default Tile$TileGrid :blend-mode))
            (dynamic edit-type (g/constantly (properties/->pb-choicebox Tile$TileGrid$BlendMode))))

  (output max-tile-index g/Any :cached (g/fnk [layer-msgs]
                                         (transduce (comp (mapcat :cell)
                                                          (map :tile))
                                                    max 0 layer-msgs)))

  (output tile-source-attributes g/Any (gu/passthrough tile-source-attributes))
  (output tile-count g/Int (g/fnk [tile-source-attributes]
                             (* (:tiles-per-row tile-source-attributes 0) (:tiles-per-column tile-source-attributes 0))))
  (output tile-dimensions g/Any
          (g/fnk [tile-source-attributes]
            (when tile-source-attributes
              (let [{:keys [width height]} tile-source-attributes]
                (when (and width height)
                  [width height])))))

  (output scene-aabb AABB :cached
          (g/fnk [tile-dimensions]
            (if-not tile-dimensions
              geom/null-aabb
              (geom/make-aabb (Point3d. 0.0 0.0 0.0)
                              (Point3d. (* 16 (first tile-dimensions)) (* 9 (second tile-dimensions)) 0.0)))))

  (output texture-set-data g/Any (gu/passthrough texture-set-data))
  (output tex-params g/Any (g/fnk [material-samplers default-tex-params]
                             (or (some-> material-samplers first material/sampler->tex-params)
                                 default-tex-params)))
  (output gpu-texture g/Any (g/fnk [gpu-texture tex-params] (texture/set-params gpu-texture tex-params)))
  (output material-shader ShaderLifecycle (gu/passthrough material-shader))
  (output scene g/Any :cached produce-scene)
  (output node-outline outline/OutlineData :cached produce-node-outline)
  (output save-value g/Any :cached produce-save-value)
  (output build-targets g/Any :cached produce-build-targets))

;;--------------------------------------------------------------------
;; tool

(shader/defshader pos-uv-vert
  (attribute vec4 position)
  (attribute vec2 texcoord0)
  (varying vec2 var_texcoord0)
  (defn void main []
    (setq gl_Position (* gl_ModelViewProjectionMatrix position))
    (setq var_texcoord0 texcoord0)))

(shader/defshader pos-uv-frag
  (varying vec2 var_texcoord0)
  (uniform sampler2D texture_sampler)
  (defn void main []
    (setq gl_FragColor (texture2D texture_sampler var_texcoord0.xy))))

(def tex-shader (shader/make-shader ::tex-shader pos-uv-vert pos-uv-frag))

(vtx/defvertex color-vtx
  (vec3 position)
  (vec4 color))

(shader/defshader pos-color-vert
  (attribute vec4 position)
  (attribute vec4 color)
  (varying vec4 var_color)
  (defn void main []
    (setq gl_Position (* gl_ModelViewProjectionMatrix position))
    (setq var_color color)))

(shader/defshader pos-color-frag
  (varying vec4 var_color)
  (defn void main []
    (setq gl_FragColor var_color)))

(def color-shader (shader/make-shader ::color-shader pos-color-vert pos-color-frag))

(def ^:private white-color (double-array (map #(/ % 255.0) [255 255 255])))
(def ^:private blue-color (double-array (map #(/ % 255.0) [0 191 255])))
(def ^:private red-color (double-array (map #(/ % 255.0) [255 0 0])))
(def ^:private orange-color (double-array (map #(/ % 255.0) [255 140 0])))

;; brush

(defn render-brush-outline
  [^GL2 gl render-args renderables count]
  (let [renderable (first renderables)
        world-transform (:world-transform renderable)
        user-data (:user-data renderable)
        [x y] (:cell user-data)
        color (:color user-data)
        {:keys [width height]} (:brush user-data)
        [tile-width tile-height] (:tile-dimensions user-data)]
    (when (and x y)
      (let [x0 (* tile-width x)
            y0 (* tile-height y)
            x1 (+ x0 (* width tile-width))
            y1 (+ y0 (* height tile-height))
            z 0.0
            c color]
        (.glMatrixMode gl GL2/GL_MODELVIEW)
        (gl/gl-push-matrix gl
          (gl/gl-mult-matrix-4d gl world-transform)
          (.glColor3d gl (nth c 0) (nth c 1) (nth c 2))
          (.glBegin gl GL2/GL_LINE_LOOP)
          (.glVertex3d gl x0 y0 z)
          (.glVertex3d gl x1 y0 z)
          (.glVertex3d gl x1 y1 z)
          (.glVertex3d gl x0 y1 z)
          (.glEnd gl))))))

(defn conj-brush-quad!
  [vbuf {:keys [tile h-flip v-flip rotate90]} uvs w h x y]
  (if-not tile
    vbuf
    (let [uv (nth uvs tile (geom/identity-uv-trans))
          [p1 p2] (geom/uv-trans uv [[0 0] [1 1]])
          u0 (first (if h-flip p2 p1))
          v0 (second (if v-flip p2 p1))
          u1 (first (if h-flip p1 p2))
          v1 (second (if v-flip p1 p2))
          x0 x
          y0 y
          x1 (+ x0 w)
          y1 (+ y0 h)]
      (if rotate90
          (-> vbuf
              (pos-uv-vtx-put! x0 y1 0 u0 v1)
              (pos-uv-vtx-put! x1 y1 0 u0 v0)
              (pos-uv-vtx-put! x1 y0 0 u1 v0)
              (pos-uv-vtx-put! x0 y0 0 u1 v1))
          (-> vbuf
              (pos-uv-vtx-put! x0 y0 0 u0 v1)
              (pos-uv-vtx-put! x0 y1 0 u0 v0)
              (pos-uv-vtx-put! x1 y1 0 u1 v0)
              (pos-uv-vtx-put! x1 y0 0 u1 v1))))))

(defn gen-brush-vbuf
  [brush uvs tile-width tile-height]
  (let [{:keys [width height tiles]} brush]
    (loop [x 0
           y 0
           tiles tiles
           vbuf (->pos-uv-vtx (* 4 (count tiles)))]
      (if (< y height)
        (if (< x width)
          (recur (inc x) y (rest tiles) (conj-brush-quad! vbuf (first tiles) uvs tile-width tile-height (* x tile-width) (* y tile-height)))
          (recur 0 (inc y) tiles vbuf))
        (vtx/flip! vbuf)))))

(defn render-brush
  [^GL2 gl render-args renderables n]
  (let [renderable (first renderables)
        user-data (:user-data renderable)
        {:keys [texture-set-data gpu-texture brush]} user-data
        uvs (:uv-transforms texture-set-data)
        [x y] (:cell user-data)
        [w h] (:tile-dimensions user-data)
        vbuf (gen-brush-vbuf brush uvs w h)
        vb (vtx/use-with ::brush vbuf tex-shader)
        ^Matrix4d layer-transform (:world-transform renderable)
        local-transform (doto (Matrix4d. geom/Identity4d)
                          (.set (Vector3d. (* x w) (* y h) 0.001)))
        brush-transform (doto (Matrix4d. local-transform)
                          (.mul layer-transform))]
    (.glMatrixMode gl GL2/GL_MODELVIEW)
    (gl/gl-push-matrix gl
      (gl/gl-mult-matrix-4d gl brush-transform)
      (gl/with-gl-bindings gl render-args [tex-shader vb gpu-texture]
        (shader/set-uniform tex-shader gl "texture_sampler" 0)
        (gl/gl-draw-arrays gl GL2/GL_QUADS 0 (count vbuf))))))


;; palette

(def ^:private tile-border-size 1)

(def ^:private ideal-tile-size 32)

(def ^:private edge-offset 100)

(def ^:private clamp-palette-mouse-offset (geom/clamper -0.5 0.5))

(def ^:private select-modes #{:select-mode :cut-mode :erase-mode})

(defn- make-palette-transform
  [tile-source-attributes viewport ^Vector3d cursor-screen-pos]
  (let [{:keys [width
                height
                visual-width
                visual-height
                tiles-per-row
                tiles-per-column]} tile-source-attributes
        {:keys [bottom right]} viewport
        cursor-pos (or cursor-screen-pos
                       (Vector3d. (* right 0.5) (* height 0.5) 0))
        scale (-> ideal-tile-size
                  (/ (max-l width height))
                  floor-d
                  (max-d 1.0))
        palette-width (-> tile-border-size
                          (* (dec tiles-per-row))
                          (+ visual-width)
                          (* scale))
        palette-height (-> tile-border-size
                           (* (dec tiles-per-column))
                           (+ visual-height)
                           (* scale))
        max-offset-x (-> palette-width
                         (- right)
                         (+ (* 2 edge-offset))
                         (max-d 0))
        max-offset-y (-> palette-height
                         (- bottom)
                         (+ (* 2 edge-offset))
                         (max-d 0))
        mouse-offset-ratio-x (-> (.-x cursor-pos)
                                 (- edge-offset)
                                 (/ (- right (* 2 edge-offset)))
                                 (- 0.5)
                                 clamp-palette-mouse-offset)
        mouse-offset-ratio-y (-> (.-y cursor-pos)
                                 (- edge-offset)
                                 (/ (- bottom (* 2 edge-offset)))
                                 (- 0.5)
                                 clamp-palette-mouse-offset)
        scale-m (doto (Matrix4d.)
                  (.setIdentity)
                  (.set (Vector3d. (* 0.5 right)
                                   (* 0.5 bottom)
                                   0.0))
                  (.setScale scale))
        trans-m (doto (Matrix4d.)
                  (.set (Vector3d. (-> 0
                                       (- (* max-offset-x mouse-offset-ratio-x))
                                       (- (* 0.5 palette-width))
                                       (/ scale))
                                   (-> 0
                                       (- (* max-offset-y mouse-offset-ratio-y))
                                       (- (* 0.5 palette-height))
                                       (/ scale))
                                   0.0)))]
    (doto scale-m
      (.mul trans-m))))

(g/defnk produce-palette-tile
  [^Vector3d cursor-screen-pos
   tile-source-attributes
   tile-dimensions
   ^Matrix4d palette-transform]
  (when (and cursor-screen-pos tile-source-attributes)
    (let [{:keys [width height tiles-per-row tiles-per-column]} tile-source-attributes
          p (Point3d. cursor-screen-pos)
          t (doto (Matrix4d. palette-transform)
              (.invert))
          _ (.transform t p)
          [x y] (world-pos->tile p
                                 (+ width tile-border-size)
                                 (+ height tile-border-size))]
      (when (and (<= 0 x (dec tiles-per-row))
                 (<= 0 y (dec tiles-per-column)))
        (+ (* y tiles-per-row) x)))))

(defn gen-palette-tiles-vbuf
  [tile-source-attributes texture-set-data]
  (let [uvs (:uv-transforms texture-set-data)
        w (:width tile-source-attributes)
        h (:height tile-source-attributes)
        rows (:tiles-per-column tile-source-attributes)
        cols (:tiles-per-row tile-source-attributes)
        vbuf (->pos-uv-vtx (* 4 (:tile-count (:texture-set texture-set-data))))]
    (loop [x 0
           y 0
           vbuf vbuf]
      (if (< y rows)
        (if (< x cols)
          (let [uv (nth uvs (+ x (* y cols)))
                x0 (+ (* x (+ tile-border-size w)) tile-border-size)
                x1 (+ x0 w)
                y0 (+ (* y (+ tile-border-size h)) tile-border-size)
                y1 (+ y0 h)
                [[u0 v0] [u1 v1]] (geom/uv-trans uv [[0 0] [1 1]])]
            (recur (inc x)
                   y
                   (-> vbuf
                       (pos-uv-vtx-put! x0 y0 0 u0 v0)
                       (pos-uv-vtx-put! x0 y1 0 u0 v1)
                       (pos-uv-vtx-put! x1 y1 0 u1 v1)
                       (pos-uv-vtx-put! x1 y0 0 u1 v0))))
          (recur 0 (inc y) vbuf))
        (vtx/flip! vbuf)))))

(defn- render-palette-tiles
  [^GL2 gl render-args tile-source-attributes texture-set-data gpu-texture]
  (let [vbuf (gen-palette-tiles-vbuf tile-source-attributes texture-set-data)
        vb (vtx/use-with ::palette-tiles vbuf tex-shader)
        gpu-texture (texture/set-params gpu-texture tile-source/texture-params)]
    (gl/with-gl-bindings gl render-args [tex-shader vb gpu-texture]
      (shader/set-uniform tex-shader gl "texture_sampler" 0)
      (gl/gl-draw-arrays gl GL2/GL_QUADS 0 (count vbuf)))))


(defn gen-palette-grid-vbuf
  [tile-source-attributes]
  (let [tw (:width tile-source-attributes)
        th (:height tile-source-attributes)
        rows (:tiles-per-column tile-source-attributes)
        cols (:tiles-per-row tile-source-attributes)
        w (+ (:visual-width tile-source-attributes) (* (inc cols) tile-border-size))
        h (+ (:visual-height tile-source-attributes) (* rows tile-border-size))]
    (as-> (->color-vtx (+ (* (+ 1 rows) 4)
                          (* (+ 1 cols) 4)))
        vbuf
      (reduce (fn [vbuf y]
                (let [y0 (* y (+ th tile-border-size))]
                  (-> vbuf
                      (color-vtx-put! 0 y0 0 0.3 0.3 0.3 1.0)
                      (color-vtx-put! w y0 0 0.3 0.3 0.3 1.0)
                      (color-vtx-put! w (+ tile-border-size y0) 0 0.3 0.3 0.3 1.0)
                      (color-vtx-put! 0 (+ tile-border-size y0) 0 0.3 0.3 0.3 1.0))))
              vbuf
              (range (inc rows)))
      (reduce (fn [vbuf x]
                (let [x0 (* x (+ tw tile-border-size))]
                  (-> vbuf
                      (color-vtx-put! x0 0 0 0.3 0.3 0.3 1.0)
                      (color-vtx-put! x0 h 0 0.3 0.3 0.3 1.0)
                      (color-vtx-put! (+ tile-border-size x0) h 0 0.3 0.3 0.3 1.0)
                      (color-vtx-put! (+ tile-border-size x0) 0 0 0.3 0.3 0.3 1.0))))
              vbuf
              (range (inc cols)))
      (vtx/flip! vbuf))))

(defn- render-palette-grid
  [^GL2 gl render-args tile-source-attributes]
  (let [vbuf (gen-palette-grid-vbuf tile-source-attributes)
        vb (vtx/use-with ::palette-grid vbuf color-shader)]
    (gl/with-gl-bindings gl render-args [color-shader vb]
      (gl/gl-draw-arrays gl GL2/GL_QUADS 0 (count vbuf)))))

(defn- render-palette-active
  [^GL2 gl render-args tile-source-attributes start-tile end-tile]
  (when (and start-tile end-tile)
    (let [tiles-per-row (:tiles-per-row tile-source-attributes)
          start-x (palette-x start-tile tiles-per-row)
          start-y (palette-y start-tile tiles-per-row)
          end-x (palette-x end-tile tiles-per-row)
          end-y (palette-y end-tile tiles-per-row)
          width (:width tile-source-attributes)
          height (:height tile-source-attributes)
          x0 (* (min start-x end-x) (+ width tile-border-size))
          x1 (+ (* (inc (max start-x end-x)) width) (* (max start-x end-x) tile-border-size))
          y0 (* (min start-y end-y) (+ height tile-border-size))
          y1 (+ (* (inc (max start-y end-y)) height) (* (max start-y end-y) tile-border-size))
          [r g b] blue-color
          a 1.0
          vbuf (-> (->color-vtx 16)
                   ;; left edge
                   (color-vtx-put! x0 y0 0 r g b a)
                   (color-vtx-put! x0 y1 0 r g b a)
                   (color-vtx-put! (+ x0 tile-border-size) y1 0 r g b a)
                   (color-vtx-put! (+ x0 tile-border-size) y0 0 r g b a)
                   ;; right edge
                   (color-vtx-put! x1 y0 0 r g b a)
                   (color-vtx-put! x1 y1 0 r g b a)
                   (color-vtx-put! (+ x1 tile-border-size) y1 0 r g b a)
                   (color-vtx-put! (+ x1 tile-border-size) y0 0 r g b a)
                   ;; bottom edge
                   (color-vtx-put! x0 y0 0 r g b a)
                   (color-vtx-put! x1 y0 0 r g b a)
                   (color-vtx-put! x1 (+ y0 tile-border-size) 0 r g b a)
                   (color-vtx-put! x0 (+ y0 tile-border-size) 0 r g b a)
                   ;; top edge
                   (color-vtx-put! x0 y1 0 r g b a)
                   (color-vtx-put! (+ x1 tile-border-size) y1 0 r g b a)
                   (color-vtx-put! (+ x1 tile-border-size) (+ y1 tile-border-size) 0 r g b a)
                   (color-vtx-put! x0 (+ y1 tile-border-size) 0 r g b a)
                   (vtx/flip!))
          vb (vtx/use-with ::palette-active vbuf color-shader)]
      (gl/with-gl-bindings gl render-args [color-shader vb]
        (gl/gl-draw-arrays gl GL2/GL_QUADS 0 (count vbuf))))))

(defn render-palette-background
  [^GL2 gl viewport]
  (let [{:keys [top left right bottom]} viewport]
    (.glColor4d gl 0.0 0.0 0.0 0.7)
    (.glBegin gl GL2/GL_QUADS)
    (.glVertex2d gl 0.0 0.0)
    (.glVertex2d gl right 0.0)
    (.glVertex2d gl right bottom)
    (.glVertex2d gl 0.0 bottom)
    (.glEnd gl)))

(defn render-palette
  [^GL2 gl render-args renderables count]
  (let [user-data (:user-data (first renderables))
        {:keys [viewport tile-source-attributes texture-set-data gpu-texture palette-transform start-tile end-tile]} user-data
        [start-tile end-tile] (if (and start-tile end-tile (<= start-tile end-tile))
                                [start-tile end-tile]
                                [end-tile (or start-tile end-tile)])]
    (render-palette-background gl viewport)
    (.glMatrixMode gl GL2/GL_MODELVIEW)
    (gl/gl-push-matrix gl
      (gl/gl-mult-matrix-4d gl palette-transform)
      (render-palette-tiles gl render-args tile-source-attributes texture-set-data gpu-texture)
      (render-palette-grid gl render-args tile-source-attributes)
      (render-palette-active gl render-args tile-source-attributes start-tile end-tile))))

(defn render-editor-select-outline
  [^GL2 gl render-args renderables count]
  (let [renderable (first renderables)
        world-transform (:world-transform renderable)
        user-data (:user-data renderable)
        [sx sy] (:start user-data)
        [ex ey] (:end user-data)
        color (:color user-data)
        [tile-width tile-height] (:tile-dimensions user-data)]
    (when (and sx sy ex ey)
      (let [x0 (* tile-width (min-l sx ex))
            y0 (* tile-height (min-l sy ey))
            x1 (* tile-width (inc (max-l sx ex)))
            y1 (* tile-height (inc (max-l sy ey)))
            z 0.0
            c color]
        (.glMatrixMode gl GL2/GL_MODELVIEW)
        (gl/gl-push-matrix gl
          (gl/gl-mult-matrix-4d gl world-transform)
          (.glColor3d gl (nth c 0) (nth c 1) (nth c 2))
          (.glBegin gl GL2/GL_LINE_LOOP)
          (.glVertex3d gl x0 y0 z)
          (.glVertex3d gl x1 y0 z)
          (.glVertex3d gl x1 y1 z)
          (.glVertex3d gl x0 y1 z)
          (.glEnd gl))))))

(defn render-editor-select
  [^GL2 gl render-args renderables n]
  (let [pass (:pass render-args)]
    (condp = pass
      pass/outline
      (render-editor-select-outline gl render-args renderables n))))

(defn render-editor
  [^GL2 gl render-args renderables n]
  (let [pass (:pass render-args)]
    (condp = pass
      pass/opaque
      (render-brush gl render-args renderables n)

      pass/outline
      (render-brush-outline gl render-args renderables n))))

(g/defnk produce-palette-renderables
  [viewport tile-source-attributes texture-set-data gpu-texture palette-transform start-palette-tile palette-tile]
  {pass/overlay [{:world-transform (Matrix4d. geom/Identity4d)
                  :render-fn render-palette
                  :user-data {:viewport viewport
                              :tile-source-attributes tile-source-attributes
                              :texture-set-data texture-set-data
                              :gpu-texture gpu-texture
                              :palette-transform palette-transform
                              :start-tile start-palette-tile
                              :end-tile palette-tile}}]})

(g/defnk produce-editor-renderables
  [active-layer-renderable op op-select-start op-select-end current-tile tile-dimensions brush viewport texture-set-data gpu-texture cursor-mode]
  (when active-layer-renderable
    {pass/opaque (cond
                        current-tile
                        [{:world-transform (:world-transform active-layer-renderable)
                          :render-fn render-editor
                          :user-data {:cell current-tile
                                      :brush (if (contains? select-modes cursor-mode)
                                               empty-brush
                                               brush)
                                      :tile-dimensions tile-dimensions
                                      :texture-set-data texture-set-data
                                      :gpu-texture gpu-texture}}])
     pass/outline (cond
                    (= :select op) [{:world-transform (:world-transform active-layer-renderable)
                                     :render-fn render-editor-select
                                     :user-data {:start op-select-start
                                                 :end op-select-end
                                                 :tile-dimensions tile-dimensions
                                                 :color (case cursor-mode
                                                          :cut-mode orange-color
                                                          :erase-mode red-color
                                                          blue-color)}}]
                    current-tile
                    [{:world-transform (:world-transform active-layer-renderable)
                      :render-fn render-editor
                      :user-data {:cell current-tile
                                  :brush (if (contains? select-modes cursor-mode)
                                           empty-brush
                                           brush)
                                  :color (case cursor-mode
                                           :cut-mode orange-color
                                           :erase-mode red-color
                                           :select-mode blue-color
                                           (if (every? nil? (:tiles brush))
                                             red-color
                                             white-color))
                                  :tile-dimensions tile-dimensions}}])}))

(g/defnk produce-tool-renderables
  [mode editor-renderables palette-renderables]
  (case mode
    :editor editor-renderables
    :palette palette-renderables))


;;--------------------------------------------------------------------
;; input handling

(defmulti begin-op (fn [op node action state evaluation-context cursor-mode] op))
(defmulti update-op (fn [op node action state evaluation-context cursor-mode] op))
(defmulti end-op (fn [op node action state evaluation-context cursor-mode] op))

;; painting tiles from brush

(defmethod begin-op :paint
  [op self action state evaluation-context cursor-mode]
  (when-let [active-layer (g/node-value self :active-layer evaluation-context)]
    (when-let [current-tile (g/node-value self :current-tile evaluation-context)]
      (let [brush (g/node-value self :brush evaluation-context)
            op-seq (gensym)]
        (swap! state assoc :last-tile current-tile)
        [(g/set-property self :op-seq op-seq)
         (g/operation-sequence op-seq)
         (g/update-property active-layer :cell-map paint current-tile brush)]))))

(defmethod update-op :paint
  [op self action state evaluation-context cursor-mode]
  (when-let [active-layer (g/node-value self :active-layer evaluation-context)]
    (when-let [current-tile (g/node-value self :current-tile evaluation-context)]
      (when (not= current-tile (-> state deref :last-tile))
        (swap! state assoc :last-tile current-tile)
        (let [brush (g/node-value self :brush evaluation-context)
              op-seq (g/node-value self :op-seq evaluation-context)]
          [(g/operation-sequence op-seq)
           (g/update-property active-layer :cell-map paint current-tile brush)])))))

(defmethod end-op :paint
  [op self action state evaluation-context cursor-mode]
  (swap! state dissoc :last-tile)
  [(g/set-property self :op-seq nil)])

;; selecting brush from cell-map

(defmethod begin-op :select
  [op self action state evaluation-context cursor-mode]
  (when-let [active-layer (g/node-value self :active-layer evaluation-context)]
    (when-let [current-tile (g/node-value self :current-tile evaluation-context)]
      [(g/set-property self :op-select-start current-tile)
       (g/set-property self :op-select-end current-tile)])))

(defmethod update-op :select
  [op self action state evaluation-context cursor-mode]
  (when-let [active-layer (g/node-value self :active-layer evaluation-context)]
    (when-let [current-tile (g/node-value self :current-tile evaluation-context)]
      [(g/set-property self :op-select-end current-tile)])))

(defmethod end-op :select
  [op self action state evaluation-context cursor-mode]
  (when-let [active-layer (g/node-value self :active-layer evaluation-context)]
    (let [cell-map (g/node-value active-layer :cell-map evaluation-context)
          start (g/node-value self :op-select-start evaluation-context)
          end (g/node-value self :op-select-end evaluation-context)]
      (concat
        (when (or (= :cut-mode cursor-mode) (= :select-mode cursor-mode))
          [(g/set-property self :brush (make-brush-from-selection cell-map start end))])
        (when (or (= :erase-mode cursor-mode) (= :cut-mode cursor-mode))
          [(g/update-property active-layer :cell-map erase-area start end)])
        [(g/set-property self :op-select-start nil)
         (g/set-property self :op-select-end nil)]))))

(defn- handle-input-editor
  [self action state evaluation-context]
  (let [op (g/node-value self :op evaluation-context)
        cursor-mode (cond
                      (and (true? (:shift action)) (true? (:control action))) :cut-mode
                      (and (true? (:shift action)) (true? (:alt action))) :erase-mode
                      (true? (:shift action)) :select-mode
                      :else :paint-mode)
        tx (case (:type action)
             :mouse-pressed
             (when-not (some? op)
               (let [op (if (true? (:shift action))
                          :select
                          :paint)
                     op-tx (begin-op op self action state evaluation-context cursor-mode)]
                 (when (seq op-tx)
                   (concat
                     (g/set-property self :op op)
                     (g/set-property self :cursor-mode cursor-mode)
                     op-tx))))

             :mouse-moved
             (concat
               (g/set-property self :cursor-world-pos (:world-pos action))
               (g/set-property self :cursor-screen-pos (:screen-pos action))
               (g/set-property self :cursor-mode cursor-mode)
               (when (some? op)
                 (update-op op self action state evaluation-context cursor-mode)))

             :mouse-released
             (when (some? op)
               (concat
                 (g/set-property self :op nil)
                 (end-op op self action state evaluation-context (g/node-value self :cursor-mode evaluation-context))))

             nil)]
    (when (seq tx)
      (g/transact tx)
      true)))

(defn- handle-input-palette
  [self action state evaluation-context]
  (let [^Point3d screen-pos (:screen-pos action)]
    (case (:type action)
      :mouse-pressed
      (do
        (g/transact
          (g/set-property self :start-palette-tile (g/node-value self :palette-tile evaluation-context)))
        true)

      :mouse-moved
      (do
        (g/transact
          (g/set-property self :cursor-screen-pos screen-pos))
        true)

      :mouse-released
      (let [start-tile (g/node-value self :start-palette-tile evaluation-context)
            end-tile (g/node-value self :palette-tile evaluation-context)]
        (g/transact
          (concat
            (when (and start-tile end-tile)
              [(g/set-property self :brush
                 (make-brush-from-selection-in-palette start-tile end-tile (g/node-value self :tile-source-attributes evaluation-context)))])
            [(g/set-property self :start-palette-tile nil)
             (g/set-property self :mode :editor)]))
        true)
      false)))

(defn handle-input
  [self action state]
  (let [evaluation-context (g/make-evaluation-context)
        mode (g/node-value self :mode evaluation-context)]
    (case mode
      :palette (handle-input-palette self action state evaluation-context)
      :editor  (handle-input-editor self action state evaluation-context))))

(defn make-input-handler
  []
  (let [state (atom nil)]
    (fn [self action _]
      (handle-input self action state))))

(defn- get-current-tile
  [cursor-world-pos tile-dimensions]
  (when (and cursor-world-pos tile-dimensions)
    (let [[w h] tile-dimensions]
      (world-pos->tile cursor-world-pos w h))))

(g/defnode TileMapController
  (property prefs g/Any)
  (property cursor-world-pos Point3d)
  (property cursor-screen-pos Vector3d)

  (property mode g/Keyword (default :editor))

  (property op g/Keyword)
  (property cursor-mode g/Keyword)
  (property op-seq g/Any)
  (property op-select-start g/Any)
  (property op-select-end g/Any)

  (property brush g/Any (default (make-brush 0)))
  (property start-palette-tile g/Int)

  (input active-tool g/Keyword)
  (input manip-space g/Keyword)
  (input camera g/Any)
  (input viewport g/Any)
  (input selected-renderables g/Any)

  (input tile-source-attributes g/Any)
  (input texture-set-data g/Any)
  (input material-shader ShaderLifecycle)
  (input gpu-texture g/Any)
  (input tile-dimensions g/Any)

  (output active-layer g/Any
          (g/fnk [active-layer-renderable]
            (:node-id active-layer-renderable)))

  (output active-layer-renderable g/Any
          (g/fnk [selected-renderables]
            (->> selected-renderables
                 (filter #(g/node-instance? LayerNode (:node-id %)))
                 single)))

  (output current-tile g/Any
          (g/fnk [cursor-world-pos tile-dimensions]
            (get-current-tile cursor-world-pos tile-dimensions)))

  (output palette-transform g/Any
          (g/fnk [tile-source-attributes viewport cursor-screen-pos]
            (when tile-source-attributes
              (make-palette-transform tile-source-attributes viewport cursor-screen-pos))))

  (output palette-tile g/Any produce-palette-tile)
  (output editor-renderables pass/RenderData produce-editor-renderables)
  (output palette-renderables pass/RenderData produce-palette-renderables)
  (output renderables pass/RenderData :cached produce-tool-renderables)
  (output input-handler Runnable :cached (g/constantly (make-input-handler)))
  (output info-text g/Str (g/fnk [cursor-world-pos tile-dimensions mode palette-tile]
                            (case mode
                              :editor (when-some [[x y] (get-current-tile cursor-world-pos tile-dimensions)]
                                        (format "Cell: %d, %d" (+ 1 x) (+ 1 y)))
                              :palette (when palette-tile
                                         (format "Tile: %d" (+ 1 palette-tile)))))))

(defmethod scene/attach-tool-controller ::TileMapController
  [_ tool-id view-id resource-id]
  (concat
   (g/connect resource-id :tile-source-attributes tool-id :tile-source-attributes)
   (g/connect resource-id :texture-set-data tool-id :texture-set-data)
   (g/connect resource-id :material-shader tool-id :material-shader)
   (g/connect resource-id :gpu-texture tool-id :gpu-texture)
   (g/connect resource-id :tile-dimensions tool-id :tile-dimensions)))


;; handlers/menu

(defn- selection->tile-map [selection]
  (handler/adapt-single selection TileMapNode))

(defn- selection->layer [selection]
  (handler/adapt-single selection LayerNode))

(defn tile-map-node
  [selection]
  (or (selection->tile-map selection)
      (some-> (selection->layer selection)
        core/scope)))

(defn- make-new-layer
  [id]
  (protobuf/make-map-without-defaults Tile$TileLayer
    :id id
    :z protobuf/float-zero))

(defn- add-layer-handler
  [tile-map-node]
  (let [layer-id (id/gen "layer" (g/node-value tile-map-node :layer-ids))]
    (g/transact
     (concat
      (g/operation-label (localization/message "operation.tile-map.add-layer"))
      (make-layer-node tile-map-node (make-new-layer layer-id))))))

(handler/defhandler :edit.add-embedded-component :workbench
  (label [user-data] (localization/message "command.edit.add-embedded-component.variant.tile-map"))
  (active? [selection] (selection->tile-map selection))
  (run [selection user-data] (add-layer-handler (selection->tile-map selection))))

(defn- erase-tool-handler [tool-controller]
  (g/set-property! tool-controller :brush empty-brush))

(defn- active-tile-map [app-view evaluation-context]
  (when-let [resource-node (g/node-value app-view :active-resource-node evaluation-context)]
    (when (g/node-instance? TileMapNode resource-node)
      resource-node)))

(defn- active-scene-view
  ([app-view]
   (g/with-auto-evaluation-context evaluation-context
     (active-scene-view app-view evaluation-context)))
  ([app-view evaluation-context]
   (when-let [view-node (g/node-value app-view :active-view evaluation-context)]
     (when (g/node-instance? scene/SceneView view-node)
       view-node))))

(defn- scene-view->tool-controller [scene-view]
  ;; TODO Hack, but better than before
  (let [input-handlers (map first (g/sources-of scene-view :input-handlers))]
    (first (filter (partial g/node-instance? TileMapController) input-handlers))))

(handler/defhandler :scene.select-erase-tool :workbench
  (active? [app-view evaluation-context]
           (and (active-tile-map app-view evaluation-context)
                (active-scene-view app-view evaluation-context)))
  (enabled? [app-view selection evaluation-context]
    (and (selection->layer selection)
         (-> (active-tile-map app-view evaluation-context)
             (g/node-value :tile-source-resource evaluation-context))))
  (run [app-view] (erase-tool-handler (-> (active-scene-view app-view) scene-view->tool-controller))))

(defn- tile-map-palette-handler [tool-controller]
  (g/update-property! tool-controller :mode (toggler :palette :editor)))

(handler/defhandler :scene.toggle-tile-palette :workbench
  (active? [app-view evaluation-context]
           (and (active-tile-map app-view evaluation-context)
                (active-scene-view app-view evaluation-context)))
  (enabled? [app-view selection evaluation-context]
            (and (selection->layer selection)
                 (let [active-tile (active-tile-map app-view evaluation-context)]
                   (and (g/node-value active-tile :tile-source-resource evaluation-context)
                        (not (g/error-value? (g/node-value active-tile :gpu-texture evaluation-context)))))))
  (run [app-view] (tile-map-palette-handler (-> (active-scene-view app-view) scene-view->tool-controller))))

(defn- transform-brush! [app-view transform-brush-fn]
  (let [scene-view (active-scene-view app-view)
        tool-controller (scene-view->tool-controller scene-view)]
    (g/update-property! tool-controller :brush transform-brush-fn)))

(handler/defhandler :scene.flip-brush-horizontally :workbench
  (active? [app-view evaluation-context]
           (and (active-tile-map app-view evaluation-context)
                (active-scene-view app-view evaluation-context)))
  (enabled? [app-view selection evaluation-context]
    (and (selection->layer selection)
         (-> (active-tile-map app-view evaluation-context)
             (g/node-value :tile-source-resource evaluation-context))))
  (run [app-view] (transform-brush! app-view flip-brush-horizontally)))

(handler/defhandler :scene.flip-brush-vertically :workbench
  (active? [app-view evaluation-context]
           (and (active-tile-map app-view evaluation-context)
                (active-scene-view app-view evaluation-context)))
  (enabled? [app-view selection evaluation-context]
            (and (selection->layer selection)
                 (-> (active-tile-map app-view evaluation-context)
                     (g/node-value :tile-source-resource evaluation-context))))
  (run [app-view] (transform-brush! app-view flip-brush-vertically)))

(handler/defhandler :scene.rotate-brush-90-degrees :workbench
  (active? [app-view evaluation-context]
           (and (active-tile-map app-view evaluation-context)
                (active-scene-view app-view evaluation-context)))
  (enabled? [app-view selection evaluation-context]
    (and (selection->layer selection)
         (-> (active-tile-map app-view evaluation-context)
             (g/node-value :tile-source-resource evaluation-context))))
  (run [app-view] (transform-brush! app-view rotate-brush-90-degrees)))

(handler/register-menu! ::menubar :editor.app-view/edit-end
  [{:label (localization/message "command.scene.toggle-tile-palette")
    :command :scene.toggle-tile-palette}
   {:label (localization/message "command.scene.select-erase-tool")
    :command :scene.select-erase-tool}
   {:label (localization/message "command.scene.flip-brush-horizontally")
    :command :scene.flip-brush-horizontally}
   {:label (localization/message "command.scene.flip-brush-vertically")
    :command :scene.flip-brush-vertically}
   {:label (localization/message "command.scene.rotate-brush-90-degrees")
    :command :scene.rotate-brush-90-degrees}])

(g/defnode TileMapGrid
  (inherits grid/Grid)
  (input grid-size g/Any :substitute nil)
  (output options g/Any (g/fnk [grid-size]
                          {:active-plane :z
                           :auto-scale false
                           :size {:x (or (first grid-size) 1.0)
                                  :y (or (second grid-size) 1.0)
                                  :z 1.0}})))

(defmethod scene/attach-grid ::TileMapGrid
  [_ grid-node-id view-id resource-node camera]
  (concat
    (g/connect grid-node-id :_node-id view-id :grid)
    (g/connect grid-node-id :renderable view-id :aux-renderables)
    (g/connect camera :camera grid-node-id :camera)
    (g/connect resource-node :tile-dimensions grid-node-id :grid-size)))

(defn register-resource-types [workspace]
  (concat
    (attachment/register
      workspace TileMapNode :layers
      :add {LayerNode attach-layer-node}
      :get attachment/nodes-getter)
    (resource-node/register-ddf-resource-type workspace
      :ext ["tilemap" "tilegrid"]
      :build-ext "tilemapc"
      :node-type TileMapNode
      :ddf-type Tile$TileGrid
      :load-fn load-tile-map
      :sanitize-fn sanitize-tile-map
      :icon tile-map-icon
      :icon-class :design
      :view-types [:scene :text]
      :view-opts {:scene {:grid TileMapGrid
                          :tool-controller TileMapController}}
      :tags #{:component :non-embeddable}
      :tag-opts {:component {:transform-properties #{:position :rotation}}}
      :label (localization/message "resource.type.tilemap"))))
