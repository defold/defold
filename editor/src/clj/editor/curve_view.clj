;; Copyright 2020-2024 The Defold Foundation
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

(ns editor.curve-view
  (:require [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.background :as background]
            [editor.camera :as camera]
            [editor.colors :as colors]
            [editor.curve-grid :as curve-grid]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.pass :as pass]
            [editor.gl.shader :as shader]
            [editor.gl.vertex :as vtx]
            [editor.handler :as handler]
            [editor.properties :as properties]
            [editor.rulers :as rulers]
            [editor.scene :as scene]
            [editor.scene-selection :as selection]
            [editor.types :as types]
            [editor.ui :as ui]
            [util.id-vec :as iv])
  (:import [com.jogamp.opengl GL GL2 GLAutoDrawable]
           [editor.properties Curve CurveSpread]
           [editor.types AABB Rect Region]
           [java.lang Runnable]
           [javafx.beans.property SimpleBooleanProperty]
           [javafx.scene Node Parent]
           [javafx.scene.control ListCell ListView SelectionMode]
           [javafx.scene.control.cell CheckBoxListCell]
           [javafx.scene.image ImageView]
           [javafx.scene.layout AnchorPane]
           [javafx.util Callback StringConverter]
           [javax.vecmath Matrix4d Point3d Vector3d Vector4d]))

(set! *warn-on-reflection* true)

(def ^:private ^:dynamic *programmatic-selection* nil)

; Line shader

(vtx/defvertex color-vtx
  (vec3 position)
  (vec4 color))

(shader/defshader line-vertex-shader
  (attribute vec4 position)
  (attribute vec4 color)
  (varying vec4 var_color)
  (defn void main []
    (setq gl_Position (* gl_ModelViewProjectionMatrix position))
    (setq var_color color)))

(shader/defshader line-fragment-shader
  (varying vec4 var_color)
  (defn void main []
    (setq gl_FragColor var_color)))

(def line-shader (shader/make-shader ::line-shader line-vertex-shader line-fragment-shader))

(defn render-curves [^GL2 gl render-args renderables rcount]
  (let [camera (:camera render-args)
        viewport (:viewport render-args)]
    (doseq [renderable renderables
            :let [screen-tris (get-in renderable [:user-data :screen-tris])
                  world-lines (get-in renderable [:user-data :world-lines])]]
      (when world-lines
        (let [vcount (count world-lines)
              vertex-binding (vtx/use-with ::lines world-lines line-shader)]
          (gl/with-gl-bindings gl render-args [line-shader vertex-binding]
            (gl/gl-draw-arrays gl GL/GL_LINES 0 vcount))))
      (when screen-tris
        (let [vcount (count screen-tris)
              vertex-binding (vtx/use-with ::tris screen-tris line-shader)]
          (gl/with-gl-bindings gl render-args [line-shader vertex-binding]
            (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 vcount)))))))

(defn- has-control-points? [[_ prop-info]]
  (let [value (:value prop-info)]
    (and (some? value)
         (let [type (g/value-type-dispatch-value (:type prop-info))]
           (if (or (= type Curve) (= type CurveSpread))
             (< 1 (properties/curve-point-count value))
             false)))))

(def ^:private x-steps 100)
(def ^:private xs (reduce (fn [res s] (conj res (double (/ s (- x-steps 1))))) [] (range x-steps)))

(g/defnk produce-curve-renderables [visible-curves]
  (let [splines+colors (mapv (fn [{:keys [node-id property curve hue]}]
                               (let [spline (->> curve
                                              (mapv second)
                                              (properties/->spline))
                                     color (colors/hsl->rgba hue 1.0 0.7)]
                                 [spline color])) visible-curves)
        curve-vs (reduce (fn [res [spline color]]
                           (let [[r g b a] color]
                             (->> (map #(properties/spline-cp spline %) xs)
                               (partition 2 1)
                               (reduce (fn [res [[x0 y0] [x1 y1]]]
                                         (-> res
                                           (conj [x0 y0 0.0 r g b a])
                                           (conj [x1 y1 0.0 r g b a]))) res))))
                         [] splines+colors)
        curve-vcount (count curve-vs)
        world-lines (when (< 0 curve-vcount)
                      (let [vb (->color-vtx curve-vcount)]
                        (doseq [v curve-vs]
                          (conj! vb v))
                        (persistent! vb)))
        renderables [{:render-fn render-curves
                      :batch-key nil
                      :user-data {:world-lines world-lines}}]]
    (into {} (map #(do [% renderables]) [pass/transparent]))))

(def ^:private tangent-length 40)

(g/defnk produce-cp-renderables [visible-curves viewport camera sub-selection-map]
  (let [sub-sel sub-selection-map
        scale (camera/scale-factor camera viewport)
        splines (mapv (fn [{:keys [node-id property curve]}]
                        (let [sel (get sub-sel [node-id property])
                              control-points (->> curve
                                               (sort-by (comp first second)))
                              order (into {} (map-indexed (fn [i [idx _]] [idx i]) control-points))]
                          [(properties/->spline (map second control-points)) (into #{} (map order sel))])) visible-curves)
        scount (count splines)
        color-hues (mapv :hue visible-curves)
        cp-r 4.0
        quad (let [[v0 v1 v2 v3] (vec (for [x [(- cp-r) cp-r]
                                            y [(- cp-r) cp-r]]
                                        [x y 0.0]))]
               (geom/scale scale [v0 v1 v2 v2 v1 v3]))
        cp-vs (mapcat (fn [[spline sel] hue]
                        (let [s 0.7
                              l 0.5]
                          (->> spline
                            (map-indexed (fn [i [x y tx ty]]
                                           (let [v (geom/transl [x y 0.0] quad)
                                                 selected? (contains? sel i)
                                                 s (if selected? 1.0 s)
                                                 l (if selected? 0.9 l)
                                                 c (colors/hsl->rgba hue s l)]
                                             (mapv (fn [v] (reduce conj v c)) v))))
                            (mapcat identity))))
                      splines color-hues)
        [sx sy] scale
        [tangent-vs line-vs] (let [s 1.0
                                l 0.9
                                cps (mapcat (fn [[spline sel] hue]
                                          (let [first-i 0
                                                last-i (dec (count spline))
                                                c (colors/hsl->rgba hue s l)]
                                            (->> sel
                                              (map (fn [i]
                                                     (if-let [[x y tx ty] (get spline i)]
                                                       (let [[tx ty] (let [v (doto (Vector3d. (/ tx sx) (/ ty sy) 0.0)
                                                                               (.normalize)
                                                                               (.scale tangent-length))]
                                                                       [(* (.x v) sx) (* (.y v) sy)])]
                                                         (cond-> []
                                                           (< i last-i) (conj [[x y 0.0] [(+ x tx) (+ y ty) 0.0] c])
                                                           (> i first-i) (conj [[x y 0.0] [(- x tx) (- y ty) 0.0] c])))
                                                       [])))
                                              (mapcat identity))))
                                       splines color-hues)]
                            [(mapcat (fn [[s cp c]]
                                      (->> (geom/transl cp quad)
                                        (mapv (fn [v] (reduce conj v c)))))
                                    cps)
                             (mapcat (fn [[s cp c]] [(reduce conj s c) (reduce conj cp c)]) cps)])
        cp-vs (into cp-vs tangent-vs)
        cp-vcount (count cp-vs)
        screen-tris (when (< 0 cp-vcount)
                      (let [vb (->color-vtx cp-vcount)]
                        (doseq [v cp-vs]
                          (conj! vb v))
                        (persistent! vb)))
        line-vcount (count line-vs)
        world-lines (when (< 0 line-vcount)
                      (let [vb (->color-vtx line-vcount)]
                        (doseq [v line-vs]
                          (conj! vb v))
                        (persistent! vb)))
        renderables [{:render-fn render-curves
                     :batch-key nil
                     :user-data {:screen-tris screen-tris
                                 :world-lines world-lines}}]]
    (into {} (map #(do [% renderables]) [pass/transparent]))))

(defn- prop-kw->hue [prop-kw]
  (let [prop-name (name prop-kw)]
    (cond
      (string/includes? prop-name "red") 0.0
      (string/includes? prop-name "green") 120.0
      (string/includes? prop-name "blue") 240.0
      (string/includes? prop-name "alpha") 60.0)))

(g/defnk produce-curves [selected-node-properties]
  (let [curves (mapcat (fn [p] (->> (:properties p)
                                 (filter has-control-points?)
                                 (map (fn [[k p]] {:node-id (:node-id p)
                                                   :property k
                                                   :curve (iv/iv-entries (:points (:value p)))}))))
                       selected-node-properties)
        ccount (count curves)
        hue-f (/ 360.0 ccount)
        curves (map-indexed (fn [i c]
                              (assoc c :hue (or (prop-kw->hue (:property c))
                                                (* (+ i 0.5) hue-f))))
                            curves)]
    curves))

(defn- aabb-contains? [^AABB aabb ^Point3d p]
  (let [min-p (types/min-p aabb)
        max-p (types/max-p aabb)]
    (and (<= (.x min-p) (.x p) (.x max-p))
         (<= (.y min-p) (.y p) (.y max-p)))))

(defn- reset-controller! [controller op-seq]
  (g/transact
    (concat
      (g/operation-sequence op-seq)
      (g/set-property controller :start nil)
      (g/set-property controller :current nil)
      (g/set-property controller :op-seq nil)
      (g/set-property controller :handle nil)
      (g/set-property controller :initial-evaluation-context nil))))

(defn handle-input [self action user-data]
  (let [^Point3d start      (g/node-value self :start)
        ^Point3d current    (g/node-value self :current)
        op-seq     (g/node-value self :op-seq)
        handle    (g/node-value self :handle)
        sub-selection (g/node-value self :sub-selection)
        ^Point3d cursor-pos (:world-pos action)]
    (case (:type action)
      :mouse-pressed (let [handled? (when-let [[handle data] (g/node-value self :curve-handle)]
                                      (if (and (= 2 (:click-count action))
                                            (or (= handle :control-point) (= handle :curve)))
                                        (do
                                          (g/transact
                                            (concat
                                              (g/operation-sequence op-seq)
                                              (case handle
                                                :control-point
                                                (let [[nid property id] data]
                                                  (g/update-property nid property types/geom-delete [id]))

                                                :curve
                                                (let [[nid property ^Point3d p] data
                                                      p [(.x p) (.y p) (.z p)]
                                                      new-curve (-> (g/node-value nid property)
                                                                  (types/geom-insert [p]))
                                                      id (iv/iv-added-id (:points new-curve))
                                                      select-fn (g/node-value self :select-fn)]
                                                  (select-fn [[nid property id]] op-seq)
                                                  (g/set-property nid property new-curve)))))
                                          (reset-controller! self op-seq)
                                          true)
                                        (when (or (= handle :control-point) (= handle :tangent))
                                          (let [op-seq (gensym)
                                                sel-mods? (some #(get action %) selection/toggle-modifiers)]
                                            (when (not sel-mods?)
                                              (when (and (= handle :control-point)
                                                         (not (contains? (set sub-selection) data)))
                                                (let [select-fn (g/node-value self :select-fn)
                                                      sub-selection [data]]
                                                  (select-fn sub-selection op-seq)))
                                              (g/transact
                                                (concat
                                                  (g/operation-sequence op-seq)
                                                  (g/set-property self :op-seq op-seq)
                                                  (g/set-property self :start cursor-pos)
                                                  (g/set-property self :current cursor-pos)
                                                  (g/set-property self :handle handle)
                                                  (g/set-property self :handle-data data)
                                                  (g/set-property self :initial-evaluation-context (atom (g/make-evaluation-context)))))
                                              true)))))]
                       (if handled? nil action))
      :mouse-released (do
                        (reset-controller! self op-seq)
                        (if handle
                          nil
                          action))
      :mouse-moved (case handle
                     :control-point (let [evaluation-context @(g/node-value self :initial-evaluation-context)
                                          delta (doto (Vector3d.)
                                                  (.sub cursor-pos start))
                                          trans (doto (Matrix4d.)
                                                  (.set delta))
                                          selected-ids (reduce (fn [ids [nid prop idx]]
                                                                 (update ids [nid prop] (fn [v] (conj (or v []) idx))))
                                                               {} sub-selection)]
                                      (g/transact
                                        (concat
                                          (g/operation-sequence op-seq)
                                          (g/set-property self :current cursor-pos)
                                          (for [[[nid prop] ids] selected-ids
                                                :let [curve (g/node-value nid prop evaluation-context)]]
                                            (g/set-property nid prop (types/geom-transform curve ids trans)))))
                                      nil)
                     :tangent (let [evaluation-context @(g/node-value self :initial-evaluation-context)
                                    [nid prop idx] (g/node-value self :handle-data)
                                    new-curve (-> (g/node-value nid prop evaluation-context)
                                                (types/geom-update [idx]
                                                                   (fn [cp]
                                                                     (let [[x y tx ty] cp
                                                                           t (doto (Vector3d. cursor-pos)
                                                                               (.sub (Vector3d. x y 0.0))
                                                                               (.setZ 0.0))]
                                                                       (when (< (.x t) 0.0)
                                                                         (.negate t))
                                                                       (when (< (.x t) 0.001)
                                                                         (.setX t 0.001))
                                                                       (.normalize t)
                                                                       [x y (.x t) (.y t)]))))]
                                (g/transact
                                  (concat
                                    (g/operation-sequence op-seq)
                                    (g/set-property nid prop new-curve)))
                                nil)
                     action)
      action)))

(g/defnode CurveController
  (property handle g/Keyword)
  (property handle-data g/Any)
  (property start Point3d)
  (property current Point3d)
  (property op-seq g/Any)
  (property initial-evaluation-context g/Any)
  (property select-fn Runnable)
  (input sub-selection g/Any)
  (input curve-handle g/Any)
  (output input-handler Runnable :cached (g/constantly handle-input))
  (output info-text g/Str (g/constantly nil)))

(defn- pick-control-points [visible-curves picking-rect camera viewport]
  (let [aabb (geom/centered-rect->aabb picking-rect)]
    (->> visible-curves
      (mapcat (fn [c]
                (->> (:curve c)
                  (filterv (fn [[idx cp]]
                             (let [[x y] cp
                                   p (doto (->> (Point3d. x y 0.0)
                                             (camera/camera-project camera viewport))
                                       (.setZ 0.0))]
                               (aabb-contains? aabb p))))
                  (mapv (fn [[idx _]] [(:node-id c) (:property c) idx])))))
      (keep identity))))

(defn- pick-tangent [visible-curves ^Rect picking-rect camera viewport sub-selection-map]
  (let [aabb (geom/centered-rect->aabb picking-rect)
        [scale-x scale-y] (camera/scale-factor camera viewport)]
    (some (fn [c]
            (when-let [sel (get sub-selection-map [(:node-id c) (:property c)])]
              (let [cps (filterv (comp sel first) (:curve c))]
                (some (fn [[idx cp]]
                        (let [[x y tx ty] cp
                              t (doto (Vector3d. (/ tx scale-x) (/ (- ty) scale-y) 0.0)
                                  (.normalize)
                                  (.scale tangent-length))
                              p (doto (camera/camera-project camera viewport (Point3d. x y 0.0))
                                  (.setZ 0.0))
                              p0 (doto (Point3d. p)
                                     (.add t))
                              p1 (doto (Point3d. p)
                                     (.sub t))]
                          (when (or (aabb-contains? aabb p0)
                                    (aabb-contains? aabb p1))
                            [(:node-id c) (:property c) idx])))
                      cps))))
          visible-curves)))

(defn- pick-closest-curve [visible-curves ^Rect picking-rect camera viewport]
  (let [p (let [p (camera/camera-unproject camera viewport (.x picking-rect) (.y picking-rect) 0.0)]
            (Point3d. (.x p) (.y p) 0.0))
        min-distance (Double/MAX_VALUE)
        curve (second
                (reduce (fn [[min-dist closest-curve] {:keys [node-id property curve]}]
                          (let [s (-> (mapv second curve)
                                    (properties/->spline))
                                cp (properties/spline-cp s (.x p))
                                [x y] cp
                                closest (Point3d. x y 0.0)
                                dist (.distanceSquared p closest)]
                            (if (< dist min-dist)
                              [dist [node-id property closest]]
                              [min-dist closest-curve])))
                        [min-distance nil] visible-curves))
        [_ _ ^Point3d closest] curve
        screen-p (and closest (camera/camera-project camera viewport closest))]
    (when (and screen-p (< (.distanceSquared screen-p (Point3d. (.x picking-rect) (.y picking-rect) 0.0))
                           (* selection/min-pick-size selection/min-pick-size)))
      curve)))

(g/defnk produce-picking-selection [visible-curves picking-rect camera viewport]
  (pick-control-points visible-curves picking-rect camera viewport))

(defn- sub-selection->map [sub-selection]
  (reduce (fn [sel [nid prop idx]] (update sel [nid prop] (fn [v] (conj (or v #{}) idx))))
          {} sub-selection))

(defn- curve-aabb [aabb geom-aabbs]
  (let [aabbs (vals geom-aabbs)]
    (when (> (count aabbs) 1)
      (loop [aabbs aabbs
             aabb aabb]
        (if-let [[min max] (first aabbs)]
          (let [aabb (apply geom/aabb-incorporate aabb min)
                aabb (apply geom/aabb-incorporate aabb max)]
            (recur (rest aabbs) aabb))
          aabb)))))

(defn- geom-cloud [v]
  (when (satisfies? types/GeomCloud v)
    v))

(g/defnk produce-aabb [selected-node-properties]
  (reduce (fn [aabb props]
            (loop [props (:properties props)
                   aabb aabb]
              (if-let [[kw p] (first props)]
                (recur (rest props)
                  (or (some->> p
                        :value
                        geom-cloud
                        types/geom-aabbs
                        (curve-aabb aabb))
                    aabb))
                aabb)))
          geom/null-aabb
          selected-node-properties))

(g/defnk produce-selected-aabb [sub-selection-map selected-node-properties]
  (reduce (fn [aabb props]
            (loop [props (:properties props)
                   aabb aabb]
              (if-let [[kw p] (first props)]
                (let [aabb (or (when-let [ids (sub-selection-map [(:node-id p) kw])]
                                 (curve-aabb aabb (types/geom-aabbs (:value p) ids)))
                             aabb)]
                  (recur (rest props) aabb))
                aabb)))
          geom/null-aabb
          selected-node-properties))

(g/defnode CurveView
  (inherits scene/SceneRenderer)

  (property image-view ImageView)
  (property viewport Region (default (types/->Region 0 0 0 0)))
  (property play-mode g/Keyword)
  (property drawable GLAutoDrawable)
  (property picking-drawable GLAutoDrawable)
  (property async-copy-state g/Any)
  (property cursor-pos types/Vec2)
  (property tool-picking-rect Rect)
  (property list ListView)
  (property hidden-curves g/Any)
  (property input-action-queue g/Any (default []))
  (property updatable-states g/Any (default (atom {})))

  (input camera-id g/NodeID :cascade-delete)
  (input grid-id g/NodeID :cascade-delete)
  (input background-id g/NodeID :cascade-delete)
  (input input-handlers Runnable :array)
  (input selected-node-properties g/Any)
  (input tool-info-text g/Str)
  (input tool-renderables pass/RenderData :array)
  (input picking-rect Rect)
  (input sub-selection g/Any)

  (output info-text g/Str (g/fnk [scene tool-info-text]
                            (or tool-info-text (:info-text scene))))
  (output all-renderables g/Any :cached (g/fnk [aux-render-data curve-renderables cp-renderables tool-renderables]
                                          (reduce (partial merge-with into)
                                                  (:renderables aux-render-data)
                                                  (into [curve-renderables cp-renderables] tool-renderables))))
  (output curves g/Any :cached produce-curves)
  (output visible-curves g/Any :cached (g/fnk [curves hidden-curves] (remove #(contains? hidden-curves (:property %)) curves)))
  (output curve-renderables g/Any :cached produce-curve-renderables)
  (output cp-renderables g/Any :cached produce-cp-renderables)
  (output picking-selection g/Any :cached produce-picking-selection)
  (output selected-tool-renderables g/Any :cached (g/fnk [] {}))
  (output sub-selection-map g/Any :cached (g/fnk [sub-selection]
                                                 (sub-selection->map sub-selection)))
  (output aabb AABB :cached produce-aabb)
  (output selected-aabb AABB :cached produce-selected-aabb)
  (output curve-handle g/Any :cached (g/fnk [visible-curves tool-picking-rect camera viewport sub-selection-map]
                                            (if-let [cp (first (pick-control-points visible-curves tool-picking-rect camera viewport))]
                                              [:control-point cp]
                                              (if-let [tangent (pick-tangent visible-curves tool-picking-rect camera viewport sub-selection-map)]
                                                [:tangent tangent]
                                                (if-let [curve (pick-closest-curve visible-curves tool-picking-rect camera viewport)]
                                                  [:curve curve]
                                                  nil)))))
  (output update-list-view g/Any :cached (g/fnk [curves ^ListView list selected-node-properties sub-selection-map]
                                                (let [p (:properties (properties/coalesce selected-node-properties))
                                                      new-items (reduce (fn [res c]
                                                                          (if (contains? p (:property c))
                                                                            (conj res {:keyword (:property c)
                                                                                      :property (get p (:property c))
                                                                                      :hue (:hue c)})
                                                                            res))
                                                                        [] curves)
                                                      old-items (ui/user-data list ::items)
                                                      old-sel (ui/user-data list ::sub-selection)]
                                                  (when (or (not= new-items old-items)
                                                            (not= sub-selection-map old-sel))
                                                    (binding [*programmatic-selection* true]
                                                      (ui/user-data! list ::items new-items)
                                                      (ui/user-data! list ::sub-selection sub-selection-map)
                                                      (let [selected (reduce conj #{} (map second (keys sub-selection-map)))
                                                            selected-indices (reduce (fn [res [i v]]
                                                                                       (if (selected v) (conj res (int i)) res))
                                                                                     [] (map-indexed (fn [i v] [i (:keyword v)]) new-items))]
                                                        (ui/items! list new-items)
                                                        (if (empty? selected-indices)
                                                          (doto (.getSelectionModel list)
                                                                  (.selectRange 0 0))
                                                          (let [index (int (first selected-indices))
                                                                rest-indices (next selected-indices)
                                                                indices ^ints (int-array (or rest-indices 0))]
                                                            (doto (.getSelectionModel list)
                                                              (.selectIndices index indices))))))))))
  (output active-updatables g/Any (g/constantly [])))

(defonce view-state (atom nil))

(defn frame-selection [view selection animate?]
  (let [aabb (if (empty? selection)
               (g/node-value view :aabb)
               (g/node-value view :selected-aabb))]
    (when-not (geom/null-aabb? aabb)
      (let [camera (g/node-feeding-into view :camera)
            viewport (g/node-value view :viewport)
            local-cam (g/node-value camera :local-camera)
            end-camera (camera/camera-orthographic-frame-aabb-y local-cam viewport aabb)]
        (scene/set-camera! camera local-cam end-camera animate?)))))

(defn- camera-filter-fn [camera]
  (let [^Point3d p (:position camera)
        y (.y p)
        z (.z p)]
    (assoc camera
           :position (Point3d. 0.5 y z)
           :focus-point (Vector4d. 0.5 y 0.0 1.0)
           :fov-x 1.2)))

(defrecord SubSelectionProvider [app-view]
  handler/SelectionProvider
  (selection [this] (g/node-value app-view :sub-selection))
  (succeeding-selection [this] [])
  (alt-selection [this] []))

(defn- on-list-selection [app-view values]
  (when-not *programmatic-selection*
    (let [selection (loop [values values
                           res []]
                      (if-let [v (first values)]
                        (let [p (:property v)
                              kw (:key p)
                              res (loop [curves (map vector (:node-ids p) (:values p))
                                         res res]
                                    (if-let [[nid curve] (first curves)]
                                      (recur (rest curves)
                                             (reduce (fn [res id] (conj res [nid kw id]))
                                                     res (iv/iv-ids (:points curve))))
                                      res))]
                          (recur (rest values)
                                 res))
                        res))]
      (app-view/sub-select! app-view selection))))

(defn make-view!
  ([app-view graph ^Parent parent ^ListView list ^AnchorPane view opts]
    (let [view-id (make-view! app-view graph parent list view opts false)]
      (reset! view-state {:app-view app-view :graph graph :parent parent :list list :view view :opts opts :view-id view-id})
      view-id))
  ([app-view graph ^Parent parent ^ListView list ^AnchorPane view opts reloading?]
    (let [[node-id] (g/tx-nodes-added
                      (g/transact (g/make-nodes graph [view-id    [CurveView :list list :hidden-curves #{}]
                                                       controller [CurveController :select-fn (fn [selection op-seq] (app-view/sub-select! app-view selection op-seq))]
                                                       selection  [selection/SelectionController :select-fn (fn [selection op-seq] (app-view/sub-select! app-view selection op-seq))]
                                                       background background/Background
                                                       camera     [camera/CameraController :local-camera (or (:camera opts) (camera/make-camera :orthographic camera-filter-fn))]
                                                       grid       curve-grid/Grid
                                                       rulers     [rulers/Rulers]]
                                                (g/update-property camera :movements-enabled disj :tumble) ; TODO - pass in to constructor

                                                (g/connect camera :_node-id view-id :camera-id)
                                                (g/connect grid :_node-id view-id :grid-id)
                                                (g/connect camera :camera view-id :camera)
                                                (g/connect camera :camera grid :camera)
                                                (g/connect camera :input-handler view-id :input-handlers)
                                                (g/connect view-id :viewport camera :viewport)
                                                (g/connect grid :renderable view-id :aux-renderables)
                                                (g/connect background :_node-id view-id :background-id)
                                                (g/connect background :renderable view-id :aux-renderables)

                                                (g/connect app-view :selected-node-properties view-id :selected-node-properties)
                                                (g/connect app-view :sub-selection view-id :sub-selection)

                                                (g/connect view-id :curve-handle controller :curve-handle)
                                                (g/connect app-view :sub-selection controller :sub-selection)
                                                (g/connect controller :input-handler view-id :input-handlers)
                                                (g/connect controller :info-text view-id :tool-info-text)

                                                (g/connect selection            :renderable                view-id          :tool-renderables)
                                                (g/connect selection            :input-handler             view-id          :input-handlers)
                                                (g/connect selection            :picking-rect              view-id          :picking-rect)
                                                (g/connect view-id              :picking-selection         selection        :picking-selection)
                                                (g/connect app-view             :sub-selection             selection        :selection)

                                                (g/connect camera :camera rulers :camera)
                                                (g/connect rulers :renderables view-id :aux-renderables)
                                                (g/connect view-id :viewport rulers :viewport)
                                                (g/connect view-id :cursor-pos rulers :cursor-pos)
                                                (g/connect view-id :_node-id app-view :scene-view-ids))))]
      (when parent
        (let [^Node pane (scene/make-gl-pane! node-id opts)]
          (ui/context! parent :curve-view {:view-id node-id} (SubSelectionProvider. app-view))
          (ui/fill-control pane)
          (ui/children! view [pane])
          (let [converter (proxy [StringConverter] []
                            (fromString ^Object [s] nil)
                            (toString ^String [item] (if (:property item)
                                                       (properties/label (:property item))
                                                       "")))
                selected-callback (reify Callback
                                    (call ^ObservableValue [this item]
                                      (let [hidden-curves (g/node-value node-id :hidden-curves)]
                                        (doto (SimpleBooleanProperty. (not (contains? hidden-curves (:keyword item))))
                                          (ui/observe (fn [observable old new]
                                                        (let [kw (:keyword item)]
                                                          (if new
                                                            (g/update-property! node-id :hidden-curves disj kw)
                                                            (g/update-property! node-id :hidden-curves conj kw)))))))))]
            (let [items (ui/selection list)]
              (ui/observe-list list items (fn [_ values] (on-list-selection app-view values))))
            (doto (.getSelectionModel list)
              (.setSelectionMode SelectionMode/MULTIPLE))
            (.setCellFactory list
              (reify Callback
                (call ^ListCell [this list]
                  (proxy [CheckBoxListCell] [selected-callback converter]
                    (updateItem [item empty]
                      (let [this ^CheckBoxListCell this]
                        (proxy-super updateItem item empty)
                        (when (and item (not empty))
                          (let [[r g b] (colors/hsl->rgb (:hue item) 1.0 0.75)]
                            (proxy-super setStyle (format "-fx-text-fill: rgb(%d, %d, %d);" (int (* 255 r)) (int (* 255 g)) (int (* 255 b)))))))))))))))
      node-id)))

(defn- delete-cps [sub-selection]
  (let [m (sub-selection->map sub-selection)]
    (g/transact
      (for [[[nid prop] idx] m]
        (g/update-property nid prop types/geom-delete idx)))))

(handler/defhandler :delete :curve-view
  (enabled? [selection] (not (empty? selection)))
  (run [selection] (delete-cps selection)))

(handler/defhandler :frame-selection :curve-view
  (run [view-id selection] (frame-selection view-id selection true)))
