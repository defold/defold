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
            [editor.localization :as localization]
            [editor.math :as math]
            [editor.os :as os]
            [editor.prefs :as prefs]
            [editor.ui :as ui])
  (:import [antlr.collections List]
           [com.sun.javafx.util Utils]
           [javafx.css Styleable]
           [javafx.event ActionEvent]
           [javafx.geometry HPos Point2D Pos VPos]
           [javafx.scene Node Parent]
           [javafx.scene.control Button CheckBox Control Label PopupControl Separator Skin Slider TextField ToggleButton ToggleGroup]
           [javafx.scene.layout HBox Priority Region StackPane VBox]
           [javafx.scene.paint Color]
           [javafx.stage PopupWindow$AnchorLocation]))

(set! *warn-on-reflection* true)

(defonce ^List axes [:x :y :z])

(defprotocol SettingsBinding
  (get-value [this key])
  (set-value! [this key v])
  (reset-all! [this]))

(defrecord PrefsBinding [prefs prefs-prefix setting-descriptors hidden-settings on-change-fn]
  SettingsBinding
  (get-value [_ key] (prefs/get prefs (conj prefs-prefix key)))
  (set-value! [_ key v]
    (prefs/set! prefs (conj prefs-prefix key) v)
    (when on-change-fn (on-change-fn v)))
  (reset-all! [_]
    (doseq [{:keys [key type]} setting-descriptors
            :when (not (contains? hidden-settings key))
            :let [prefs-path (into prefs-prefix key)]]
      (prefs/set! prefs prefs-path (:default (prefs/schema prefs prefs-path))))
    (when on-change-fn (on-change-fn nil))))

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

(defn- ensure-focus-traversable! [^Node node]
  (ui/observe (.focusTraversableProperty node)
    (fn [_ _ _]
      (.setFocusTraversable node true))))

(defn- slider-setting [settings-binding ^PopupControl popup key label-text range-min range-max]
  (let [value (get-value settings-binding key)
        slider (Slider. range-min range-max value)
        label (Label. label-text)]
    (doto slider
      (ensure-focus-traversable!)
      (.setBlockIncrement 0.1)
      (.setOnMouseEntered (ui/event-handler e (.setAutoHide popup false)))
      (.setOnMouseExited (ui/event-handler e (.setAutoHide popup true))))

    (ui/observe
      (.valueProperty slider)
      (fn [_observable _old-val new-val]
        (let [val (math/round-with-precision new-val 0.01)]
          (set-value! settings-binding key val))))
    [label slider]))

(defn toggle-setting [settings-binding key label-text acc-text] ;; TODO JOE: Make this private
  (let [check-box (CheckBox.)
        label (Label. label-text)
        acc (Label. (or acc-text ""))]
    (doto check-box
      (ui/value! (get-value settings-binding key))
      (ui/remove-style! "check-box")
      (ui/add-style! "slide-switch")
      (ensure-focus-traversable!)
      (ui/on-action! (fn [_]
                       (let [value (ui/value check-box)]
                         (set-value! settings-binding key value)))))
    (HBox/setHgrow label Priority/ALWAYS)
    (ui/add-style! label "slide-switch-label")
    (when (os/is-mac-os?)
      (.setStyle acc "-fx-font-family: 'Lucida Grande';"))
    (ui/add-style! acc "accelerator-label")
    (let [hbox (doto (HBox.)
                 (.setAlignment Pos/CENTER_LEFT)
                 (ui/add-style! "toggle-row")
                 (ui/on-click! (fn [_]
                                 (let [value (ui/value check-box)]
                                   (ui/value! check-box (not value))
                                   (set-value! settings-binding key value))))
                 (ui/children! [label acc check-box]))]
      [hbox])))

(defn- vec3-group
  [settings-binding key axis]
  (let [text-field (TextField.)
        label (Label. (string/upper-case (name axis)))
        size-val (str (get (get-value settings-binding key) axis))
        cancel-fn (fn [_] (ui/text! text-field size-val))
        update-fn (fn [_] (try
                            (let [value (Float/parseFloat (.getText text-field))]
                              (if (pos? value)
                                (do
                                  (set-value! settings-binding key (assoc (get-value settings-binding key) axis value))
                                  (ui/text! text-field (str value)))
                                (cancel-fn nil)))
                            (catch Exception _e
                              (cancel-fn nil))))]
    (doto text-field
      (ui/text! size-val)
      (ui/customize! update-fn cancel-fn)
      (ensure-focus-traversable!))
    [label text-field]))

(defn- vec3-floats-setting [settings-binding key]
  (into []
    (comp (map (partial vec3-group settings-binding key))
          (mapcat identity))
    axes))

;; TODO: Rename this cause plane is too specific
(defn- plane-toggle-button [settings-binding plane-group key plane]
  (let [active-plane (get-value settings-binding key)]
    (doto (ToggleButton. (string/upper-case (name plane)))
      (ensure-focus-traversable!)
      (.setToggleGroup plane-group)
      (.setSelected (= plane active-plane))
      (ui/add-style! "plane-toggle"))))

(defn- vec3-toggle-setting [settings-binding key label-text]
  (let [plane-group (ToggleGroup.)
        buttons (mapv (partial plane-toggle-button settings-binding plane-group key) axes)
        label (Label. label-text)]
    (ui/observe (.selectedToggleProperty plane-group)
                (fn [_ ^ToggleButton old-value ^ToggleButton new-value]
                  (if new-value
                    (let [active-plane (-> (.getText new-value)
                                           string/lower-case
                                           keyword)]
                      (set-value! settings-binding key active-plane))
                    (.setSelected old-value true))))
    (concat [label] buttons)))

(defn- color-setting [settings-binding key label-text]
  (let [text-field (TextField.)
        [r g b a] (get-value settings-binding key)
        color (->> (Color. r g b a) (.toString) nnext (drop-last 2) (apply str "#"))
        label (Label. label-text)
        cancel-fn (fn [_] (ui/text! text-field color))
        update-fn (fn [_] (try
                            (if-let [value (some-> (.getText text-field) colors/hex-color->color)]
                              (set-value! settings-binding key value)
                              (cancel-fn nil))
                            (catch Exception _e
                              (cancel-fn nil))))]
    (doto text-field
      (ui/text! color)
      (ui/customize! update-fn cancel-fn)
      (ensure-focus-traversable!))
    [label text-field]))

(declare settings)

(defn- reset-button [localization settings-binding ^PopupControl popup setting-descriptors hidden-settings button-text]
  (let [button (doto (Button. button-text)
                 (.setPrefWidth Double/MAX_VALUE))
        reset-fn
        (fn [^ActionEvent event]
          (let [target ^Node (.getTarget event)
                parent (.getParent target)]
            (reset-all! settings-binding)
            (doto parent
              (ui/children! (ui/node-array (settings localization settings-binding popup setting-descriptors hidden-settings)))
              (.requestFocus))))]
    (doto button
      (ui/on-action! reset-fn)
      (ensure-focus-traversable!))
    button))

(defn settings-visible? [^Parent owner]
  (some? (ui/user-data owner ::popup)))

(defn pref-popup-position
  ^Point2D [^Parent container width]
  (Utils/pointRelativeTo container width 0 HPos/RIGHT VPos/BOTTOM 0.0 10.0 true))

(defn- setting-row [localization settings-binding popup {:keys [type key label min max accelerator]}]
  (println label)
  (let [label-text (when label (localization (localization/message label)))]
    (case type
      :slider      (slider-setting settings-binding popup key label-text min max)
      :toggle      (toggle-setting settings-binding key label-text accelerator)
      :vec3-floats (vec3-floats-setting settings-binding key)
      :vec3-toggle (vec3-toggle-setting settings-binding key label-text)
      :color       (color-setting settings-binding key label-text)
      :space       [(Region.)]
      :separator   [(Separator.)])))

(defn- settings [localization settings-binding popup setting-descriptors include-reset-btn hidden-settings]
  (let [button-text (localization (localization/message "scene-popup.reset-defaults-button"))
        reset-btn (when include-reset-btn
                    (reset-button localization settings-binding popup setting-descriptors hidden-settings button-text))]
    (->> setting-descriptors
         (remove (fn [{:keys [key]}] (contains? hidden-settings key)))
         (reduce (fn [rows descriptor]
                   (let [children (setting-row localization settings-binding popup descriptor)
                         row (doto (HBox. (ui/node-array children))
                               (.setAlignment Pos/CENTER))]
                     (doseq [child children]
                       (HBox/setHgrow child Priority/ALWAYS))
                     (conj rows row)))
                 (if include-reset-btn [reset-btn] [])))))

(defn show-settings!
  ([^Parent owner localization settings-binding width setting-descriptors include-reset-btn]
   (show-settings! owner localization settings-binding width setting-descriptors include-reset-btn nil))
  ([^Parent owner localization settings-binding width setting-descriptors include-reset-btn hidden-settings]
   (if-let [popup ^PopupControl (ui/user-data owner ::popup)]
     (.hide popup)
     (let [region (StackPane.)
           popup (make-popup owner region)
           anchor ^Point2D (pref-popup-position (.getParent owner) width)]
       (.setPrefWidth region width)
       (ui/children! region [(doto (Region.)
                               (ui/add-style! "popup-shadow"))
                             (doto (VBox. (ui/node-array (settings localization settings-binding popup setting-descriptors include-reset-btn hidden-settings)))
                               (.setFocusTraversable true)
                               (ensure-focus-traversable!)
                               (ui/add-style! "popup-settings"))])
       (ui/user-data! owner ::popup popup)
       (doto popup
         (.setAnchorLocation PopupWindow$AnchorLocation/CONTENT_TOP_RIGHT)
         (ui/on-closed! (fn [_] (ui/user-data! owner ::popup nil)))
         (.show owner (.getX anchor) (.getY anchor)))))))
