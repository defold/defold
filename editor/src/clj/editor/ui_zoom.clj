;; Copyright 2020-2026 The Defold Foundation
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

(ns editor.ui-zoom
  (:require [editor.prefs :as prefs]
            [service.log :as log])
  (:import [com.defold.control ZoomPane]
           [javafx.beans.value ChangeListener]
           [javafx.collections ListChangeListener]
           [javafx.scene Node Parent Scene]
           [javafx.scene.transform Scale]
           [javafx.stage PopupWindow Stage Window]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defonce ^:private application-scale (atom 1.0))
(defonce ^:private popups-installed (atom false))

(defn startup-scale
  "Resolve the optional JVM override as a factor (1.5 means 150%). Missing or
  invalid overrides fall back to the saved preference so startup can continue."
  [preferred-scale override]
  (if-not override
    preferred-scale
    (try
      (let [scale (Double/parseDouble override)]
        (if (and (Double/isFinite scale) (<= 1.0 scale 3.0))
          scale
          (throw (NumberFormatException. "Expected a scale between 1.0 and 3.0"))))
      (catch NumberFormatException _
        (log/warn :message "Ignoring invalid defold.uiScale; expected a number between 1.0 and 3.0."
                  :value override)
        preferred-scale))))

(defn initialize!
  "Read the saved percentage and optional JVM override before creating windows.
  Each stage captures the factor; preference changes require a restart."
  [prefs]
  (reset! application-scale
          (startup-scale (/ (double (prefs/get prefs [:window :ui-scale])) 100.0)
                         (System/getProperty "defold.uiScale"))))

(defn- scale-popup-content! [^Parent root ^double scale]
  ;; PopupWindow sizes itself from its children's transformed bounds. Preserve
  ;; its root and content list, which are managed by JavaFX control skins.
  (doseq [^Node child (.getChildrenUnmodifiable root)]
    ;; Reused popup content must not accumulate transforms on each show.
    (when-not (.containsKey (.getProperties child) ::popup-scale)
      (.put (.getProperties child) ::popup-scale scale)
      (.add (.getTransforms child) (Scale. scale scale 0.0 0.0)))))

(defn window-scale
  "Application zoom for a window, excluding the operating system display scale.
  Popups follow their owner chain because they live outside the stage's scene."
  ^double [^Window window]
  (if (instance? PopupWindow window)
    (recur (.getOwnerWindow ^PopupWindow window))
    (let [root (.getRoot (.getScene window))]
      (if (instance? ZoomPane root) (.getZoom ^ZoomPane root) 1.0))))

(defn- install-popups!
  "Observe control-skin popups, which bypass our stage factory."
  []
  ;; Window/getWindows is application-wide, so one listener covers every stage.
  (when (compare-and-set! popups-installed false true)
    (.addListener (Window/getWindows)
                  (reify ListChangeListener
                    (onChanged [_ change]
                      (while (.next change)
                        (doseq [window (.getAddedSubList change)]
                          (when (instance? PopupWindow window)
                            (let [scale (window-scale window)
                                  root (.getRoot (.getScene ^PopupWindow window))]
                              (scale-popup-content! root scale)
                              (when-not (.containsKey (.getProperties root) ::popup-listener)
                                (.put (.getProperties root) ::popup-listener true)
                                ;; Skins may replace content after showing.
                                (.addListener (.getChildrenUnmodifiable root)
                                              (reify ListChangeListener
                                                (onChanged [_ _]
                                                  (scale-popup-content! root scale))))))))))))))

(defn- wrap-root!
  "Wrap the current root, preserving its identity for FXML and cljfx."
  [^Scene scene scale]
  (let [root (.getRoot scene)]
    ;; Setting the root below can notify our root listener again. This guard
    ;; makes the nested notification a no-op instead of adding another wrapper.
    (when-not (instance? ZoomPane root)
      (let [container (ZoomPane. scale)]
        ;; Detach the old root before making it a child of the new root.
        (.setRoot scene container)
        (.setContent container root)))))

(defn install!
  "Install application zoom before a stage receives its scene. At 100%, leave
  the scene graph untouched and avoid installing zoom listeners."
  [^Stage stage]
  (let [scale @application-scale]
    (when-not (= 1.0 scale)
      (install-popups!)
      (.addListener (.sceneProperty stage)
                    (reify ChangeListener
                      (changed [_ _ _ scene]
                        (when scene
                          (wrap-root! scene scale)
                          ;; cljfx can replace a scene's root on updates.
                          (.addListener (.rootProperty ^Scene scene)
                                        (reify ChangeListener
                                          (changed [_ _ _ _]
                                            (wrap-root! scene scale)))))))))))
