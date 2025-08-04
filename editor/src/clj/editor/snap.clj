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

(ns editor.snap
  (:require [dynamo.graph :as g]
            [editor.geom :as geom]
            [editor.gl.pass :as pass]
            [editor.handler :as handler]
            [editor.ui :as ui]
            [editor.ui.popup :as popup])
  (:import [com.jogamp.opengl GL2]
           [com.sun.javafx.util Utils]
           [javax.vecmath Matrix4d]
           [javafx.geometry HPos Point2D VPos]
           [javafx.scene Parent]
           [javafx.scene.control PopupControl]
           [javafx.scene.layout Region StackPane]
           [javafx.stage PopupWindow$AnchorLocation]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn render-snapped-point
  [^GL2 gl render-args renderables n])

(g/defnk produce-renderables
  [grid-points]
  {pass/overlay [{:world-transform (Matrix4d. geom/Identity4d)
                  :render-fn render-snapped-point
                  :user-data {}}]})

(g/defnode SnapNode
  (input grid-points g/Any)
  (output renderable pass/RenderData produce-renderables))

(defn- pref-popup-position
  ^Point2D [^Parent container]
  (Utils/pointRelativeTo container 0 0 HPos/RIGHT VPos/BOTTOM 0.0 10.0 true))

(defn show-settings! [app-view ^Parent owner prefs]
  (if-let [popup ^PopupControl (ui/user-data owner ::popup)]
    (.hide popup)
    (let [region (StackPane.)
          popup (popup/make-popup owner region)
          anchor ^Point2D (pref-popup-position (.getParent owner))]
      (ui/children! region [(doto (Region.)
                              (ui/add-style! "popup-shadow"))])
      (ui/user-data! owner ::popup popup)
      (doto popup
        (.setAnchorLocation PopupWindow$AnchorLocation/CONTENT_TOP_RIGHT)
        (ui/on-closed! (fn [_] (ui/user-data! owner ::popup nil)))
        (.show owner (.getX anchor) (.getY anchor))))))


(handler/defhandler :scene.snap.toggle :workbench
  (active? [] true)
  (run [])
  (state [] true))
