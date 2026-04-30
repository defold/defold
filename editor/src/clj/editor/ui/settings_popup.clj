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
            [cljfx.fx.button :as fx.button]
            [cljfx.fx.check-box :as fx.check-box]
            [cljfx.fx.region :as fx.region]
            [cljfx.fx.separator :as fx.separator]
            [cljfx.fx.slider :as fx.slider]
            [clojure.string :as string]
            [editor.colors :as colors]
            [editor.fxui :as fxui]
            [editor.handler :as handler]
            [editor.keymap :as keymap]
            [editor.localization :as localization]
            [editor.math :as math]
            [editor.os :as os]
            [editor.ui :as ui])
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

(defn- make-toggle-row-fx [{:keys [key label accelerator state swap-state on-selected-changed]}]
  {:fx/type fxui/horizontal
   :style-class "toggle-row"
   :alignment :center-left
   :on-mouse-clicked (fn [_]
                       (swap-state assoc key (not (boolean (key state))))
                       (on-selected-changed (not (boolean (key state)))))
   ;; :disable true
   :children [{:fx/type fxui/label
               :style-class "slide-switch-label"
               :text (or label "")
               :h-box/hgrow :always
               :max-width Double/MAX_VALUE}
              {:fx/type fxui/label
               :style-class "accelerator-label"
               :text (or accelerator "")}
              {:fx/type fxui/ext-ensure-focus-traversable
               :desc
               {:fx/type fx.check-box/lifecycle
                :style-class ["slide-switch"]
                :selected (key state)
                :on-selected-changed (fn [v]
                                       (swap-state assoc key v)
                                       (on-selected-changed v))}}]})

(defn- make-slider-row-fx [{:keys [key label min max snap-to state swap-state slider-value->string on-value-changed]}]
  (let [slider-value->string (or slider-value->string #(str (math/round-with-precision % 0.01)))
        snap-fn (if snap-to
                  (fn [^double v]
                    (* (double snap-to) (Math/round (/ v (double snap-to)))))
                  identity)]
    {:fx/type fxui/horizontal
     :children [{:fx/type fxui/label
                 :text (or label "")
                 :h-box/hgrow :always
                 :max-width Double/MAX_VALUE}
                {:fx/type fxui/label
                 :style-class "slider-value-label"
                 :text (slider-value->string (key state))}
                {:fx/type fxui/ext-ensure-focus-traversable
                 :desc
                 (cond-> {:fx/type fx.slider/lifecycle
                          :min min
                          :max max
                          :value (key state)
                          :block-increment 0.1
                          :on-value-changed (fn [v]
                                              (on-value-changed v)
                                              (swap-state assoc key v))}
                         snap-to
                         (assoc :snap-to-ticks true :major-tick-unit snap-to
                                :on-mouse-released (fn [^javafx.event.Event e]
                                                     (let [slider ^javafx.scene.control.Slider (.getSource e)
                                                           v (snap-fn (.getValue slider))]
                                                       (on-value-changed v)
                                                       (swap-state assoc key v)))))}]}))

;; `fxui/color-picker` is a compound component (an HBox containing a value-field and a JavaFX
;; ColorPicker), so we can't just wrap it the same way -- we need to reach into its internals to
;; find the text input. Rather than modifying fxui to expose this, we use
;; `fx/ext-on-instance-lifecycle` here to look up the inner text field by style class and apply the
;; same focus-traversable workaround.
(defn- ext-color-picker-focus-traversable [{:keys [desc]}]
  {:fx/type fx/ext-on-instance-lifecycle
   :on-created (fn [^javafx.scene.Node node]
                 (when-let [tf (.lookup node ".ext-color-picker-field")]
                   (ui/observe (.focusTraversableProperty tf)
                     (fn [_ _ _]
                       (.setFocusTraversable tf true)))
                   (.setFocusTraversable tf true)))
   :desc desc})

(defn- make-color-row-fx [{:keys [key label state swap-state on-value-changed]}]
  {:fx/type fxui/horizontal
   :children [{:fx/type fxui/label
               :text (or label "")
               :h-box/hgrow :always
               :max-width Double/MAX_VALUE}
              {:fx/type ext-color-picker-focus-traversable
               :desc
               {:fx/type fxui/color-picker
                :value (when (key state)
                         (let [[r g b a] (key state)]
                           (Color. (float r) (float g) (float b) (float a))))
                :on-value-changed (fn [^Color c]
                                    (let [color [(.getRed c) (.getGreen c) (.getBlue c) (.getOpacity c)]]
                                      (swap-state assoc key color)
                                      (on-value-changed color)))
                :ignore-alpha false}}]})

(defn- make-vec3-floats-row-fx [{:keys [key state swap-state on-value-changed]}]
  {:fx/type fxui/horizontal
   :children (into []
                   (mapcat (fn [axis]
                             [{:fx/type fxui/label
                               :text (string/upper-case (name axis))
                               :min-width :use-pref-size}
                              {:fx/type fxui/ext-ensure-focus-traversable
                               :desc
                               {:fx/type fxui/value-field
                                :value (get (key state) axis)
                                :to-value (fn [s]
                                            (try (let [v (Float/parseFloat s)]
                                                   (when (pos? v) v))
                                                 (catch Exception _ nil)))
                                :on-value-changed (fn [v]
                                                    (let [new-vec3 (assoc (key state) axis v)]
                                                      (swap-state assoc key new-vec3)
                                                      (when on-value-changed
                                                        (on-value-changed new-vec3))))}}]))
                   axes)})

(defn- make-vec3-toggle-row-fx [{:keys [key label state swap-state on-value-changed]}]
  {:fx/type fxui/horizontal
   :children (into [{:fx/type fxui/label
                     :text (or label "")
                     :h-box/hgrow :always
                     :max-width Double/MAX_VALUE}]
                   (map (fn [axis]
                          {:fx/type fxui/ext-ensure-focus-traversable
                           :desc
                           {:fx/type fxui/toggle-button
                            :style-class ["toggle-button" "plane-toggle"]
                            :text (string/upper-case (name axis))
                            :selected (= axis (key state))
                            :on-selected-changed (fn [selected?]
                                                   (when selected?
                                                     (swap-state assoc key axis)
                                                     (when on-value-changed
                                                       (on-value-changed axis))))}}))
                   axes)})

(defn- make-reset-row-fx [{:keys [text swap-state on-reset]}]
  {:fx/type fxui/horizontal
   :style-class "reset-button"
   :children [{:fx/type fxui/ext-ensure-focus-traversable
               :desc
               {:fx/type fx.button/lifecycle
                :text text
                :max-width Double/MAX_VALUE
                :on-action (fn [_] (on-reset swap-state))}}]})

(defn- make-row-fx [keymap localization-state state swap-state descriptor]
  (case (:type descriptor)
    :toggle
    {:fx/type make-toggle-row-fx
     :label (localization-state (localization/message (:label descriptor)))
     :accelerator (keymap/display-text keymap (:command descriptor) "")
     :key (:key descriptor)
     :state state
     :swap-state swap-state
     :on-selected-changed #((:on-value-changed descriptor) %)}

    :slider
    {:fx/type make-slider-row-fx
     :label (localization-state (localization/message (:label descriptor)))
     :min (:min descriptor)
     :max (:max descriptor)
     :snap-to (:snap-to descriptor)
     :slider-value->string (:slider-value->string descriptor)
     :key (:key descriptor)
     :state state
     :swap-state swap-state
     :on-value-changed #((:on-value-changed descriptor) %)}

    :color
    {:fx/type make-color-row-fx
     :label (localization-state (localization/message (:label descriptor)))
     :key (:key descriptor)
     :state state
     :swap-state swap-state
     :on-value-changed #((:on-value-changed descriptor) %)}

    :vec3-floats
    {:fx/type make-vec3-floats-row-fx
     :key (:key descriptor)
     :state state
     :swap-state swap-state
     :on-value-changed #((:on-value-changed descriptor) %)}

    :vec3-toggle
    {:fx/type make-vec3-toggle-row-fx
     :label (when-let [l (:label descriptor)]
              (localization-state (localization/message (:label descriptor))))
     :key (:key descriptor)
     :state state
     :swap-state swap-state
     :on-value-changed #((:on-value-changed descriptor) %)}

    :reset-all
    {:fx/type make-reset-row-fx
     :text (localization-state (localization/message "scene-popup.reset-defaults-button"))
     :swap-state swap-state
     :on-reset (:on-reset descriptor)}

    :space
    {:fx/type fxui/horizontal
     :style-class "settings-divider-row"
     :children [{:fx/type fx.region/lifecycle}]}

    :separator
    {:fx/type fxui/horizontal
     :style-class "settings-divider-row"
     :children [{:fx/type fx.separator/lifecycle
                 :h-box/hgrow :always
                 :max-width Double/MAX_VALUE}]}

    nil))

(fxui/defc cljfx-popup-view
  {:compose [{:fx/type fx/ext-watcher
              :ref (:localization props)
              :key :localization-state}
             {:fx/type fx/ext-state
              :initial-state (:state props)}]}
  [{:keys [descriptors keymap state swap-state on-change localization-state]}]
  {:fx/type fxui/vertical
   :style-class "popup-settings"
   :children (keep (fn [descriptor]
                     (make-row-fx keymap localization-state state swap-state descriptor))
                   descriptors)})

(defn settings-visible? [^Parent owner]
  (some? (ui/user-data owner ::popup)))

(defn- pref-popup-position
  ^Point2D [^Parent container width x-offset]
  (Utils/pointRelativeTo container width 0 HPos/RIGHT VPos/BOTTOM x-offset 10.0 true))

;; NOTE: This settings UI is shown inside a JavaFX PopupWindow (see `make-popup` above). PopupWindow has
;; its own focus-traversal behavior that tends to set `focusTraversable` to false on child controls,
;; breaking Tab navigation inside the popup. For the simple controls we own (toggles, sliders, value
;; fields), we wrap them in `fxui/ext-ensure-focus-traversable`, which observes the property and
;; forces it back to true.
(defn show!
  ([^Parent owner keymap localization state on-change width x-offset setting-descriptors]
   (show! owner keymap localization state on-change width x-offset setting-descriptors nil))
  ([^Parent owner keymap localization state on-change width x-offset setting-descriptors hidden-settings]
   (show! owner keymap localization state on-change width x-offset setting-descriptors hidden-settings nil))
  ([^Parent owner keymap localization state on-change width x-offset setting-descriptors hidden-settings on-closed]
   (if-let [popup ^PopupControl (ui/user-data owner ::popup)]
     (do (.hide popup) nil)
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
       ;; Request focus so the first UI element loses focus
       (.requestFocus content)
       advance!))))
