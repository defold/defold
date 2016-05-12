(ns editor.gui.shapes
  (:require [editor.gui.visual-node :as vn]
            [dynamo.graph :as g]
            [editor.properties :as properties]
            [editor.geom :as geom]
            [editor.gui.util :as gutil]
            [editor.types :as types])
  (:import [com.dynamo.gui.proto Gui$NodeDesc$ClippingMode Gui$NodeDesc$PieBounds]))


(g/defnode ShapeNode
  (inherits vn/VisualNode)

  (property texture g/Str
            (dynamic edit-type (g/fnk [texture-ids] (properties/->choicebox (cons "" (keys texture-ids)))))
            (value (g/fnk [texture-input animation]
                     (str texture-input (if (and animation (not (empty? animation))) (str "/" animation) ""))))
            (set (fn [basis self _ ^String new-value]
                   (let [textures (g/node-value self :texture-ids :basis basis)
                         animation (let [sep (.indexOf new-value "/")]
                                     (if (>= sep 0) (subs new-value (inc sep)) ""))]
                     (concat
                       (g/set-property self :animation animation)
                       (for [label [:texture-input :gpu-texture :anim-data]]
                         (g/disconnect-sources self label))
                       (if (contains? textures new-value)
                         (let [tex-node (textures new-value)]
                           (concat
                             (g/connect tex-node :name self :texture-input)
                             (g/connect tex-node :gpu-texture self :gpu-texture)
                             (g/connect tex-node :anim-data self :anim-data)))
                         []))))))

  (property clipping-mode g/Keyword (default :clipping-mode-none)
            (dynamic edit-type (g/fnk [] (properties/->pb-choicebox Gui$NodeDesc$ClippingMode))))
  (property clipping-visible g/Bool (default true))
  (property clipping-inverted g/Bool (default false))

  (input texture-input g/Str)
  (input anim-data g/Any)
  (input textures {g/Str g/NodeID})
  (input texture-ids {g/Str g/NodeID}))

;; Box nodes

(defn- pairs [v]
  (filter (fn [[v0 v1]] (> (Math/abs (double (- v1 v0))) 0)) (partition 2 1 v)))

(g/defnode BoxNode
  (inherits ShapeNode)

  (property slice9 types/Vec4 (default [0 0 0 0]))

  (display-order (into gutil/base-display-order
                       [:texture :slice9 :color :alpha :inherit-alpha :layer :blend-mode :pivot :x-anchor :y-anchor
                        :adjust-mode :clipping :visible-clipper :inverted-clipper]))

  ;; Overloaded outputs
  (output scene-renderable-user-data g/Any :cached
          (g/fnk [pivot size color slice9 texture anim-data]
                 (let [[w h _] size
                       offset (gutil/pivot-offset pivot size)
                       order [0 1 3 3 1 2]
                       x-vals (pairs [0 (get slice9 0) (- w (get slice9 2)) w])
                       y-vals (pairs [0 (get slice9 3) (- h (get slice9 1)) h])
                       corners (for [[x0 x1] x-vals
                                     [y0 y1] y-vals]
                                 (geom/transl offset [[x0 y0 0] [x0 y1 0] [x1 y1 0] [x1 y0 0]]))
                       vs (vec (mapcat #(map (partial nth %) order) corners))
                       tex (get anim-data texture)
                       tex-w (:width tex 1)
                       tex-h (:height tex 1)
                       u-vals (pairs [0 (/ (get slice9 0) tex-w) (- 1 (/ (get slice9 2) tex-w)) 1])
                       v-vals (pairs [1 (- 1 (/ (get slice9 3) tex-h)) (/ (get slice9 1) tex-h) 0])
                       uv-trans (get-in tex [:uv-transforms 0])
                       uvs (for [[u0 u1] u-vals
                                 [v0 v1] v-vals]
                             (geom/uv-trans uv-trans [[u0 v0] [u0 v1] [u1 v1] [u1 v0]]))
                       uvs (vec (mapcat #(map (partial nth %) order) uvs))
                       lines (vec (mapcat #(interleave % (drop 1 (cycle %))) corners))]
                   {:geom-data vs
                    :line-data lines
                    :uv-data uvs
                    :color color}))))

;; Pie nodes

(defn- sort-by-angle [ps max-angle]
  (-> (sort-by (fn [[x y _]] (let [a (* (Math/atan2 y x) (if (< max-angle 0) -1.0 1.0))]
                              (cond-> a
                                (< a 0) (+ (* 2.0 Math/PI))))) ps)
    (vec)))

(defn- cornify [ps ^double max-angle]
  (let [corner-count (int (/ (+ (Math/abs max-angle) 45) 90))]
    (if (> corner-count 0)
      (let [right (if (< max-angle 0) -90 90)
            half-right (if (< max-angle 0) -45 45)]
        (-> ps
         (into (geom/chain (dec corner-count) (partial geom/rotate [0 0 right]) (geom/rotate [0 0 half-right] [[1 0 0]])))
         (sort-by-angle max-angle)))
      ps)))

(defn- pie-circling [segments ^double max-angle corners? ps]
  (let [cut-off? (< (Math/abs max-angle) 360)
        angle (* (/ 360 segments) (if (< max-angle 0) -1.0 1.0))
        segments (if cut-off?
                   (int (/ max-angle angle))
                   segments)]
    (cond-> (geom/chain segments (partial geom/rotate [0 0 angle]) ps)
      corners? (cornify max-angle)
      cut-off? (into (geom/rotate [0 0 max-angle] ps)))))

(g/defnode PieNode
  (inherits ShapeNode)

  (property outer-bounds g/Keyword (default :piebounds-ellipse)
            (dynamic edit-type (g/fnk [] (properties/->pb-choicebox Gui$NodeDesc$PieBounds))))
  (property inner-radius g/Num (default 0.0))
  (property perimeter-vertices g/Num (default 10.0))
  (property pie-fill-angle g/Num (default 360.0))

  (display-order (into gutil/base-display-order
                       [:texture :inner-radius :outer-bounds :perimeter-vertices :pie-fill-angle
                        :color :alpha :inherit-alpha :layer :blend-mode :pivot :x-anchor :y-anchor
                        :adjust-mode :clipping :visible-clipper :inverted-clipper]))

  (output pie-data {g/Keyword g/Any} (g/fnk [outer-bounds inner-radius perimeter-vertices pie-fill-angle]
                                            {:outer-bounds outer-bounds :inner-radius inner-radius
                                             :perimeter-vertices perimeter-vertices :pie-fill-angle pie-fill-angle}))

  ;; Overloaded outputs
  (output scene-renderable-user-data g/Any :cached
          (g/fnk [pivot size color pie-data texture anim-data]
                 (let [[w h _] size
                       offset (mapv + (gutil/pivot-offset pivot size) [(* 0.5 w) (* 0.5 h) 0])
                       {:keys [outer-bounds inner-radius perimeter-vertices ^double pie-fill-angle]} pie-data
                       outer-rect? (= :piebounds-rectangle outer-bounds)
                       cut-off? (< (Math/abs pie-fill-angle) 360)
                       hole? (> inner-radius 0)
                       vs (pie-circling perimeter-vertices pie-fill-angle outer-rect? [[1 0 0]])
                       vs-outer (if outer-rect?
                                  (mapv (fn [[x y z]]
                                          (let [abs-x (Math/abs (double x))
                                                abs-y (Math/abs (double y))]
                                            (if (< abs-x abs-y)
                                              [(/ x abs-y) (/ y abs-y) z]
                                              [(/ x abs-x) (/ y abs-x) z]))) vs)
                                  vs)
                       vs-inner (if (> inner-radius 0)
                                  (let [xs (/ inner-radius w)
                                        ys (* xs (/ h w))]
                                    (geom/scale [xs ys 1] vs))
                                  [[0 0 0]])
                       lines (->> (cond-> (vec (apply concat (partition 2 1 vs-outer)))
                                    hole? (into (apply concat (partition 2 1 vs-inner)))
                                    cut-off? (into [(first vs-outer) (first vs-inner) (last vs-outer) (last vs-inner)]))
                                  (geom/scale [(* 0.5 w) (* 0.5 h) 1])
                                  (geom/transl offset))
                       vs (if hole?
                            (reduce into []
                                    (concat
                                     (map #(do [%1 (second %2) (first %2)]) vs-outer (partition 2 1 vs-inner))
                                     (map #(do [%1 (first %2) (second %2)]) (drop 1 (cycle vs-inner)) (partition 2 1 vs-outer))))
                            (vec (mapcat into (repeat vs-inner) (partition 2 1 vs-outer))))
                       uvs (->> vs
                                (map #(subvec % 0 2))
                                (geom/transl [1 1])
                                (geom/scale [0.5 -0.5])
                                (geom/transl [0 1])
                                (geom/uv-trans (get-in anim-data [texture :uv-transforms 0])))
                       vs (->> vs
                               (geom/scale [(* 0.5 w) (* 0.5 h) 1])
                               (geom/transl offset))]
                   {:geom-data vs
                    :line-data lines
                    :uv-data uvs
                    :color color}))))
