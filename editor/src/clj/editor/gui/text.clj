(ns editor.gui.text
  (:require [dynamo.graph :as g]
            [editor.gui.visual-node :as visual-node]
            [editor.properties :as properties]
            [editor.types :as types]
            [editor.font :as font]
            [editor.geom :as geom]
            [editor.gui.util :as gutil])
  (:import  [javax.vecmath Matrix4d Point3d Quat4d Vector3d]))

(def ^:private font-connections [[:name :font-input]
                                 [:gpu-texture :gpu-texture]
                                 [:font-map :font-map]
                                 [:font-data :font-data]
                                 [:font-shader :material-shader]])

(g/defnode TextNode
  (inherits visual-node/VisualNode)

  ; Text
  (property text g/Str)
  (property line-break g/Bool (default false))
  (property font g/Str
    (dynamic edit-type (g/fnk [font-ids] (properties/->choicebox (map first font-ids))))
    (value (g/fnk [font-input] font-input))
    (set (fn [basis self _ new-value]
           (let [font-ids (g/node-value self :font-ids :basis basis)]
             (concat
               (for [label (map second font-connections)]
                 (g/disconnect-sources self label))
               (if (contains? font-ids new-value)
                 (let [font-node (font-ids new-value)]
                   (for [[from to] font-connections]
                     (g/connect font-node from self to)))
                 []))))))
  (property text-leading g/Num)
  (property text-tracking g/Num)
  (property outline types/Color (default [1 1 1 1]))
  (property outline-alpha g/Num (default 1.0)
    (value (g/fnk [outline] (get outline 3)))
    (set (fn [basis self _ new-value]
          (g/update-property self :outline (fn [v] (assoc v 3 new-value)))))
    (dynamic edit-type (g/fnk [] {:type :slider
                                  :min 0.0
                                  :max 1.0
                                  :precision 0.01})))
  (property shadow types/Color (default [1 1 1 1]))
  (property shadow-alpha g/Num (default 1.0)
    (value (g/fnk [shadow] (get shadow 3)))
    (set (fn [basis self _ new-value]
          (g/update-property self :shadow (fn [v] (assoc v 3 new-value)))))
    (dynamic edit-type (g/fnk [] {:type :slider
                                  :min 0.0
                                  :max 1.0
                                  :precision 0.01})))

  (display-order (into gutil/base-display-order [:text :line-break :font :color :alpha :inherit-alpha :text-leading :text-tracking :outline :outline-alpha :shadow :shadow-alpha :layer]))

  (input font-input g/Str)
  (input font-map g/Any)
  (input font-data font/FontData)

  (input font-ids {g/Str g/NodeID})

  ;; Overloaded outputs
  (output scene-renderable-user-data g/Any :cached
          (g/fnk [aabb pivot color text-data]
                 (let [min (types/min-p aabb)
                       max (types/max-p aabb)
                       size [(- (.x max) (.x min)) (- (.y max) (.y min))]
                       [w h _] size
                       offset (gutil/pivot-offset pivot size)
                       lines (mapv conj (apply concat (take 4 (partition 2 1 (cycle (geom/transl offset [[0 0] [w 0] [w h] [0 h]]))))) (repeat 0))]
                   {:line-data lines
                    :color color
                    :text-data (when-let [font-data (get text-data :font-data)]
                                 (assoc text-data :offset (let [[x y] offset]
                                                            [x (+ y (- h (get-in font-data [:font-map :max-ascent])))])))})))
  (output aabb-size g/Any :cached (g/fnk [size font-map text line-break text-leading text-tracking]
                                         (font/measure font-map text line-break (first size) text-tracking text-leading)))
  (output text-data {g/Keyword g/Any} (g/fnk [text font-data line-break outline shadow size pivot text-leading text-tracking]
                                        {:text text :font-data font-data
                                         :line-break line-break :outline outline :shadow shadow :max-width (first size)
                                         :text-leading text-leading :text-tracking text-tracking :align (gutil/pivot->h-align pivot)})))
