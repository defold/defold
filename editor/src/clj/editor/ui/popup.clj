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

(ns editor.ui.popup
  (:require [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.colors :as colors]
            [editor.math :as math]
            [editor.prefs :as prefs]
            [editor.ui :as ui]
            [util.eduction :as e])
  (:import [antlr.collections List]
           [com.sun.javafx.util Utils]
           [javafx.css Styleable]
           [javafx.event ActionEvent]
           [javafx.geometry HPos Point2D Pos VPos]
           [javafx.scene Node Parent]
           [javafx.scene.control Button CheckBox Control Label PopupControl Skin Slider TextField ToggleButton ToggleGroup]
           [javafx.scene.layout HBox Priority Region StackPane VBox]
           [javafx.scene.paint Color]
           [javafx.stage PopupWindow$AnchorLocation]))

(set! *warn-on-reflection* true)

(defonce ^List axes [:x :y :z])

(defn make-popup
  ^PopupControl [^Styleable owner ^Node content]
  (let [popup (proxy [PopupControl] []
                (getStyleableParent [] owner))
        *skinnable (atom popup)
        popup-skin (reify Skin
                     (getSkinnable [_] @*skinnable)
                     (getNode [_] content)
                     (dispose [_] (reset! *skinnable nil)))]
    (doto popup
      (.setSkin popup-skin)
      (.setConsumeAutoHidingEvents true)
      (.setAutoHide true)
      (.setAutoFix true)
      (.setHideOnEscape true))))

(defmulti settings-row (fn [_app-view _prefs _prefs-path _popup option] option))

(defn- ensure-focus-traversable! [^Control control]
  (ui/observe (.focusTraversableProperty control)
    (fn [_ _ _]
      (.setFocusTraversable control true))))

(defn slider-setting
  ([app-view prefs ^PopupControl popup option prefs-path label-text range-min range-max]
   (slider-setting app-view prefs ^PopupControl popup option prefs-path label-text range-min range-max nil))
  ([app-view prefs ^PopupControl popup option prefs-path label-text range-min range-max on-change-fn]
   (let [prefs-path (conj prefs-path option)
         value (prefs/get prefs prefs-path)
         slider (Slider. range-min range-max value)
         label (Label. label-text)]
     (doto slider
       (ensure-focus-traversable!)
       (.setBlockIncrement 0.1)
       ;; Hacky way to fix a Linux specific issue that interferes with mouse events,
       ;; when autoHide is set to true.
       (.setOnMouseEntered (ui/event-handler e (.setAutoHide popup false)))
       (.setOnMouseExited (ui/event-handler e (.setAutoHide popup true))))

     (ui/observe
       (.valueProperty slider)
       (fn [_observable _old-val new-val]
         (let [val (math/round-with-precision new-val 0.01)]
           (prefs/set! prefs prefs-path val)
           ;; TODO: Maybe pass some extra arguments
           (when on-change-fn (on-change-fn)))))
     [label slider])))

(defn toggle-setting
  ([app-view prefs _popup option prefs-path label-text]
   (toggle-setting app-view prefs _popup option prefs-path label-text nil))
  ([app-view prefs _popup option prefs-path label-text on-change-fn]
   (let [prefs-path (conj prefs-path option)
         value (prefs/get prefs prefs-path)
         check-box (CheckBox.)
         label (Label. label-text)]
     (doto check-box
       (ui/value! value)
       (ui/remove-style! "check-box")
       (ui/add-style! "slide-switch")
       (ensure-focus-traversable!)
       (ui/on-action! (fn [_]
                        (prefs/set! prefs prefs-path (ui/value check-box))
                        (when on-change-fn (on-change-fn)))))
     (HBox/setHgrow label Priority/ALWAYS)
     (ui/add-style! label "slide-switch-label")
     [label check-box])))

(defn- vec3-group
  [app-view prefs prefs-path on-change-fn axis]
  (let [text-field (TextField.)
        label (Label. (string/upper-case (name axis)))
        size-val (str (get (prefs/get prefs prefs-path) axis))
        cancel-fn (fn [_] (ui/text! text-field size-val))
        update-fn (fn [_] (try
                            (let [value (Float/parseFloat (.getText text-field))]
                              (if (pos? value)
                                (do (prefs/set! prefs (conj prefs-path axis) value)
                                    (ui/text! text-field (str value))
                                    (when on-change-fn (on-change-fn)))
                                (cancel-fn nil)))
                            (catch Exception _e
                              (cancel-fn nil))))]
    (doto text-field
      (ui/text! size-val)
      (ui/customize! update-fn cancel-fn)
      (ensure-focus-traversable!))
    [label text-field]))

(defn vec3-floats-setting [app-view prefs prefs-path _popup option on-change-fn]
  (let [prefs-path (conj prefs-path option)]
    (into []
          (comp (map (partial vec3-group app-view prefs prefs-path on-change-fn))
                (mapcat identity))
          axes)))

(defn- plane-toggle-button
  [prefs plane-group prefs-path plane]
  (let [active-plane (prefs/get prefs prefs-path)]
    (doto (ToggleButton. (string/upper-case (name plane)))
      (ensure-focus-traversable!)
      (.setToggleGroup plane-group)
      (.setSelected (= plane active-plane))
      (ui/add-style! "plane-toggle"))))

(defn vec3-toggle-setting [app-view prefs prefs-path _popup option label-text on-change-fn]
  (let [prefs-path (conj prefs-path option)
        plane-group (ToggleGroup.)
        buttons (mapv (partial plane-toggle-button prefs plane-group prefs-path) axes)
        label (Label. label-text)]
    (ui/observe (.selectedToggleProperty plane-group)
                (fn [_ ^ToggleButton old-value ^ToggleButton new-value]
                  (if new-value
                    (do (let [active-plane (-> (.getText new-value)
                                               string/lower-case
                                               keyword)]
                          (prefs/set! prefs prefs-path active-plane))
                        (when on-change-fn (on-change-fn)))
                    (.setSelected old-value true))))
    (concat [label] buttons)))

(defn color-setting [app-view prefs prefs-path _popup option on-change-fn]
  (let [prefs-path (conj prefs-path option)
        text-field (TextField.)
        [r g b a] (prefs/get prefs prefs-path)
        color (->> (Color. r g b a) (.toString) nnext (drop-last 2) (apply str "#"))
        label (Label. "Color")
        cancel-fn (fn [_] (ui/text! text-field color))
        update-fn (fn [_] (try
                            (if-let [value (some-> (.getText text-field) colors/hex-color->color)]
                              (do (prefs/set! prefs prefs-path value)
                                  (when on-change-fn (on-change-fn)))
                              (cancel-fn nil))
                            (catch Exception _e
                              (cancel-fn nil))))]
    (doto text-field
      (ui/text! color)
      (ui/customize! update-fn cancel-fn)
      (ensure-focus-traversable!))
    [label text-field]))

(declare settings)

(defn- reset-button [app-view prefs ^PopupControl popup prefs-path settings-paths hidden-settings reset-callback]
  (let [button (doto (Button. "Reset to Defaults")
                 (.setPrefWidth Double/MAX_VALUE))
        reset-fn
        (fn [^ActionEvent event]
          (let [target ^Node (.getTarget event)
                parent (.getParent target)]
            (doseq [path settings-paths]
              (let [path (into prefs-path path)]
                (prefs/set! prefs path (:default (prefs/schema prefs path)))))
            (when reset-callback (reset-callback))
            (doto parent
              (ui/children! (ui/node-array (settings app-view prefs popup prefs-path settings-paths hidden-settings reset-callback)))
              (.requestFocus))))]
    (doto button
      (ui/on-action! reset-fn)
      (ensure-focus-traversable!))
    button))

(defn- settings
  [app-view prefs popup prefs-path settings-paths hidden-settings reset-callback]
  (let [reset-btn (reset-button app-view prefs popup prefs-path settings-paths hidden-settings reset-callback)]
    (->> settings-paths
         (e/map first)
         (e/distinct)
         (e/remove (partial contains? hidden-settings))
         (reduce (fn [rows setting]
                   (conj rows (doto (HBox. 5 (ui/node-array (settings-row app-view prefs prefs-path popup setting)))
                                (.setAlignment Pos/CENTER))))
                 [reset-btn]))))

(defn- pref-popup-position
  ^Point2D [^Parent container]
  (Utils/pointRelativeTo container 0 0 HPos/RIGHT VPos/BOTTOM 0.0 10.0 true))

;; TODO: We have to check whether any other settings popup is already open
(defn show-settings!
  ([app-view ^Parent owner prefs prefs-path settings-paths]
   (show-settings! app-view ^Parent owner prefs prefs-path settings-paths nil nil))
  ([app-view ^Parent owner prefs prefs-path settings-paths hidden-settings reset-callback]
   (if-let [popup ^PopupControl (ui/user-data owner ::popup)]
     (.hide popup)
     (let [region (StackPane.)
           popup (make-popup owner region)
           anchor ^Point2D (pref-popup-position (.getParent owner))]
       (ui/children! region [(doto (Region.)
                               (ui/add-style! "popup-shadow"))
                             (doto (VBox. 10 (ui/node-array (settings app-view prefs popup prefs-path settings-paths hidden-settings reset-callback)))
                               (.setFocusTraversable true)
                               (ensure-focus-traversable!)
                               (ui/add-style! "grid-settings"))])
       (ui/user-data! owner ::popup popup)
       (doto popup
         (.setAnchorLocation PopupWindow$AnchorLocation/CONTENT_TOP_RIGHT)
         (ui/on-closed! (fn [_] (ui/user-data! owner ::popup nil)))
         (.show owner (.getX anchor) (.getY anchor)))))))
