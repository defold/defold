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
            [editor.handler :as handler]
            [editor.keymap :as keymap]
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
    (when on-change-fn (on-change-fn key v)))
  (reset-all! [_]
    (doseq [{:keys [key type]} setting-descriptors
            :when (and key (not (contains? hidden-settings key)))
            :let [prefs-path (conj prefs-prefix key)]]
      (prefs/set! prefs prefs-path (prefs/default-value-at prefs prefs-path)))
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

(defn- wrap-in-hbox [children]
  (HBox. (ui/node-array children)))

(defn- slider-setting [^PopupControl popup settings-binding {:keys [key label min max on-change-fn]}]
  (let [value (get-value settings-binding key)
        slider (Slider. min max value)
        label (Label. label)]
    (doto slider
      (ensure-focus-traversable!)
      (.setBlockIncrement 0.1)
      (.setOnMouseEntered (ui/event-handler e (.setAutoHide popup false)))
      (.setOnMouseExited (ui/event-handler e (.setAutoHide popup true))))

    (ui/observe
      (.valueProperty slider)
      (fn [_observable _old-val new-val]
        (let [val (math/round-with-precision new-val 0.01)]
          (when on-change-fn
            (on-change-fn val))
          (set-value! settings-binding key val))))
    (wrap-in-hbox [label slider])))

(defn- toggle-setting [^PopupControl popup settings-binding {:keys [key label command accelerator] :as descriptor}]
  (let [check-box (CheckBox.)
        label-node (Label. (or label ""))
        acc (Label. (or accelerator ""))]
    (doto check-box
      (ui/value! (get-value settings-binding key))
      (ui/remove-style! "check-box")
      (ui/add-style! "slide-switch")
      (ensure-focus-traversable!)
      (ui/on-action! (fn [_]
                       (let [value (ui/value check-box)]
                         (set-value! settings-binding key value)))))
    (HBox/setHgrow label-node Priority/ALWAYS)
    (ui/add-style! label-node "slide-switch-label")
    (when (os/is-mac-os?)
      (.setStyle acc "-fx-font-family: 'Lucida Grande';"))
    (ui/add-style! acc "accelerator-label")
    (when command
      (let [listener-id [::toggle-setting key]]
        (handler/add-listener! listener-id command
          (fn []
            (ui/value! check-box (get-value settings-binding key))))
        (ui/on-closed! popup
          (fn [_]
            (handler/remove-listener! listener-id command)))))
    (let [hbox (doto (HBox.)
                 (.setAlignment Pos/CENTER_LEFT)
                 (ui/add-style! "toggle-row")
                 (ui/on-click! (fn [_]
                                 (let [value (not (ui/value check-box))]
                                   (ui/value! check-box value)
                                   (set-value! settings-binding key value))))
                 (ui/children! [label-node acc check-box]))]
      hbox)))

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

(defn- vec3-floats-setting [_popup settings-binding {:keys [key]}]
  (wrap-in-hbox (into []
                      (comp (map (partial vec3-group settings-binding key))
                            (mapcat identity))
                      axes)))

;; TODO: Rename this cause plane is too specific
(defn- plane-toggle-button [settings-binding plane-group key plane]
  (let [active-plane (get-value settings-binding key)]
    (doto (ToggleButton. (string/upper-case (name plane)))
      (ensure-focus-traversable!)
      (.setToggleGroup plane-group)
      (.setSelected (= plane active-plane))
      (ui/add-style! "plane-toggle"))))

(defn- vec3-toggle-setting [popup settings-binding descriptor]
  (let [{:keys [key label]} descriptor
        plane-group (ToggleGroup.)
        buttons (mapv (partial plane-toggle-button settings-binding plane-group key) axes)
        label (Label. label)]
    (ui/observe (.selectedToggleProperty plane-group)
                (fn [_ ^ToggleButton old-value ^ToggleButton new-value]
                  (if new-value
                    (let [active-plane (-> (.getText new-value)
                                           string/lower-case
                                           keyword)]
                      (set-value! settings-binding key active-plane))
                    (.setSelected old-value true))))
    (wrap-in-hbox (concat [label] buttons))))

(defn- color-setting [popup settings-binding {:keys [key label]}]
  (let [text-field (TextField.)
        [r g b a] (get-value settings-binding key)
        color (->> (Color. r g b a) (.toString) nnext (drop-last 2) (apply str "#"))
        label (Label. label)
        cancel-fn (fn [_] (ui/text! text-field color))
        update-fn (fn [_] (try
                            (if-let [value (some-> (.getText text-field) colors/hex-color->color)]
                              (set-value! settings-binding key value)
                              (cancel-fn nil))
                            (catch Exception _e
                              (cancel-fn nil))))]
    (doto text-field
      (ui/text! color)
      (ui/add-style! "color-setting-row")
      (ui/customize! update-fn cancel-fn)
      (ensure-focus-traversable!))
    (wrap-in-hbox [label text-field])))

(declare settings)

(defn- reset-button [keymap localization settings-binding ^PopupControl popup setting-descriptors hidden-settings button-text]
  (let [button (Button. button-text)
        reset-fn
        (fn [^ActionEvent event]
          (reset-all! settings-binding)
          (let [target ^Node (.getTarget event)
                parent (.getParent (.getParent target)) ;; Button is nested inside an HBox
                [rows _controls] (settings keymap localization settings-binding popup setting-descriptors hidden-settings)]
            (doto parent
              (ui/children! (ui/node-array rows))
              (.requestFocus))))]
    (doto button
      (ui/on-action! reset-fn)
      (HBox/setHgrow Priority/ALWAYS)
      (ensure-focus-traversable!))
    (doto (wrap-in-hbox [button])
      (ui/add-style! "reset-button"))))

(defn settings-visible? [^Parent owner]
  (some? (ui/user-data owner ::popup)))

(defn- pref-popup-position
  ^Point2D [^Parent container width x-offset]
  (Utils/pointRelativeTo container width 0 HPos/RIGHT VPos/BOTTOM x-offset 10.0 true))

(def ^:private setting-constructors
  {:slider      slider-setting
   :toggle      toggle-setting
   :vec3-floats vec3-floats-setting
   :vec3-toggle vec3-toggle-setting
   :color       color-setting})

(defn- setting-row [keymap localization popup settings-binding descriptor]
  (let [descriptor (cond-> descriptor
                     (:label descriptor)
                     (assoc :label (localization (localization/message (:label descriptor))))
                     (:command descriptor)
                     (assoc :accelerator (keymap/display-text keymap (:command descriptor) "")))]
    (if-let [ctor (setting-constructors (:type descriptor))]
      (ctor popup settings-binding descriptor)
      (case (:type descriptor)
        :space     (doto (wrap-in-hbox [(Region.)]) (ui/add-style! "settings-divider-row"))
        :separator (doto (wrap-in-hbox [(Separator.)]) (ui/add-style! "settings-divider-row"))))))

(defn- settings [keymap localization settings-binding popup setting-descriptors hidden-settings]
  (let [visible-descriptors (remove (fn [{:keys [key]}] (contains? hidden-settings key)) setting-descriptors)
        [rows controls]
        (reduce (fn [[rows controls] descriptor]
                  (if (= :reset-all (:type descriptor))
                    (let [button-text (localization (localization/message "scene-popup.reset-defaults-button"))
                          reset-btn (reset-button keymap localization settings-binding popup visible-descriptors hidden-settings button-text)]
                      [(conj rows reset-btn) controls])
                    (let [key (:key descriptor)
                          row (setting-row keymap localization popup settings-binding descriptor)
                          children (.getChildren ^HBox row)]
                      (when-let [style-class (:style-class descriptor)]
                        (ui/add-style! row style-class))
                      (let [first-child (first children)]
                        (HBox/setHgrow first-child Priority/ALWAYS)
                        (.setMaxWidth ^Region first-child Double/MAX_VALUE))
                      (doseq [child (rest children)]
                        (HBox/setHgrow child Priority/NEVER))
                      [(conj rows row) (cond-> controls key (assoc key children))])))
                [[] {}]
                visible-descriptors)]
    [rows controls]))

(defn show-settings!
  ([^Parent owner keymap localization settings-binding width x-offset setting-descriptors]
   (show-settings! owner keymap localization settings-binding width x-offset setting-descriptors nil))
  ([^Parent owner keymap localization settings-binding width x-offset setting-descriptors hidden-settings]
   (show-settings! owner keymap localization settings-binding width x-offset setting-descriptors hidden-settings nil))
  ([^Parent owner keymap localization settings-binding width x-offset setting-descriptors hidden-settings on-closed]
   (if-let [popup ^PopupControl (ui/user-data owner ::popup)]
     (do (.hide popup) nil)
     (let [region (StackPane.)
           popup (make-popup owner region)
           [rows controls] (settings keymap localization settings-binding popup setting-descriptors hidden-settings)
           anchor ^Point2D (pref-popup-position (.getParent owner) width x-offset)]
       (.setPrefWidth region width)
       (ui/children! region [(doto (Region.)
                               (ui/add-style! "popup-shadow"))
                             (doto (VBox. (ui/node-array rows))
                               (.setFocusTraversable true)
                               (ensure-focus-traversable!)
                               (ui/add-style! "popup-settings"))])
       (ui/user-data! owner ::popup popup)
       (doto popup
         (.setAnchorLocation PopupWindow$AnchorLocation/CONTENT_TOP_RIGHT)
         (ui/on-closed! (fn [e]
                          (when on-closed (on-closed e))
                          (ui/user-data! owner ::popup nil)))
         (.show owner (.getX anchor) (.getY anchor)))
       controls))))
