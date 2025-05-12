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

(ns editor.scene-selection
  (:require [dynamo.graph :as g]
            [editor.system :as system]
            [editor.geom :as geom]
            [editor.handler :as handler]
            [editor.types :as types]
            [editor.ui :as ui]
            [editor.gl.pass :as pass]
            [schema.core :as s])
  (:import [editor.types Rect]
           [java.lang Runnable Math]
           [com.jogamp.opengl GL2]
           [javafx.scene.input DragEvent]
           [javafx.scene Node Scene]
           [javax.vecmath Point2i Point3d Matrix4d]))

(set! *warn-on-reflection* true)

(handler/register-menu! ::scene-selection-menu
                        [{:label "Cut"
                          :command :edit.cut}
                         {:label "Copy"
                          :command :edit.copy}
                         {:label "Paste"
                          :command :edit.paste}
                         {:label "Delete"
                          :icon "icons/32/Icons_M_06_trash.png"
                          :command :edit.delete}
                         {:label :separator}
                         {:label "Show/Hide Objects"
                          :command :scene.visibility.toggle-selection}
                         {:label "Hide Unselected Objects"
                          :command :scene.visibility.hide-unselected}
                         {:label "Show All Hidden Objects"
                          :command :scene.visibility.show-all}
                         {:label :separator
                          :id ::context-menu-end}])

(defn render-selection-box [^GL2 gl _render-args renderables _count]
  (let [user-data (:user-data (first renderables))
        start (:start user-data)
        current (:current user-data)]
    (when (and start current)
     (let [min-fn (fn [v1 v2] (map #(Math/min ^Double %1 ^Double %2) v1 v2))
           max-fn (fn [v1 v2] (map #(Math/max ^Double %1 ^Double %2) v1 v2))
           min-p (reduce min-fn [start current])
           min-x (nth min-p 0)
           min-y (nth min-p 1)
           max-p (reduce max-fn [start current])
           max-x (nth max-p 0)
           max-y (nth max-p 1)
           z 0.0
           c (double-array (map #(/ % 255.0) [131 188 212]))]
       (.glColor3d gl (nth c 0) (nth c 1) (nth c 2))
       (.glBegin gl GL2/GL_LINE_LOOP)
       (.glVertex3d gl min-x min-y z)
       (.glVertex3d gl min-x max-y z)
       (.glVertex3d gl max-x max-y z)
       (.glVertex3d gl max-x min-y z)
       (.glEnd gl)

       (.glBegin gl GL2/GL_QUADS)
       (.glColor4d gl (nth c 0) (nth c 1) (nth c 2) 0.2)
       (.glVertex3d gl min-x, min-y, z);
       (.glVertex3d gl min-x, max-y, z);
       (.glVertex3d gl max-x, max-y, z);
       (.glVertex3d gl max-x, min-y, z);
       (.glEnd gl)))))

(defn- select [controller op-seq mode toggle?]
  (let [select-fn (g/node-value controller :select-fn)
        selection (g/node-value controller :picking-selection)
        contextual? (g/node-value controller :contextual?)
        mode-filter-fn (case mode
                        :single (fn [selection] (if-let [sel (first selection)] [sel] []))
                        :multi identity)
        toggle-filter-fn (cond
                           toggle?
                           (fn [selection]
                             (let [selection-set (set selection)
                                   prev-selection (g/node-value controller :prev-selection)
                                   prev-selection-set (set prev-selection)]
                               (into [] (concat (filter (complement selection-set) prev-selection) (filter (complement prev-selection-set) selection)))))

                           contextual?
                           (fn [selection]
                             (let [prev-selection (g/node-value controller :prev-selection)]
                               (if (some #(= (first selection) %) prev-selection)
                                 prev-selection
                                 selection)))
                           
                           :else
                           identity)
        sel-filter-fn (comp toggle-filter-fn mode-filter-fn)
        selection (or (not-empty (sel-filter-fn selection))
                      (filter #(not (nil? %)) [(g/node-value controller :root-id)]))]
    (select-fn selection op-seq)))

(def mac-toggle-modifiers #{:shift :meta})
(def other-toggle-modifiers #{:shift})
(def toggle-modifiers (if system/mac? mac-toggle-modifiers other-toggle-modifiers))

(def ^Integer min-pick-size 10)

(defn distance
  [[x0 y0 z0] [x1 y1 z1]]
  (.distance (Point3d. x0 y0 z0) (Point3d. x1 y1 z1)))

(defn handle-selection-input [self action _user-data]
  (let [start (g/node-value self :start)
        op-seq (g/node-value self :op-seq)
        mode (g/node-value self :mode)
        toggle? (g/node-value self :toggle?)
        cursor-pos [(:x action) (:y action) 0]
        contextual? (= (:button action) :secondary)]
    (case (:type action)
      :drag-dropped (when-let [drop-fn (g/node-value self :drop-fn)]
                      (let [op-seq (gensym)
                            select-fn (g/node-value self :select-fn)
                            drag-event ^DragEvent (:event action)
                            _ (ui/request-focus! (:gesture-target action))
                            added-nodes (drop-fn action op-seq)]
                        (.consume drag-event)
                        (when (seq added-nodes)
                          (select-fn added-nodes op-seq)
                          (ui/user-data! (ui/main-scene) ::ui/refresh-requested? true)
                          (.setDropCompleted drag-event true)))
                      nil)
      :mouse-pressed (let [op-seq (gensym)
                           toggle? (true? (some true? (map #(% action) toggle-modifiers)))
                           mode :single]
                       (g/transact
                         (concat
                           (g/set-property self :op-seq op-seq)
                           (g/set-property self :start cursor-pos)
                           (g/set-property self :current cursor-pos)
                           (g/set-property self :mode mode)
                           (g/set-property self :toggle? toggle?)
                           (g/set-property self :contextual? contextual?)
                           (g/set-property self :prev-selection (g/node-value self :selection))))
                       (select self op-seq mode toggle?)
                       (when contextual?
                         (let [node ^Node (:target action)
                               scene ^Scene (.getScene node)
                               context-menu (ui/init-context-menu! ::scene-selection-menu scene)]
                           (.show context-menu node ^double (:screen-x action) ^double (:screen-y action))))
                       nil)
      :mouse-released (do
                        (g/transact
                          (concat
                            (g/set-property self :start nil)
                            (g/set-property self :current nil)
                            (g/set-property self :op-seq nil)
                            (g/set-property self :mode nil)
                            (g/set-property self :toggle? nil)
                            (g/set-property self :contextual? nil)
                            (g/set-property self :prev-selection nil)))
                        nil)
      :mouse-moved (if (and start (not (g/node-value self :contextual?)))
                     (let [new-mode (if (and (= :single mode) (< min-pick-size (distance start cursor-pos)))
                                      :multi
                                      mode)]
                       (g/transact
                         (concat
                           (when (not= new-mode mode) (g/set-property self :mode new-mode))
                           (g/set-property self :current cursor-pos)))
                       (select self op-seq new-mode toggle?)
                       nil)
                     action)
      action)))

(defn- imin [^Integer v1 ^Integer v2] (Math/min v1 v2))
(defn- imax [^Integer v1 ^Integer v2] (Math/max v1 v2))

(defn calc-picking-rect
  "Returns a rect where .x, .y is center of rect spanned by
  points [start current] and .width, .height the corresponding
  dimensions"
  [start current]
  (let [ps [start current]
        min-p (Point2i. (reduce imin (map first ps)) (reduce imin (map second ps)))
        max-p (Point2i. (reduce imax (map first ps)) (reduce imax (map second ps)))
        dims (doto (Point2i. max-p) (.sub min-p))
        center (doto (Point2i. min-p) (.add (Point2i. (/ (.x dims) 2) (/ (.y dims) 2))))]
    (types/rect (.x center) (.y center) (Math/max (.x dims) min-pick-size) (Math/max (.y dims) min-pick-size))))

(g/deftype SelectionMode (s/enum :single :multi))

(g/defnode SelectionController
  (property select-fn Runnable)
  (property drop-fn Runnable)
  (property start types/Vec3)
  (property current types/Vec3)
  (property op-seq g/Any)
  (property mode SelectionMode)
  (property toggle? g/Bool)
  (property contextual? g/Bool)
  (property prev-selection g/Any)

  (input selection g/Any)
  (input picking-selection g/Any)
  (input root-id g/NodeID)

  (output picking-rect Rect :cached (g/fnk [start current] (calc-picking-rect start current)))
  (output renderable pass/RenderData :cached (g/fnk [start current] {pass/overlay [{:world-transform (Matrix4d. geom/Identity4d)
                                                                                    :render-fn render-selection-box
                                                                                    :user-data {:start start :current current}}]}))
  (output input-handler Runnable :cached (g/constantly handle-selection-input)))
