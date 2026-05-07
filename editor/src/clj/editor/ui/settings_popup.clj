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
            [cljfx.fx.toggle-button :as fx.toggle-button]
            [cljfx.fx.toggle-group :as fx.toggle-group]
            [clojure.string :as string]
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
           [javafx.event Event]
           [javafx.geometry HPos Point2D VPos]
           [javafx.scene Cursor Node Parent]
           [javafx.scene.control PopupControl Skin Slider]
           [javafx.scene.input MouseEvent]
           [javafx.scene.layout StackPane]
           [javafx.scene.paint Color]
           [javafx.stage PopupWindow$AnchorLocation]))

(set! *warn-on-reflection* true)

(defonce ^List axes [:x :y :z])

(defn- make-toggle-row [{:keys [key label accelerator command disabled? state swap-state on-selected-changed style-class]}]
  {:fx/type fx/ext-on-instance-lifecycle
   :on-created (fn [_]
                 (when command
                   (handler/add-listener! [::toggle-setting key] command #(swap-state update key not))))
   :on-deleted (fn [_]
                 (when command
                   (handler/remove-listener! [::toggle-setting key] command)))
   :desc
   {:fx/type fxui/horizontal
    :style-class (cond-> ["toggle-row"]
                   style-class (conj style-class))
    :alignment :center-left
    :on-mouse-clicked (fn [_]
                        (swap-state assoc key (not (boolean (key state))))
                        (when on-selected-changed
                          (on-selected-changed (not (boolean (key state))))))
    :disable (boolean (when disabled? (disabled? state)))
    :children [{:fx/type fxui/label
                :style-class "slide-switch-label"
                :text (or label "")
                :h-box/hgrow :always
                :max-width Double/MAX_VALUE}
               {:fx/type fxui/label
                :style-class "accelerator-label"
                :style (if (os/is-mac-os?) "-fx-font-family: 'Lucida Grande';" "")
                :text (or accelerator "")}
               {:fx/type fxui/ext-ensure-focus-traversable
                :desc
                {:fx/type fx.check-box/lifecycle
                 :style-class ["slide-switch"]
                 :selected (key state)
                 :on-selected-changed (fn [v]
                                        (swap-state assoc key v)
                                        (when on-selected-changed
                                          (on-selected-changed v)))}}]}})

(defn- ext-safe-popup-slider
  [{:keys [popup] :as props}]
  {:fx/type fx/ext-on-instance-lifecycle
   :on-created (fn [^Slider slider]
                 (let [pressed? (volatile! false)
                       install! (fn [^Node thumb]
                                  (doto thumb
                                    (.addEventFilter MouseEvent/MOUSE_PRESSED  (ui/event-handler _ (vreset! pressed? true)))
                                    (.addEventFilter MouseEvent/MOUSE_RELEASED (ui/event-handler _ (vreset! pressed? false)))
                                    (.addEventFilter MouseEvent/MOUSE_DRAGGED  (ui/event-handler e (when-not @pressed?
                                                                                                     (.consume ^Event e))))))
                       try-install! (fn []
                                      (when-let [thumb (.lookup slider ".thumb")]
                                        (install! thumb)
                                        true))]
                   (when-not (try-install!)
                     (.addListener (.skinProperty slider)
                                   (reify javafx.beans.value.ChangeListener
                                     (changed [this _ _ _]
                                       (when (try-install!)
                                         (.removeListener (.skinProperty slider) this)))))))
                 (doto slider
                   (.setOnMouseEntered (ui/event-handler _ (.setAutoHide ^PopupControl popup false)))
                   (.setOnMouseExited  (ui/event-handler _ (.setAutoHide ^PopupControl popup true)))))
   :desc (assoc (dissoc props :popup) :fx/type fx.slider/lifecycle)})

(defn- make-slider-row [{:keys [popup key label min max snap-to state swap-state slider-value->string on-value-changed]}]
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
                 (cond-> {:fx/type ext-safe-popup-slider
                          :popup popup
                          :min min
                          :max max
                          :value (key state)
                          :block-increment 0.1
                          :on-value-changed (fn [v]
                                              (on-value-changed v)
                                              (swap-state assoc key v))}
                   snap-to
                   (assoc :snap-to-ticks true
                          :major-tick-unit snap-to
                          :minor-tick-count 0
                          :show-tick-marks true
                          :on-mouse-released (fn [^Event e]
                                               (let [v (snap-fn (.getValue ^Slider (.getSource e)))]
                                                 (on-value-changed v)
                                                 (swap-state assoc key v)))))}]}))

;; The focus-traversable part: `fxui/color-picker` is a compound component (an HBox containing a value-field and a
;; JavaFX ColorPicker), so we can't just wrap it the same way; we need to reach into its internals to find the text
;; input. Rather than modifying fxui to expose this, we use `fx/ext-on-instance-lifecycle` here to look up the inner
;; text field by style class and apply the same focus-traversable workaround.
;;
;; The setAutoHide part: The popup loses focus when we open the "Custom Color..." color picker window, so
;; we apply the same workaround the slider's get by disabling auto-hide and enabling it once it closes.
(defn- ext-color-picker-focus-traversable [{:keys [desc popup]}]
  {:fx/type fx/ext-on-instance-lifecycle
   :on-created (fn [^javafx.scene.Node node]
                 (when-let [tf (.lookup node ".ext-color-picker-field")]
                   (ui/observe (.focusTraversableProperty tf)
                     (fn [_ _ _]
                       (.setFocusTraversable tf true)))
                   (.setFocusTraversable tf true))
                 (when-let [cp (.lookup node ".ext-color-picker-icon")]
                   (when (instance? javafx.scene.control.ColorPicker cp)
                     (let [^javafx.scene.control.ColorPicker cp cp]
                       (.setOnShowing cp (fn [_] (.setAutoHide ^PopupControl popup false)))
                       (.setOnHidden  cp (fn [_] (.setAutoHide ^PopupControl popup true)))))))
   :desc desc})

(defn- make-color-row [{:keys [popup key label state swap-state on-value-changed]}]
  {:fx/type fxui/horizontal
   :children [{:fx/type fxui/label
               :text (or label "")
               :h-box/hgrow :always
               :max-width Double/MAX_VALUE}
              {:fx/type ext-color-picker-focus-traversable
               :popup popup
               :desc
               {:fx/type fxui/color-picker
                :value (let [[r g b a] (key state)]
                         (Color. (float r) (float g) (float b) (float a)))
                :on-value-changed (fn [^Color c]
                                    (let [color [(.getRed c) (.getGreen c) (.getBlue c) (.getOpacity c)]]
                                      (swap-state assoc key color)
                                      (on-value-changed color)))
                :ignore-alpha false}}]})

(defn- make-vec3-floats-row [{:keys [key state swap-state on-value-changed]}]
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

(defn- make-vec3-toggle-row [{:keys [key label state swap-state on-value-changed]}]
  {:fx/type fx/ext-let-refs
   :refs {::toggle-group {:fx/type fx.toggle-group/lifecycle}}
   :desc
   {:fx/type fxui/horizontal
    :children (into [{:fx/type fxui/label
                      :text (or label "")
                      :h-box/hgrow :always
                      :max-width Double/MAX_VALUE}]
                    (map (fn [axis]
                           {:fx/type fxui/ext-ensure-focus-traversable
                            :desc {:fx/type fx.toggle-button/lifecycle
                             :toggle-group {:fx/type fx/ext-get-ref
                                            :ref ::toggle-group}
                             :style-class ["toggle-button" "plane-toggle"]
                             :text (string/upper-case (name axis))
                             :selected (= axis (key state))
                             :on-selected-changed (fn [selected?]
                                                    (when selected?
                                                      (swap-state assoc key axis)
                                                      (when on-value-changed
                                                        (on-value-changed axis))))}}))
                    axes)}})

(defn- make-reset-button [{:keys [text swap-state on-reset]}]
  {:fx/type fxui/horizontal
   :style-class "reset-button"
   :children [{:fx/type fxui/ext-ensure-focus-traversable
               :desc
               {:fx/type fx.button/lifecycle
                :text text
                :max-width Double/MAX_VALUE
                :on-action (fn [^javafx.event.ActionEvent e]
                             (on-reset swap-state)
                             (.requestFocus (.getParent ^Node (.getSource e))))}}]})

(defn- make-row [popup keymap localization-state state swap-state descriptor]
  (let [descriptor-with-state (merge descriptor {:state state :swap-state swap-state :popup popup})
        label #(localization-state (localization/message (:label descriptor)))]
    (case (:type descriptor)
      :toggle      (assoc descriptor-with-state
                          :fx/type make-toggle-row
                          :label (label)
                          :accelerator (keymap/display-text keymap (:command descriptor) "")
                          :on-selected-changed (:on-value-changed descriptor))
      :slider      (assoc descriptor-with-state :fx/type make-slider-row :label (label))
      :color       (assoc descriptor-with-state :fx/type make-color-row  :label (label))
      :vec3-floats (assoc descriptor-with-state :fx/type make-vec3-floats-row)
      :vec3-toggle (assoc descriptor-with-state :fx/type make-vec3-toggle-row :label (label))
      :reset-all   {:fx/type make-reset-button
                    :text (localization-state (localization/message "scene-popup.reset-defaults-button"))
                    :swap-state swap-state
                    :on-reset (:on-reset descriptor)}
      :space     {:fx/type fxui/horizontal :style-class "settings-divider-row"
                  :children [{:fx/type fx.region/lifecycle}]}
      :separator {:fx/type fxui/horizontal :style-class "settings-divider-row"
                  :children [{:fx/type fx.separator/lifecycle :h-box/hgrow :always :max-width Double/MAX_VALUE}]}
      nil)))

(fxui/defc cljfx-popup-view
  {:compose [{:fx/type fx/ext-watcher
              :ref (:localization props)
              :key :localization-state}
             {:fx/type fx/ext-state
              :initial-state (:state props)}]}
  [{:keys [popup descriptors keymap state swap-state localization-state]}]
  {:fx/type fxui/vertical
   :style-class "popup-settings"
   :children (keep (fn [descriptor]
                     (make-row popup keymap localization-state state swap-state descriptor))
                   descriptors)})

(defn settings-visible? [^Parent owner]
  (some? (ui/user-data owner ::popup)))

(defn- pref-popup-position
  ^Point2D [^Parent container width x-offset]
  (Utils/pointRelativeTo container width 0 HPos/RIGHT VPos/BOTTOM x-offset 10.0 true))

(defn- make-popup
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

;; NOTE: This settings UI is shown inside a JavaFX PopupWindow (see `make-popup` above). PopupWindow has
;; its own focus-traversal behavior that tends to set `focusTraversable` to false on child controls,
;; breaking Tab navigation inside the popup. For the simple controls we own (toggles, sliders, value
;; fields), we wrap them in `fxui/ext-ensure-focus-traversable`, which observes the property and
;; forces it back to true.
(defn show!
  "Shows a settings popup anchored to `owner`, or hides it if already visible.

  Returns an `advance!` function that can be called with a new state map to
  update the popup's displayed values, or nil if the popup was hidden.

  Arguments:
    owner                JavaFX Parent node the popup is anchored to
    keymap               keymap used to resolve accelerator labels for :toggle rows
    localization         localization watcher ref used for label translation
    state                initial state map, keyed by the :key of each descriptor.
                         May include a :disabled set of keys for toggle-rows that are
                         grayed out
    width                preferred width of the popup content in pixels
    x-offset             horizontal offset applied when positioning the popup
    setting-descriptors  sequence of row descriptor maps (see below)
    hidden-settings      optional set of :key values whose rows are omitted
    on-closed            optional 0-or-1-argument callback invoked when the popup
                         is closed

  Row descriptor maps have the following shape (all keys optional unless noted):
    :key     required for most types, keyword identifying this setting in state
    :type    required, one of:
               :toggle       a labeled checkbox row
               :slider       a labeled slider with a value label
               :color        a labeled color picker
               :vec3-floats  three labeled float input fields for X/Y/Z
               :vec3-toggle  a labeled group of X/Y/Z toggle buttons
               :reset-all    a button that resets all settings to defaults
               :space        an empty spacer row
               :separator    a horizontal rule divider
    :label                 localization message key for the row label
    :command               for :toggle, keymap command used to display an accelerator
    :min / :max            for :slider, numeric bounds
    :snap-to               for :slider, snaps value to multiples of this number
    :slider-value->string  for :slider, 1-arg fn formatting the displayed value
    :on-value-changed      1-arg callback invoked when this setting's value changes
    :disabled?             1-arg fn of state returning truthy to disable the row
    :on-reset              for :reset-all, 1-arg callback receiving swap-state"
  ([^Parent owner keymap localization state width x-offset setting-descriptors]
   (show! owner keymap localization state width x-offset setting-descriptors nil))
  ([^Parent owner keymap localization state width x-offset setting-descriptors hidden-settings]
   (show! owner keymap localization state width x-offset setting-descriptors hidden-settings nil))
  ([^Parent owner keymap localization state width x-offset setting-descriptors hidden-settings on-closed]
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
                                             :popup popup
                                             :localization localization
                                             :state state}]}}))]
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
       ;; HACK: scene-visibility opens the popup right next to the outline's split pane divider, when you mouse over
       ;; the edge, the cursor gets set to H_RESIZE. If you move your cursor fast enough from the divider to the popup, the H_RESIZE
       ;; persists and only gets reset to DEFAULT when you leave the popup and reenter. The scene apparently still thinks it's
       ;; the DEFAULT cursor, so if you set it to DEFAULT, it's a NOOP, so we set it to NONE first, then DEFAULT, and it works.
       ;; Note that this might just be linux specific, but wouldn't hurt to just do it for everyone.
       (.addEventHandler content MouseEvent/MOUSE_ENTERED
                         (ui/event-handler e
                                           (let [scene (.getScene owner)]
                                             (.setCursor scene Cursor/NONE)
                                             (.setCursor scene Cursor/DEFAULT))))
       ;; Request focus so the first UI element loses focus
       (.requestFocus content)
       advance!))))
