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

(ns editor.ui.settings-popup
  (:require [cljfx.api :as fx]
            [cljfx.fx.check-box :as fx.check-box]
            [cljfx.fx.region :as fx.region]
            [cljfx.fx.slider :as fx.slider]
            [clojure.string :as string]
            [editor.colors :as colors]
            [editor.fxui :as fxui]
            [editor.handler :as handler]
            [editor.keymap :as keymap]
            [editor.localization :as localization]
            [editor.math :as math]
            [editor.os :as os]
            [editor.prefs :as prefs]
            [editor.ui :as ui]
            [util.defonce :as defonce])
  (:import [antlr.collections List]
           [com.sun.javafx.util Utils]
           [javafx.css Styleable]
           [javafx.event ActionEvent]
           [javafx.geometry HPos Point2D Pos VPos]
           [javafx.scene Node Parent]
           [javafx.scene.control Button CheckBox Control Label PopupControl Separator Skin Slider TextField ToggleButton ToggleGroup]
           [javafx.scene.layout HBox Priority Region StackPane]
           [javafx.scene.paint Color]
           [javafx.stage PopupWindow$AnchorLocation]))

(set! *warn-on-reflection* true)

(defonce ^List axes [:x :y :z])

(defonce/protocol SettingsStore
  (get-value [this key])
  (get-default-value [this key])
  (set-value! [this key v]))

(defonce/record PrefsStore [prefs prefs-prefix setting-descriptors hidden-settings on-change-fn]
  SettingsStore
  (get-value [_ key] (prefs/get prefs (conj prefs-prefix key)))
  (get-default-value [_ key] (prefs/default-value-at prefs (conj prefs-prefix key)))
  (set-value! [_ key v]
    (prefs/set! prefs (conj prefs-prefix key) v)
    (when on-change-fn (on-change-fn key v))))

;; TODO JOE: Inline this
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
      (.setConsumeAutoHidingEvents false)
      (.setAutoHide true)
      (.setAutoFix true)
      (.setHideOnEscape true))))

(defn- ensure-focus-traversable! [^Node node]
  (ui/observe (.focusTraversableProperty node)
    (fn [_ _ _]
      (.setFocusTraversable node true))))

(defn- wrap-in-hbox [children]
  (HBox. (ui/node-array children)))

(defn- make-slider-row [^PopupControl popup settings-store {:keys [key label min max snap-to slider-value->string]}]
  (let [value (get-value settings-store key)
        slider (Slider. min max value)
        label (Label. label)
        snap-fn (if snap-to
                  (fn [^double v]
                    (* (double snap-to) (Math/round (/ v (double snap-to)))))
                  identity)
        slider-value->string (or slider-value->string #(str (math/round-with-precision % 0.01)))
        value-label (doto (Label. (slider-value->string value))
                      (ui/add-style! "slider-value-label"))]
    (doto slider
      (ensure-focus-traversable!)
      (.setBlockIncrement 0.1)
      (.setOnMouseEntered (ui/event-handler e (.setAutoHide popup false)))
      (.setOnMouseExited (ui/event-handler e (.setAutoHide popup true))))

    (ui/observe
      (.valueProperty slider)
      (fn [_observable _old-val new-val]
        (let [new-val (snap-fn new-val)
              str-val (slider-value->string new-val)]
          (ui/text! value-label str-val)
          (set-value! settings-store key new-val))))
    (wrap-in-hbox [label value-label slider])))

(defn- make-toggle-row [^PopupControl popup settings-store {:keys [key label command accelerator] :as descriptor}]
  (let [check-box (CheckBox.)
        label-node (Label. (or label ""))
        acc (Label. (or accelerator ""))]
    (doto check-box
      (ui/value! (get-value settings-store key))
      (ui/remove-style! "check-box")
      (ui/add-style! "slide-switch")
      (ensure-focus-traversable!)
      (ui/on-action! (fn [_]
                       (let [value (ui/value check-box)]
                         (set-value! settings-store key value)))))
    (HBox/setHgrow label-node Priority/ALWAYS)
    (ui/add-style! label-node "slide-switch-label")
    (when (os/is-mac-os?)
      (.setStyle acc "-fx-font-family: 'Lucida Grande';"))
    (ui/add-style! acc "accelerator-label")
    (when command
      (let [listener-id [::toggle-setting key]]
        (handler/add-listener! listener-id command
          (fn []
            (ui/value! check-box (get-value settings-store key))))
        (ui/on-closed! popup
          (fn [_]
            (handler/remove-listener! listener-id command)))))
    (let [hbox (doto (HBox.)
                 (.setAlignment Pos/CENTER_LEFT)
                 (ui/add-style! "toggle-row")
                 (ui/on-click! (fn [_]
                                 (let [value (not (ui/value check-box))]
                                   (ui/value! check-box value)
                                   (set-value! settings-store key value))))
                 (ui/children! [label-node acc check-box]))]
      hbox)))

(defn- make-vec3-group
  [settings-store key axis]
  (let [text-field (TextField.)
        label (Label. (string/upper-case (name axis)))
        size-val (str (get (get-value settings-store key) axis))
        cancel-fn (fn [_] (ui/text! text-field size-val))
        update-fn (fn [_] (try
                            (let [value (Float/parseFloat (.getText text-field))]
                              (if (pos? value)
                                (do
                                  (set-value! settings-store key (assoc (get-value settings-store key) axis value))
                                  (ui/text! text-field (str value)))
                                (cancel-fn nil)))
                            (catch Exception _e
                              (cancel-fn nil))))]
    (doto text-field
      (ui/text! size-val)
      (ui/customize! update-fn cancel-fn)
      (ensure-focus-traversable!))
    [label text-field]))

(defn- make-vec3-floats-row [_popup settings-store {:keys [key]}]
  (wrap-in-hbox (into []
                      (comp (map (partial make-vec3-group settings-store key))
                            (mapcat identity))
                      axes)))

(defn- make-vec3-toggle-button [settings-store plane-group key plane]
  (let [active-plane (get-value settings-store key)]
    (doto (ToggleButton. (string/upper-case (name plane)))
      (ensure-focus-traversable!)
      (.setToggleGroup plane-group)
      (.setSelected (= plane active-plane))
      (ui/add-style! "plane-toggle"))))

(defn- make-vec3-toggle-row [_popup settings-store descriptor]
  (let [{:keys [key label]} descriptor
        plane-group (ToggleGroup.)
        buttons (mapv (partial make-vec3-toggle-button settings-store plane-group key) axes)
        label (Label. label)]
    (ui/observe (.selectedToggleProperty plane-group)
                (fn [_ ^ToggleButton old-value ^ToggleButton new-value]
                  (if new-value
                    (let [active-plane (-> (.getText new-value)
                                           string/lower-case
                                           keyword)]
                      (set-value! settings-store key active-plane))
                    (.setSelected old-value true))))
    (wrap-in-hbox (concat [label] buttons))))

(defn- make-color-row [_popup settings-store {:keys [key label]}]
  (let [text-field (TextField.)
        [r g b a] (get-value settings-store key)
        color (->> (Color. r g b a) (.toString) nnext (drop-last 2) (apply str "#"))
        label (Label. label)
        cancel-fn (fn [_] (ui/text! text-field color))
        update-fn (fn [_] (try
                            (if-let [value (some-> (.getText text-field) colors/hex-color->color)]
                              (set-value! settings-store key value)
                              (cancel-fn nil))
                            (catch Exception _e
                              (cancel-fn nil))))]
    (doto text-field
      (ui/text! color)
      (ui/add-style! "color-setting-row")
      (ui/customize! update-fn cancel-fn)
      (ensure-focus-traversable!))
    (wrap-in-hbox [label text-field])))

(declare build-controls)

(defn- make-reset-button-row [keymap localization settings-store ^PopupControl popup setting-descriptors hidden-settings button-text]
  (let [button (Button. button-text)
        reset-fn
        (fn [^ActionEvent event]
          (doseq [{:keys [key]} setting-descriptors
                  :when (and key (not (contains? hidden-settings key)))]
            ;; TODO JOE: Add the new reset-all! function, we need prefs
            #_(prefs/reset-path! prefs key))
          (let [target ^Node (.getTarget event)
                parent (.getParent (.getParent target)) ;; Button is nested inside an HBox
                [rows _controls] (build-controls keymap localization settings-store popup setting-descriptors hidden-settings)]
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

(defn- make-settings-divider-row [^Control control]
  (doto (wrap-in-hbox [control])
    (ui/add-style! "settings-divider-row")))

(defn- pref-popup-position
  ^Point2D [^Parent container width x-offset]
  (Utils/pointRelativeTo container width 0 HPos/RIGHT VPos/BOTTOM x-offset 10.0 true))

(defn- make-row-from-descriptor [keymap localization popup settings-store descriptor]
  (let [descriptor (cond-> descriptor
                     (:label descriptor)
                     (assoc :label (localization (localization/message (:label descriptor))))
                     (:command descriptor)
                     (assoc :accelerator (keymap/display-text keymap (:command descriptor) "")))]
    (case (:type descriptor)
      :slider      (make-slider-row popup settings-store descriptor)
      :toggle      (make-toggle-row popup settings-store descriptor)
      :vec3-floats (make-vec3-floats-row popup settings-store descriptor)
      :vec3-toggle (make-vec3-toggle-row popup settings-store descriptor)
      :color       (make-color-row popup settings-store descriptor)
      :space       (make-settings-divider-row (Region.))
      :separator   (make-settings-divider-row (Separator.)))))

(defn- build-controls [keymap localization settings-store popup setting-descriptors hidden-settings]
  (let [visible-descriptors (remove (fn [{:keys [key]}] (contains? hidden-settings key)) setting-descriptors)
        [rows controls]
        (reduce (fn [[rows controls] descriptor]
                  (if (= :reset-all (:type descriptor))
                    (let [button-text (localization (localization/message "scene-popup.reset-defaults-button"))
                          reset-btn (make-reset-button-row keymap localization settings-store popup
                                                           visible-descriptors hidden-settings button-text)]
                      [(conj rows reset-btn) controls])
                    (let [key (:key descriptor)
                          row (make-row-from-descriptor keymap localization popup settings-store descriptor)
                          children (.getChildren ^HBox row)]
                      (when-let [style-class (:style-class descriptor)]
                        (ui/add-style! row style-class))
                      (let [first-child (first children)]
                        (HBox/setHgrow first-child Priority/ALWAYS)
                        (.setMaxWidth ^Region first-child Double/MAX_VALUE))
                      (doseq [child (rest children)]
                        (HBox/setHgrow child Priority/NEVER))
                      [(conj rows row) (cond-> controls key (assoc key row))])))
                [[] {}]
                visible-descriptors)]
    [rows controls]))

(defn show!
  ([^Parent owner keymap localization settings-store width x-offset setting-descriptors]
   (show! owner keymap localization settings-store width x-offset setting-descriptors nil))
  ([^Parent owner keymap localization settings-store width x-offset setting-descriptors hidden-settings]
   (show! owner keymap localization settings-store width x-offset setting-descriptors hidden-settings nil))
  ([^Parent owner keymap localization settings-store width x-offset setting-descriptors hidden-settings on-closed]
   (if-let [popup ^PopupControl (ui/user-data owner ::popup)]
     (do (.hide popup) nil)
     (let [region (StackPane.)
           popup (make-popup owner region)
           [rows controls] (build-controls keymap localization settings-store popup setting-descriptors hidden-settings)
           anchor ^Point2D (pref-popup-position (.getParent owner) width x-offset)]
       (.setPrefWidth region width)
       ;; (ui/children! region [(doto (Region.)
       ;;                         (ui/add-style! "popup-shadow"))
       ;;                       (doto (VBox. (ui/node-array rows))
       ;;                         (.setFocusTraversable true)
       ;;                         (ensure-focus-traversable!)
       ;;                         (ui/add-style! "popup-settings"))])
       (ui/user-data! owner ::popup popup)
       (doto popup
         (.setAnchorLocation PopupWindow$AnchorLocation/CONTENT_TOP_RIGHT)
         (ui/on-closed! (fn [e]
                          (when on-closed (on-closed e))
                          (ui/user-data! owner ::popup nil)))
         (.show owner (.getX anchor) (.getY anchor)))
       controls))))

;; CLJFX Port from here on out

(defn- make-toggle-row-fx [{:keys [label accelerator state swap-state selected on-selected-changed]}]
   (println state)
  {:fx/type fxui/horizontal
   :style-class "toggle-row"
   :alignment :center-left
   :on-mouse-clicked (fn [_]
                       (on-selected-changed (not selected)))
   ;; :disable true
   :children [{:fx/type fxui/label
               :style-class "slide-switch-label"
               :text (or label "")
               :h-box/hgrow :always
               :max-width Double/MAX_VALUE}
              {:fx/type fxui/label
               :style-class "accelerator-label"
               :text (or accelerator "")}
              {:fx/type fx.check-box/lifecycle
               :style-class ["slide-switch"]
               :selected (boolean selected)
               :on-selected-changed (or on-selected-changed (fn [_]))}]})

(defn- make-slider-row-fx [{:keys [label min max value slider-value->string on-value-changed]}]
  (let [slider-value->string (or slider-value->string #(str (math/round-with-precision % 0.01)))
        value (or value 1337.0)]
    {:fx/type fxui/horizontal
     :children [{:fx/type fxui/label
                 :text (or label "")
                 :h-box/hgrow :always
                 :max-width Double/MAX_VALUE}
                {:fx/type fxui/label
                 :style-class "slider-value-label"
                 :text (slider-value->string value)}
                {:fx/type fx.slider/lifecycle
                 :min min
                 :max max
                 :value value
                 :block-increment 0.1
                 :on-value-changed (or on-value-changed (fn [_]))}]}))

(defn- make-color-row-fx [{:keys [label value on-value-changed]}]
  {:fx/type fxui/horizontal
   :children [{:fx/type fxui/label
               :text (or label "")
               :h-box/hgrow :always
               :max-width Double/MAX_VALUE}
              {:fx/type fxui/color-picker
               :value (when value
                        (let [[r g b a] value]
                          (Color. (float r) (float g) (float b) (float a))))
               :on-value-changed (fn [^Color c]
                                   (when on-value-changed
                                     (on-value-changed [(.getRed c) (.getGreen c) (.getBlue c) (.getOpacity c)])))
               :ignore-alpha false}]})

(defn- make-vec3-floats-row-fx [{:keys [value on-value-changed]}]
  {:fx/type fxui/horizontal
   :children (into []
                   (mapcat (fn [axis]
                             [{:fx/type fxui/label
                               :text (string/upper-case (name axis))
                               :min-width :use-pref-size}
                              {:fx/type fxui/value-field
                               :text (str (get value axis))
                               :to-value (fn [s]
                                           (try (let [v (Float/parseFloat s)]
                                                  (when (pos? v) v))
                                                (catch Exception _ nil)))
                               :on-value-changed (fn [v]
                                                   (when on-value-changed
                                                     (on-value-changed (assoc value axis v))))}]))
                   axes)})

(defn- make-vec3-toggle-row-fx [{:keys [label value on-value-changed]}]
  {:fx/type fxui/horizontal
   :children (into [{:fx/type fxui/label
                     :text (or label "")
                     :h-box/hgrow :always
                     :max-width Double/MAX_VALUE}]
                   (map (fn [axis]
                          {:fx/type fxui/toggle-button
                           :style-class ["toggle-button" "plane-toggle"]
                           :text (string/upper-case (name axis))
                           :selected (= axis value)
                           :on-action {:event-type :set-value
                                       :key nil ;; filled in by caller
                                       :fx/event axis}}))
                   axes)})

(defn- make-row-fx [keymap localization-state state swap-state descriptor]
  (case (:type descriptor)
    :toggle
    {:fx/type make-toggle-row-fx
     :label (localization-state (localization/message (:label descriptor)))
     :accelerator (keymap/display-text keymap (:command descriptor) "")
     :selected (:value descriptor)
     :state state
     :swap-state swap-state
     :on-selected-changed (fn [v]
                            (:on-value-changed descriptor))}

    :slider
    {:fx/type make-slider-row-fx
     :label (localization-state (localization/message (:label descriptor)))
     :min (:min descriptor)
     :max (:max descriptor)
     :snap-to (:snap-to descriptor)
     :slider-value->string (:slider-value->string descriptor)
     :value (get state (:key descriptor))
     :on-value-changed {:event-type :set-value
                        :key (:key descriptor)}}

    :color
    {:fx/type make-color-row-fx
     :label (localization-state (localization/message (:label descriptor)))
     :value (get state (:key descriptor))
     :on-value-changed {:event-type :set-value
                        :key (:key descriptor)}}

    :vec3-floats
    {:fx/type make-vec3-floats-row-fx
     :value (get state (:key descriptor))
     :on-value-changed {:event-type :set-value
                        :key (:key descriptor)}}

    :vec3-toggle
    {:fx/type make-vec3-toggle-row-fx
     :label (when-let [l (:label descriptor)]
              (localization-state (localization/message (:label descriptor))))
     :value (get state (:key descriptor))
     :on-value-changed {:event-type :set-value
                        :key (:key descriptor)}}

    nil))

(fxui/defc cljfx-popup-view
  {:compose [{:fx/type fx/ext-watcher
              :ref (:localization props)
              :key :localization-state}
             {:fx/type fx/ext-state
              :initial-state (:state props)}]}
  [{:keys [descriptors keymap state swap-state on-change localization-state]}]
  (println swap-state)
  {:fx/type fxui/vertical
   :style-class "popup-settings"
   :focus-traversable true
   :children (keep (fn [descriptor]
                     (make-row-fx keymap localization-state state swap-state descriptor))
                   descriptors)})

(defn- cljfx-popup-desc [descriptors keymap localization state on-change]
  {:fx/type cljfx-popup-view
   :descriptors descriptors
   :keymap keymap
   :localization localization
   :state state
   :on-change on-change})

(defn show!
  ([^Parent owner keymap localization state on-change width x-offset setting-descriptors]
   (show! owner keymap localization state on-change width x-offset setting-descriptors nil))
  ([^Parent owner keymap localization state on-change width x-offset setting-descriptors hidden-settings]
   (show! owner keymap localization state on-change width x-offset setting-descriptors hidden-settings nil))
  ([^Parent owner keymap localization state on-change width x-offset setting-descriptors hidden-settings on-closed]
   (when-not (ui/user-data owner ::popup)
     (let [visible-descriptors (remove #(contains? hidden-settings (:key %)) setting-descriptors)
           content (StackPane.)
           popup (make-popup owner content)
           anchor ^Point2D (pref-popup-position (.getParent owner) width x-offset)
           advance! (fn [state]
                      (fxui/advance-ui-user-data-component!
                        content ::popup
                        {:fx/type fxui/ext-with-stack-pane-props
                         :desc {:fx/type fxui/ext-value :value content}
                         :props {:children [{:fx/type fx.region/lifecycle
                                             :style-class "popup-shadow"}
                                            {:fx/type cljfx-popup-view
                                             :descriptors visible-descriptors
                                             :keymap keymap
                                             :localization localization
                                             :state state
                                             :on-change on-change}]}}))]
       (.setPrefWidth content width)
       (advance! state)
       (ui/user-data! owner ::popup popup)
       (doto popup
         (.setAnchorLocation PopupWindow$AnchorLocation/CONTENT_TOP_RIGHT)
         (ui/on-closed! (fn [e]
                          (fxui/advance-ui-user-data-component! content ::popup nil)
                          (when on-closed (on-closed e))
                          (ui/user-data! owner ::popup nil)))
         (.show owner (.getX anchor) (.getY anchor)))
       advance!))))
