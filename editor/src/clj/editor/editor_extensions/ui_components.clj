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

(ns editor.editor-extensions.ui-components
  (:require [cljfx.api :as fx]
            [cljfx.component :as fx.component]
            [cljfx.fx.button :as fx.button]
            [cljfx.fx.column-constraints :as fx.column-constraints]
            [cljfx.fx.image-view :as fx.image-view]
            [cljfx.fx.label :as fx.label]
            [cljfx.fx.region :as fx.region]
            [cljfx.fx.row-constraints :as fx.row-constraints]
            [cljfx.fx.stage :as fx.stage]
            [cljfx.fx.v-box :as fx.v-box]
            [cljfx.lifecycle :as fx.lifecycle]
            [cljfx.mutator :as fx.mutator]
            [cljfx.prop :as fx.prop]
            [clojure.spec.alpha :as s]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.dialogs :as dialogs]
            [editor.editor-extensions.coerce :as coerce]
            [editor.editor-extensions.runtime :as rt]
            [editor.editor-extensions.ui-docs :as ui-docs]
            [editor.error-reporting :as error-reporting]
            [editor.field-expression :as field-expression]
            [editor.fs :as fs]
            [editor.future :as future]
            [editor.fxui :as fxui]
            [editor.fxui.combo-box :as fxui.combo-box]
            [editor.icons :as icons]
            [editor.localization :as localization]
            [editor.resource :as resource]
            [editor.resource-dialog :as resource-dialog]
            [editor.ui :as ui]
            [internal.util :as iutil]
            [util.coll :as coll :refer [pair]]
            [util.fn :as fn])
  (:import [com.defold.editor.luart DefoldLuaFn]
           [java.nio.file Path]
           [java.util Collection List]
           [javafx.beans.property ReadOnlyProperty]
           [javafx.beans.value ChangeListener]
           [javafx.event Event]
           [javafx.scene Node]
           [javafx.scene.control CheckBox TextField]
           [javafx.scene.input KeyCode KeyEvent]
           [javafx.stage DirectoryChooser FileChooser FileChooser$ExtensionFilter]
           [org.luaj.vm2 LuaError LuaValue]))

(set! *warn-on-reflection* true)

;; See also:
;; - Components doc: https://docs.google.com/document/d/1e6kmVLspQEoe17Ys1nmbbf54cqUn2fNPZowIe7JBwl4/edit
;; - Reactive UI doc: https://docs.google.com/document/d/1cgqFPLUc3YQih2tsEA067Pz9Q0nWlKoVflqLEVX8CPY/edit

(s/def ::create ifn?)
(s/def ::advance ifn?)
(s/def ::return ifn?)
(s/def ::hook
  (s/keys :req-un [:editor.editor-extensions.ui-docs.component/name ::create ::advance ::return]))

(defn- ^{:arglists '([name & {:keys [create advance return]}])} make-hook
  "Construct a hook definition map

  Args:
    name    string name of the function, should start with \"use_\" and be
            snake_cased

  Kv-args:
    :create          required, hook function that is executed when the hook is
                     created, it receives following arguments:
                       component-atom        atom of the component, can be
                                             modified using `update-hook-state!`
                       component-state       component atom's state at the point
                                             when the hook is initialized, i.e.
                                             it has a valid :current-hook-index
                                             key that needs to be passed to the
                                             `update-hook-state!` function
                       evaluation-context    evaluation context for the
                                             invocation
                       ...rest of lua args passed by the editor script
                     the function returns its state, any value
    :advance         required, hook function that is invoked on subsequent UI
                     updates, it receives following arguments:
                       component-state       component atom's state as in
                                             :create
                       old-hook-state        previously returned hook state
                       evaluation-context    evaluation context for the
                                             invocation
                       ...rest of lua args passed by the editor script
                     the function returns its new state, any value
    :return          a function from hook state to lua value that is then
                     returned to the calling editor script"
  [name & {:as m}]
  {:post [(s/assert ::hook %)]}
  (assoc m :name name))

(defn- make-component-lua-fn [name props fn]
  (let [string-representation (str "editor.ui." name "(...)")
        {req true opt false} (iutil/group-into {} {} :required (coll/pair-fn :name :coerce) props)
        props-coercer (coerce/hash-map :req req :opt opt)]
    (rt/lua-fn component-factory [{:keys [rt]} lua-props]
      (let [props (rt/->clj rt props-coercer lua-props)]
        (-> (fn (assoc props :rt rt))
            (vary-meta assoc :type :component :props props)
            (rt/wrap-userdata string-representation))))))

;; region layout components

;; General approach to layout can be described as follows:
;; - all components have `alignment` property that configures how the
;;   component content is aligned within its assigned bounds (default: top-left)
;; - leaf components take as much space as available to them
;; - layout components have different approach to giving space to their children:
;;   - `horizontal` gives as much space as possible on a vertical axis, but not
;;     on a horizontal axis. Child components may use `grow` prop to grow on
;;     the horizontal axis.
;;   - Same applies to `vertical`, but with the axes switched
;;   - `grid` does not expand its children, unless `grow` row or column
;;     setting is used.

(defn- apply-alignment [props alignment]
  {:pre [(some? alignment)]}
  (case alignment
    (:top-left :top-right :center :bottom-left :bottom-right) (assoc props :alignment alignment)
    :top (assoc props :alignment :top-center)
    :left (assoc props :alignment :center-left)
    :right (assoc props :alignment :center-right)
    :bottom (assoc props :alignment :bottom-center)))

(defn- scroll-view [{:keys [content]}]
  {:fx/type fxui/scroll
   :content content})

(defn- apply-constraints [props props-key lifecycle grow-key constraints]
  (assoc props props-key (mapv (fn [maybe-constraint]
                                 (let [ret {:fx/type lifecycle}]
                                   (if maybe-constraint
                                     (let [{:keys [grow]} maybe-constraint]
                                       (cond-> ret grow (assoc grow-key :always)))
                                     ret)))
                               constraints)))

(defn- grid-view [{:keys [children rows columns alignment padding spacing]
                   :or {spacing :medium}}]
  (cond->
    {:fx/type fxui/grid
     :spacing spacing}
    children (assoc :children
                    (into []
                          (coll/mapcat-indexed
                            (fn [row row-children]
                              (eduction
                                (keep-indexed
                                  (fn [column child]
                                    (when child
                                      (let [{:keys [row_span column_span]
                                             :or {row_span 1 column_span 1}} (:props (meta child))]
                                        (assoc child :grid-pane/row row
                                                     :grid-pane/column column
                                                     :grid-pane/row-span row_span
                                                     :grid-pane/column-span column_span)))))
                                row-children)))
                          children))
    rows (apply-constraints :row-constraints fx.row-constraints/lifecycle :vgrow rows)
    columns (apply-constraints :column-constraints fx.column-constraints/lifecycle :hgrow columns)
    padding (assoc :padding padding)
    alignment (assoc :alignment alignment)))

(defn- separator-view [{:keys [alignment orientation]
                        :or {alignment :center
                             orientation :horizontal}}]
  (apply-alignment
    {:fx/type fx.v-box/lifecycle
     :children [{:fx/type fx.region/lifecycle
                 :v-box/vgrow :always
                 :style-class ["ext-separator"]
                 :pseudo-classes #{orientation}}]}
    alignment))

(defn- make-list-view-fn [fx-type grow-key]
  (fn list-view [{:keys [children padding spacing alignment]
                  :or {spacing :medium}}]
    (cond-> {:fx/type fx-type
             :children (into []
                             (keep-indexed
                               (fn [i desc]
                                 (when desc
                                   (-> desc
                                       (assoc :fx/key i)
                                       (cond-> (:grow (:props (meta desc)))
                                               (assoc grow-key :always))))))
                             children)
             :spacing spacing}
            padding (assoc :padding padding)
            alignment (assoc :alignment alignment))))

(def ^:private horizontal-view (make-list-view-fn fxui/horizontal :h-box/hgrow))

(def ^:private vertical-view (make-list-view-fn fxui/vertical :v-box/vgrow))

;; endregion

;; region data presentation components

(def ^:private ^{:arglists '([props color])} apply-label-color
  (let [color->style-class (fn/make-case-fn (coll/pair-map-by identity #(str "ext-label-color-" (name %)) (:color ui-docs/enums)))]
    (fn apply-label-color [props color]
      (fxui/add-style-classes props (color->style-class color)))))

(fxui/defc label-view
  {:compose [{:fx/type fx/ext-get-env :env [:localization-state]}]}
  [{:keys [alignment text text_alignment color tooltip localization-state]
    :or {alignment :top-left
         color :text}}]
  (cond-> {:fx/type fxui/label :alignment alignment}
          text (assoc :text (localization-state text))
          color (assoc :color color)
          text_alignment (assoc :text-alignment text_alignment)
          tooltip (assoc :tooltip (localization-state tooltip))))

(fxui/defc paragraph-view
  {:compose [{:fx/type fx/ext-get-env :env [:localization-state]}]}
  [{:keys [alignment text text_alignment color word_wrap localization-state]
    :or {alignment :top-left
         word_wrap true
         color :text}}]
  (cond-> {:fx/type fxui/paragraph
           :wrap-text word_wrap
           :alignment alignment
           :color color}
          text (assoc :text (localization-state text))
          text_alignment (assoc :text-alignment text_alignment)))

(def ^:private heading-style->label-style-class
  (fn/make-case-fn (coll/pair-map-by identity #(str "ext-heading-style-" (name %)) (:heading-style ui-docs/enums))))

(fxui/defc heading-view
  {:compose [{:fx/type fx/ext-get-env :env [:localization-state]}]}
  [{:keys [alignment text text_alignment color word_wrap style localization-state]
    :or {alignment :top-left
         word_wrap true
         style :h3
         color :text}}]
  (-> {:fx/type fx.label/lifecycle
       :style-class ["label" (heading-style->label-style-class style)]
       :max-width Double/MAX_VALUE
       :max-height Double/MAX_VALUE
       :wrap-text word_wrap}
      (apply-alignment alignment)
      (apply-label-color color)
      (cond-> text (assoc :text (localization-state text))
              text_alignment (assoc :text-alignment text_alignment))))

(defn- wrap-in-alignment-container
  "Wrapper for components that don't specify alignment

  Will keep the child as small as possible by default"
  ([desc maybe-alignment]
   (wrap-in-alignment-container desc maybe-alignment false))
  ([desc maybe-alignment fill-horizontal]
   (cond-> {:fx/type fx.v-box/lifecycle
            :fill-width fill-horizontal
            :children [desc]}
           maybe-alignment
           (apply-alignment maybe-alignment))))

(defn- icon-view [{:keys [icon alignment]}]
  (let [fit-size (case icon
                   :open-resource 22.0
                   16.0)]
    (wrap-in-alignment-container
      {:fx/type fx.v-box/lifecycle
       :style-class "ext-icon-container"
       :alignment :center
       :children [{:fx/type fx.image-view/lifecycle
                   :image (icons/get-image
                            (case icon
                              :open-resource "icons/32/Icons_S_14_linkarrow.png"
                              :plus "icons/32/Icons_M_07_plus.png"
                              :minus "icons/32/Icons_M_11_minus.png"
                              :clear "icons/32/Icons_S_02_Reset.png"))
                   :fit-width fit-size
                   :fit-height fit-size}]}
      alignment)))

;; endregion

;; region input components

(defn- report-error-to-script [rt label ex]
  (.println (rt/stderr rt)
            (str label " failed: " (or (ex-message ex) (.getSimpleName (class ex))))))

(defn- report-async-error-to-script [future rt label]
  (future/catch future #(report-error-to-script rt label %)))

(def ^:private ^:dynamic *owner-window* nil)

(defn- current-owner-window []
  (or *owner-window* (ui/main-stage)))

(defn- invoke-event-handler
  ([owner-window rt label lua-fn]
   (binding [*owner-window* owner-window]
     (report-async-error-to-script (rt/invoke-suspending-1 rt lua-fn) rt label)))
  ([owner-window rt label lua-fn value]
   (binding [*owner-window* owner-window]
     (report-async-error-to-script (rt/invoke-suspending-1 rt lua-fn (rt/->lua value)) rt label))))

(defn- make-event-handler-0 [event->owner-window rt label lua-fn]
  (fn event-handler-0 [e]
    (invoke-event-handler (event->owner-window e) rt label lua-fn)))

(defn- make-event-handler-1 [event->owner-window event->value rt label lua-fn]
  (fn event-handler-1 [e]
    (invoke-event-handler (event->owner-window e) rt label lua-fn (event->value e))))

;; region button

(defn- event-source->owner-window [^Event e]
  (.getWindow (.getScene ^Node (.getSource e))))

(fxui/defc button-view
  {:compose [{:fx/type fx/ext-get-env :env [:localization-state]}]}
  [{:keys [rt
           ;; text
           text
           text_alignment
           ;; icon
           icon
           ;; rest
           on_pressed
           enabled
           alignment
           ;; env
           localization-state]
    :or {enabled true}}]
  (let [text (localization-state text)
        has-text (not (string/blank? text))
        has-icon (some? icon)
        style-class (cond
                      (and has-text has-icon) ["ext-button" "ext-button-text-and-icon"]
                      has-text ["ext-button" "ext-button-text"]
                      has-icon ["ext-button" "ext-button-icon"]
                      :else ["ext-button"])]
    (-> {:fx/type fx.button/lifecycle
         :disable (not enabled)
         :alignment :center
         :min-width :use-pref-size
         :min-height :use-pref-size
         :focus-traversable false
         :style-class style-class}
        (cond->
          has-text (assoc :text text)
          text_alignment (assoc :text-alignment text_alignment)
          has-icon (assoc :graphic (icon-view {:icon icon}))
          (and (not has-text) (not has-icon)) (assoc :graphic {:fx/type fx.region/lifecycle
                                                               :style-class "ext-icon-container"})
          on_pressed (assoc :on-action (make-event-handler-0 event-source->owner-window rt "on_pressed" on_pressed)))
        (wrap-in-alignment-container alignment))))

;; endregion

;; region check_box

(def ^:private property-change-listener-with-source-owner-window-lifecycle
  (fx.lifecycle/wrap-coerce
    fx.lifecycle/event-handler
    (fn [f]
      (reify ChangeListener
        (changed [_ observable-value _ v]
          (f {:window (.getWindow (.getScene ^Node (.getBean ^ReadOnlyProperty observable-value)))
              :value v}))))))

(def ^:private ext-with-extra-check-box-props
  (fx/make-ext-with-props
    {:on-selected-changed (fx.prop/make
                            (fx.mutator/property-change-listener #(.selectedProperty ^CheckBox %))
                            property-change-listener-with-source-owner-window-lifecycle)}))

(defn- set-tooltip-and-issue [props tooltip issue localization-state]
  (let [message (:message issue)]
    (cond-> props
            (or message tooltip)
            (fxui/apply-tooltip
              {:severity (:severity issue :info)
               :message (localization-state
                          (if (and message tooltip)
                            (localization/join "\n\n" [message tooltip])
                            (or message tooltip)))}))))

(fxui/defc check-box-view
  {:compose [{:fx/type fx/ext-get-env :env [:localization-state]}]}
  [{:keys [rt text text_alignment alignment issue tooltip enabled value on_value_changed localization-state]
    :or {enabled true
         value false}}]
  (wrap-in-alignment-container
    {:fx/type ext-with-extra-check-box-props
     :desc (-> {:fx/type fxui/check-box
                :disable (not enabled)
                :selected value}
               (set-tooltip-and-issue tooltip issue localization-state)
               (cond->
                 issue (assoc :color (:severity issue))
                 text (assoc :text (localization-state text))
                 text_alignment (assoc :text-alignment text_alignment)))
     :props (cond-> {} on_value_changed (assoc :on-selected-changed (make-event-handler-1 :window :value rt "on_value_changed" on_value_changed)))}
    alignment))

;; endregion

;; region select_box

(def ^:private message-or-to-string-coercer
  (coerce/one-of ui-docs/message-pattern-coercer coerce/to-string))

(defn- stringify-lua-value
  ([rt maybe-lua-value localization-state]
   (if (nil? maybe-lua-value)
     ""
     (localization-state (rt/->clj rt message-or-to-string-coercer maybe-lua-value))))
  ([rt to_string maybe-lua-value localization-state]
   (if (nil? maybe-lua-value)
     ""
     (localization-state (rt/->clj rt message-or-to-string-coercer (rt/invoke-immediate-1 rt to_string maybe-lua-value))))))

(defn- create-select-box-to-string [rt to_string localization-state]
  ;; Note: if a combo box has no value provided, the default is JVM null
  (if to_string
    #(stringify-lua-value rt to_string % localization-state)
    #(stringify-lua-value rt % localization-state)))

(def ^:private ext-with-extra-props
  (fx/make-ext-with-props {}))

(def ^:private prop-instance-ref
  (fx/make-prop
    (fx.mutator/setter (fn [v a] (reset! a v)))
    fx.lifecycle/scalar))

(defn- ref->window-fn [ref]
  (fn [_event]
    (.getWindow (.getScene ^Node (deref ref)))))

(fxui/defc select-box-view
  {:compose [{:fx/type fx/ext-get-env :env [:localization-state]}
             {:fx/type fxui/ext-memo :fn atom :args [nil] :key :ref}]}
  [{:keys [rt alignment tooltip issue enabled value options on_value_changed to_string localization-state ref]
    :or {options []
         enabled true}}]
  (wrap-in-alignment-container
    {:fx/type ext-with-extra-props
     :desc {:fx/type fxui/ext-memo
            :fn create-select-box-to-string
            :args [rt to_string localization-state]
            :key :to-string
            :desc (-> {:fx/type fxui.combo-box/view
                       :value value
                       :items options
                       :disable (not enabled)
                       :filter-prompt-text (localization-state (localization/message "ui.combo-box.filter-prompt"))
                       :no-items-text (localization-state (localization/message "ui.combo-box.no-items"))
                       :not-found-text (localization-state (localization/message "ui.combo-box.not-found"))}
                      (cond->
                        on_value_changed (assoc :on-value-changed (make-event-handler-1 (ref->window-fn ref) identity rt "on_value_changed" on_value_changed))
                        issue (assoc :color (:severity issue)))
                      (set-tooltip-and-issue tooltip issue localization-state))}
     :props (cond-> {prop-instance-ref ref})}
    alignment
    true))

;; endregion

;; region text field

(def ^:private ext-with-extra-text-field-props
  (fx/make-ext-with-props
    {:on-text-changed (fx.prop/make
                        (fx.mutator/property-change-listener #(.textProperty ^TextField %))
                        property-change-listener-with-source-owner-window-lifecycle)}))

(fxui/defc text-field-view
  {:compose [{:fx/type fx/ext-get-env :env [:localization-state]}]}
  [{:keys [rt text on_text_changed tooltip issue enabled alignment localization-state]
    :or {text "" enabled true}}]
  (wrap-in-alignment-container
    {:fx/type ext-with-extra-text-field-props
     :desc (-> {:fx/type fxui/text-field
                :disable (not enabled)
                :text text}
               (set-tooltip-and-issue tooltip issue localization-state)
               (cond-> issue (assoc :color (:severity issue))))
     :props (cond-> {} on_text_changed (assoc :on-text-changed (make-event-handler-1 :window :value rt "on_text_changed" on_text_changed)))}
    alignment true))

;; endregion

;; region value field

(def ^:private value-field-value-coercer
  ;; if field value is null, coerce it to JVM null (for special handling in
  ;; to-string part of the converter)
  (coerce/one-of coerce/null coerce/untouched))

(defn- notify-value-field-change [owner-window rt on_value_changed old-maybe-lua-value new-lua-value]
  (let [notify (or (nil? old-maybe-lua-value)
                   (not (rt/eq? rt old-maybe-lua-value new-lua-value)))]
    (when (and notify on_value_changed)
      (invoke-event-handler owner-window rt "on_value_changed" on_value_changed new-lua-value))
    notify))

(defn- meaningful-value-field-edit? [edit maybe-lua-value text]
  (and (some? edit)
       (or (nil? maybe-lua-value)
           (not= edit text))))

(defn- on-value-field-key-pressed [rt maybe-lua-value text on_value_changed to_value edit swap-state ^KeyEvent e]
  (condp = (.getCode e)
    KeyCode/ENTER
    (do
      (when (meaningful-value-field-edit? edit maybe-lua-value text)
        (let [maybe-new-lua-value (try
                                    (rt/->clj rt value-field-value-coercer (rt/invoke-immediate-1 rt to_value (rt/->lua edit)))
                                    (catch LuaError _))]
          ;; nil result or error from `to_value` means couldn't convert => keep editing
          (if (nil? maybe-new-lua-value)
            (do
              (fxui/play-invalid-value-animation! (.getSource e))
              (.consume e))
            ;; new value!
            (do (swap-state #(-> % (assoc :value maybe-new-lua-value) (dissoc :edit)))
                (notify-value-field-change (event-source->owner-window e) rt on_value_changed maybe-lua-value maybe-new-lua-value)))))
      (.consume e))

    KeyCode/ESCAPE
    (when edit
      (swap-state dissoc :edit)
      (.consume e))

    nil))

(defn- on-value-field-focus-changed [rt maybe-lua-value text on_value_changed to_value edit swap-state {focused :value owner-window :window}]
  (when (and (not focused)
             (meaningful-value-field-edit? edit maybe-lua-value text))
    (let [maybe-new-lua-value (try
                                (rt/->clj rt value-field-value-coercer (rt/invoke-immediate-1 rt to_value (rt/->lua edit)))
                                (catch LuaError _))]
      ;; nil result or error from `to_value` means couldn't convert => discard edit
      (if (nil? maybe-new-lua-value)
        (swap-state dissoc :edit)
        ;; new value!
        (do (swap-state #(-> % (assoc :value maybe-new-lua-value) (dissoc :edit)))
            (notify-value-field-change owner-window rt on_value_changed maybe-lua-value maybe-new-lua-value))))))

(def ^:private ext-with-extra-value-field-props
  (fx/make-ext-with-props
    {:text (fx.prop/make
             (fx.mutator/setter
               (fn [^TextField text-field text]
                 (when-not (= text (.getText text-field))
                   (.setText text-field text)
                   (.selectAll text-field))))
             fx.lifecycle/scalar)
     :on-focused-changed (fx.prop/make
                           (fx.mutator/property-change-listener #(.focusedProperty ^TextField %))
                           property-change-listener-with-source-owner-window-lifecycle)}))

(defn- value-field-view-impl-2
  [{:keys [text                                             ;; text of the current value
           state swap-state                                 ;; local state, may contain :value and :edit
           rt
           value                                            ;; current (maybe) lua value, either value from local state or from editor script
           on_value_changed
           to_value
           #_to_string                                      ;; the key is here, but unused, see value-field-view-impl-1
           tooltip
           issue
           enabled
           alignment
           localization-state]
    :or {enabled true}}]
  (let [{:keys [edit]} state]
    (wrap-in-alignment-container
      {:fx/type ext-with-extra-value-field-props
       :props {:text (or edit text)
               :on-focused-changed #(on-value-field-focus-changed rt value text on_value_changed to_value edit swap-state %)}
       :desc (-> {:fx/type fxui/text-field
                  :disable (not enabled)
                  :on-text-changed #(swap-state assoc :edit %)
                  :on-key-pressed #(on-value-field-key-pressed rt value text on_value_changed to_value edit swap-state %)}
                 (set-tooltip-and-issue tooltip issue localization-state)
                 (cond-> issue (assoc :color (:severity issue))))}
      alignment
      true)))

(fxui/defc value-field-view-impl-1
  {:compose [{:fx/type fx/ext-get-env :env [:localization-state]}]}
  [{:keys [state rt value to_string localization-state]
    :as props}]
  ;; overrides :value to point to the current value of the control, which might
  ;; differ from the one assigned from outside.
  ;; adds value's :text to props
  (let [current-value (:value state value)]
    {:fx/type fxui/ext-memo
     :fn stringify-lua-value
     :args [rt to_string current-value localization-state]
     :key :text
     :desc (assoc props :fx/type value-field-view-impl-2 :value current-value :localization-state localization-state)}))

(defn- value-field-view [props]
  ;; adds :state and :swap-state to props
  {:fx/type fx/ext-state
   ;; we put value as a "key" of the initial state so that when value changes, the
   ;; local state will reset. Other keys used by this local state are:
   ;;   :value    last entered value, never lua nil
   ;;   :edit     currently edited text, a string
   :initial-state {:key (:value props)}
   :desc (assoc props :fx/type value-field-view-impl-1)})

;; endregion

;; regions value_field's friends

(def ^:private lua-to-string-fn
  (DefoldLuaFn.
    (fn to-lua-string [^LuaValue arg]
      ;; translated from package-private org.luaj.vm2.lib.BaseLib$tostring
      (let [h (.metatag arg LuaValue/TOSTRING)]
        (if-not (.isnil h)
          (.call h arg)
          (let [v (.tostring arg)]
            (if-not (.isnil v)
              v
              (LuaValue/valueOf (.tojstring arg)))))))))

(defn- string-field-view [props]
  (assoc props :fx/type value-field-view
               :to_string lua-to-string-fn
               :to_value lua-to-string-fn))

(def ^:private lua-to-integer-fn
  (DefoldLuaFn.
    (fn to-lua-integer [^LuaValue arg]
      (rt/->lua (field-expression/to-long (.tojstring arg))))))

(defn- integer-field-view [props]
  (assoc props :fx/type value-field-view
               :to_string lua-to-string-fn
               :to_value lua-to-integer-fn))

(def ^:private lua-to-number-fn
  (DefoldLuaFn.
    (fn to-lua-number [^LuaValue arg]
      (rt/->lua (field-expression/to-double (.tojstring arg))))))

(defn- number-field-view [props]
  (assoc props :fx/type value-field-view
               :to_string lua-to-string-fn
               :to_value lua-to-number-fn))

;; endregion

;; endregion

;; region dialog components

(fxui/defc dialog-button-view
  {:compose [{:fx/type fx/ext-get-env :env [:localization-state]}]}
  [{:keys [enabled text result cancel default localization-state]
    :or {enabled true cancel false default false}}]
  (let [button {:fx/type fxui/legacy-button
                :disable (not enabled)
                :variant (if default :primary :secondary)
                :cancel-button cancel
                :default-button default
                :text (localization-state text)
                :on-action {:result result}}]
    (if default
      {:fx/type fxui/ext-focused-by-default :desc button}
      button)))

(fxui/defc dialog-view
  {:compose [{:fx/type fx/ext-get-env :env [:localization-state]}]}
  [{:keys [title header content buttons localization-state]}]
  (let [title (localization-state title)
        header (or header {:fx/type fxui/legacy-label :text title :variant :header})
        buttons (into []
                      (keep-indexed
                        (fn [i desc]
                          (when desc
                            (assoc desc :fx/key i))))
                      buttons)
        cancel-result (:result (some (fn [desc]
                                       (let [{:keys [props]} (meta desc)]
                                         (when (:cancel props) props)))
                                     buttons))
        footer {:fx/type dialogs/dialog-buttons
                :children (if (coll/empty? buttons)
                            [{:fx/type dialog-button-view :text (localization/message "dialog.button.close") :cancel true :default true}]
                            buttons)}]
    (cond-> {:fx/type dialogs/dialog-stage
             :title title
             :on-close-request {:result cancel-result}
             :header header
             :footer footer}
            content
            (assoc :content content))))

;; endregion

;; region functions

;; The value gets bound to a volatile with evaluation context. When the context
;; becomes invalid (fills the cache after use), the volatile is set to nil to
;; signal that this context can't be used anymore. The default value is a
;; reduced instance to support `deref`, but not `vreset!`
(def ^:private ^:dynamic *lifecycle-evaluation-context-ideref* (reduced nil))

(defn- lifecycle-evaluation-context []
  {:post [(some? %)]}
  @*lifecycle-evaluation-context-ideref*)

(defmacro ^:private with-nestable-evaluation-context [& body]
  `(if @*lifecycle-evaluation-context-ideref*
     (do ~@body)
     (g/with-auto-evaluation-context ec#
       (let [v# (volatile! ec#)]
         (binding [*lifecycle-evaluation-context-ideref* v#]
           (let [ret# (do ~@body)]
             (vreset! v# nil)
             ret#))))))

(def ext-with-evaluation-context
  "Lifecycle that shares an evaluation context in cljfx tree

  Expected keys:
    :desc    child description that will be able to access a shared evaluation
             context using [[lifecycle-evaluation-context]] fn"
  (reify fx.lifecycle/Lifecycle
    (create [_ {:keys [desc]} opts]
      (with-nestable-evaluation-context
        (fx.lifecycle/create fx.lifecycle/dynamic desc opts)))
    (advance [_ component {:keys [desc]} opts]
      (with-nestable-evaluation-context
        (fx.lifecycle/advance fx.lifecycle/dynamic component desc opts)))
    (delete [_ component opts]
      (with-nestable-evaluation-context
        (fx.lifecycle/delete fx.lifecycle/dynamic component opts)))))

(fxui/defc root-view
  {:doc "Root lifecycle that must be used by all extension UIs

         Expected keys:
           :desc            extension cljfx description
           :localization    localization instance

         Provides:
           - evaluation context: shared along the whole cljfx tree during
             lifecycle updates, can be accessed using
             [[lifecycle-evaluation-context]] fn
           - localization state: available in cljfx env using [[fx/ext-get-env]]
             with :localization-state key"
   :compose [{:fx/type fx/ext-watcher :ref (:localization props) :key :localization-state}
             {:fx/type fx/ext-set-env :env {:localization-state (:localization-state props)}}
             {:fx/type ext-with-evaluation-context}]}
  [{:keys [desc]}]
  desc)

;; region show_dialog

(def ^:private ext-with-stage-props
  (fx/make-ext-with-props fx.stage/props))

(defn- show-dialog-wrapper-view [{:keys [desc] :as props}]
  {:fx/type ext-with-stage-props
   :props {:showing (fxui/dialog-showing? props)}
   :desc desc})

(defn- make-lua-show-dialog-fn [localization]
  (rt/suspendable-lua-fn show-dialog [{:keys [rt]} lua-dialog-component]
    (let [desc {:fx/type show-dialog-wrapper-view
                :desc {:fx/type root-view
                       :localization localization
                       :desc (rt/->clj rt ui-docs/component-coercer lua-dialog-component)}}
          f (future/make)]
      (fx/run-later
        (future/complete!
          f
          (fxui/show-dialog-and-await-result!
            :event-handler #(assoc %1 ::fxui/result (:result %2))
            :error-handler #(future/fail! f %)
            :description desc)))
      f)))

;; endregion

;; region show_external_file_dialog

(def ^:private show-external-file-dialog-opts-coercer
  (coerce/hash-map
    :opt {:title ui-docs/string-or-message-pattern-coercer
          :path coerce/string
          :filters ui-docs/external-file-dialog-filters-coercer}))

(def ^:private default-select-external-file-dialog-title-message
  (localization/message "dialog.file.title.select"))

(defn- make-show-external-file-dialog-lua-fn [^Path project-path localization]
  (rt/suspendable-lua-fn select-external-file
    ([ctx]
     (select-external-file ctx nil))
    ([{:keys [rt]} maybe-lua-opts]
     (let [opts (when maybe-lua-opts
                  (rt/->clj rt show-external-file-dialog-opts-coercer maybe-lua-opts))
           {:keys [title ^String path filters]
            :or {title default-select-external-file-dialog-title-message path "."}} opts
           initial-path (.normalize (.resolve project-path path))
           [^Path initial-directory-path initial-file-name]
           (if (fs/path-exists? initial-path)
             (if (fs/path-is-directory? initial-path)
               [initial-path nil]
               [(.getParent initial-path) (str (.getFileName initial-path))])
             (let [parent-path (.getParent initial-path)]
               (if (fs/path-is-directory? parent-path)
                 [parent-path (str (.getFileName initial-path))]
                 [project-path nil])))
           dialog (doto (FileChooser.)
                    (.setTitle (localization title))
                    (.setInitialDirectory (.toFile initial-directory-path)))]
       (when filters
         (.addAll (.getExtensionFilters dialog)
                  ^Collection
                  (mapv (fn [{:keys [description extensions]}]
                          (FileChooser$ExtensionFilter.
                            ^String (localization description)
                            ^List extensions))
                        filters)))
       (when initial-file-name
         (.setInitialFileName dialog initial-file-name))
       (let [f (future/make)
             owner-window (current-owner-window)]
         (fx/run-later
           (try
             (future/complete! f (some-> (.showOpenDialog dialog owner-window) str))
             (catch Throwable e
               (future/fail! f e))))
         f)))))

;; endregion

;; region show_external_directory_dialog

(def ^:private show-external-directory-dialog-opts-coercer
  (coerce/hash-map
    :opt {:title ui-docs/string-or-message-pattern-coercer
          :path coerce/string}))

(def ^:private default-select-external-directory-dialog-title-message
  (localization/message "dialog.directory.title.select"))

(defn- make-show-external-directory-dialog-lua-fn [^Path project-path localization]
  (rt/suspendable-lua-fn show-external-directory-dialog
    ([ctx]
     (show-external-directory-dialog ctx nil))
    ([{:keys [rt]} maybe-lua-opts]
     (let [opts (when maybe-lua-opts
                  (rt/->clj rt show-external-directory-dialog-opts-coercer maybe-lua-opts))
           {:keys [title ^String path]
            :or {title default-select-external-directory-dialog-title-message path "."}} opts
           resolved-path (.normalize (.resolve project-path path))
           ^Path initial-path (cond
                                (not (fs/path-exists? resolved-path)) project-path
                                (fs/path-is-directory? resolved-path) resolved-path
                                :else (.getParent resolved-path))
           dialog (doto (DirectoryChooser.)
                    (.setTitle (localization title))
                    (.setInitialDirectory (.toFile initial-path)))
           f (future/make)]
       (fx/run-later
         (try
           (future/complete! f (some-> (.showDialog dialog (current-owner-window)) str))
           (catch Throwable e (future/fail! f e))))
       f))))

;; endregion

;; region show_resource_dialog

;; ext, title, selection

(def ^:private show-resource-dialog-opts-coercer
  (coerce/hash-map :opt {:extensions (coerce/vector-of coerce/string :min-count 1)
                         :selection (coerce/enum :single :multiple)
                         :title ui-docs/string-or-message-pattern-coercer}))

(def ^:private default-select-resource-title-message
  (localization/message "dialog.select-resource.title"))

(defn- make-show-resource-dialog-lua-fn [workspace project]
  (rt/suspendable-lua-fn show-resource-dialog
    ([ctx] (show-resource-dialog ctx nil))
    ([{:keys [rt]} maybe-lua-opts]
     (let [opts (when maybe-lua-opts
                  (rt/->clj rt show-resource-dialog-opts-coercer maybe-lua-opts))
           {:keys [selection extensions title]
            :or {selection :single
                 title default-select-resource-title-message}} opts
           f (future/make)
           owner (current-owner-window)]
       (fx/run-later
         (try
           (future/complete!
             f
             (when-let [results (resource-dialog/make workspace project {:selection selection
                                                                         :ext extensions
                                                                         :title title
                                                                         :owner owner})]
               (case selection
                 :single (some-> (first results) resource/proj-path)
                 :multiple (coll/not-empty (mapv resource/proj-path results)))))
           (catch Throwable e (future/fail! f e))))
       f))))

;; endregion

;; region component

;; This dynamic makes the hooks work. It's bound when component lua function is
;; invoked, and hooks use the component atom for setting up their behavior.
;; Component atom contains a map with the following keys:
;;   :hooks                 a vector of maps with the following keys:
;;                            :hook    the hook map, created with make-hook
;;                            :state   the hook state
;;   :current-hook-index    integer identifying the current hook in :hooks. when
;;                          component function is called, it's zero, and then it
;;                          gets incremented after every hook use. After the
;;                          component function returns, it's used to verify that
;;                          there was an expected number of hooks calls.
;;   :mode                  either :create, :advance or :delete. When the
;;                          component instance is created in cljfx tree, mode is
;;                          set to :create, then it's set to :advance on
;;                          updates, and finally it's set to :delete on cljfx
;;                          component delete. Mode affects how hooks work, and
;;                          whether the re-renders are scheduled
;;   :rt                    editor extensions runtime
;;   :lua-fn                the component lua function
;;   :lua-props             the component lua props table
;;   :opts                  cljfx opts
;;   :render-counter        render counter. atom has a watcher that schedules
;;                          the view refresh when counter is set from 0 to 1.
;;                          Hooks might increment the counter, which will
;;                          schedule a refresh. Since JavaFX is mutable, cljfx
;;                          updates cannot be retried. The watcher performs a
;;                          refresh on the UI thread with a particular counter
;;                          in mind, and then updates the atom, either resetting
;;                          the counter back to zero if the counter didn't
;;                          change, or scheduling another refresh if it changed
;;                          during the update.
(def ^:private ^:dynamic *current-component-atom* nil)

;; used in `swap!`
(defn- complete-component-rendering [component-state desc child render-counter]
  (-> component-state
      (assoc :desc desc :child child)
      (cond-> (= render-counter (:render-counter component-state))
              (assoc :render-counter 0))))

;; used in `swap!`
(defn- complete-component-advance [component-state lua-props desc child opts render-counter]
  (-> component-state
      (assoc :lua-props lua-props :opts opts)
      (complete-component-rendering desc child render-counter)))

;; used in `swap!`
(defn- complete-hook-triggered-advance [component-state desc child render-counter]
  (complete-component-rendering component-state desc child render-counter))

;; not used in `swap!` since fx.lifecycle/advance cannot be retried. returns new child
(defn- advance-component-child [old-child desc opts]
  (let [old-instance (fx.component/instance old-child)
        new-child (fx.lifecycle/advance fx.lifecycle/root old-child desc opts)
        new-instance (fx.component/instance new-child)]
    ;; It's not allowed to change the final JavaFX node instance on a component atom
    ;; because the change will not be picked up by cljfx. In cljfx, components are
    ;; considered immutable descriptions of mutable objects. Re-assigning the node
    ;; happens if node instance of old component is not the same as node instance of
    ;; the new component. But since function component stays the same — an atom —
    ;; both old and new instances are going to be the same. In practice, this is not
    ;; an issue, since function components typically return the same root instance,
    ;; and the function is only needed for local state that affects behavior.
    (when-not (identical? old-instance new-instance)
      (throw (LuaError. "component function returned a different UI component instance, which is not allowed")))
    new-child))

(defn- invoke-with-hooks [component-atom rt lua-fn lua-props]
  (binding [*current-component-atom* component-atom]
    (let [ret (rt/invoke-immediate-1 rt lua-fn lua-props (lifecycle-evaluation-context))
          {:keys [hooks current-hook-index]} @component-atom]
      (swap! component-atom assoc :current-hook-index 0)
      (when (< current-hook-index (count hooks))
        ;; the check if there were more hooks than necessary is in the main hook
        ;; dispatcher: see make-hook-lua-fn
        (throw (LuaError. "component function used fewer hooks than expected")))
      ret)))

(defn- perform-hook-triggered-render [component-atom]
  (try
    (let [{:keys [mode render-counter lua-fn lua-props rt child opts]} @component-atom]
      (when (and (not= :delete mode) (pos? render-counter))
        (try
          (let [new-state (with-nestable-evaluation-context
                            (let [lua-desc (invoke-with-hooks component-atom rt lua-fn lua-props)
                                  desc (rt/->clj rt ui-docs/component-coercer lua-desc)
                                  new-child (advance-component-child child desc opts)]
                              (swap! component-atom complete-hook-triggered-advance desc new-child render-counter)))]
            (when (and (not= :delete (:mode new-state))
                       (pos? (:render-counter new-state)))
              (fx/run-later (perform-hook-triggered-render component-atom))))
          (catch LuaError e (report-error-to-script rt (rt/->clj rt coerce/to-string lua-fn) e)))))
    (catch Throwable e (error-reporting/report-exception! e))))

(defn- component-atom-watcher [_ component-atom old-component-state new-component-state]
  (when (and (not= :delete (:mode new-component-state))
             (zero? (:render-counter old-component-state))
             (pos? (:render-counter new-component-state)))
    (fx/run-later (perform-hook-triggered-render component-atom))))

(def ^:private component-atom-meta
  {`fx.component/instance #(fx.component/instance (:child @%))})

(def ^:private lua-component-lifecycle
  (reify fx.lifecycle/Lifecycle
    (create [this {:keys [rt lua-fn lua-props]} opts]
      (let [component-atom (doto (atom {:hooks []
                                        :current-hook-index 0
                                        :mode :create       ;; or :advance, :delete
                                        :rt rt
                                        :lua-fn lua-fn
                                        :lua-props lua-props
                                        :opts opts
                                        :render-counter 0}
                                       :meta component-atom-meta)
                             (add-watch this component-atom-watcher))
            lua-desc (invoke-with-hooks component-atom rt lua-fn lua-props)
            desc (rt/->clj rt ui-docs/component-coercer lua-desc)]
        (doto component-atom
          (swap! assoc
                 :mode :advance
                 :desc desc
                 :child (fx.lifecycle/create fx.lifecycle/dynamic desc opts)))))
    (advance [this component-atom {:keys [rt lua-fn lua-props] :as this-desc} opts]
      (let [component-state @component-atom]
        (if (and (= (:rt component-state) rt)
                 (= (:lua-fn component-state) lua-fn))
          (do
            (try
              (let [desc (if (rt/tables-shallow-eq? rt (:lua-props component-state) lua-props)
                           (:desc component-state)
                           (rt/->clj rt ui-docs/component-coercer (invoke-with-hooks component-atom rt lua-fn lua-props)))
                    new-child (advance-component-child (:child component-state) desc opts)]
                (swap! component-atom complete-component-advance lua-props desc new-child opts (:render-counter component-state)))
              (catch LuaError e (report-error-to-script rt (rt/->clj rt coerce/to-string lua-fn) e))
              (catch Throwable e (error-reporting/report-exception! e)))
            component-atom)
          (do (fx.lifecycle/delete this component-atom opts)
              (fx.lifecycle/create this this-desc opts)))))
    (delete [this component-atom opts]
      (remove-watch component-atom this)
      (let [{:keys [child]} (swap! component-atom assoc :mode :delete)]
        (fx.lifecycle/delete fx.lifecycle/dynamic child opts)))))

(def ^:private function-component-lua-fn
  (rt/lua-fn function-component [{:keys [rt]} lua-fn]
    (DefoldLuaFn.
      (fn create-function-component [lua-props]
        (let [read-only-props (rt/->clj rt ui-docs/read-only-props-coercer lua-props)]
          (-> {:fx/type lua-component-lifecycle
               :rt rt
               :lua-fn lua-fn
               :lua-props lua-props}
              (with-meta {:type :component :props read-only-props})
              (rt/wrap-userdata "editor.ui.component(...)")))))))

;; endregion

;; region hooks

;; region use_state

(defn- update-hook-state!
  "For use by the hooks to update their own state, returns new hook state"
  [component-atom hook-index request-render f & args]
  (let [new-state
        (swap!
          component-atom
          (fn [current-state]
            (cond-> (apply update-in current-state [:hooks hook-index :state] f args)
                    request-render
                    (update :render-counter inc))))]
    (-> new-state :hooks (get hook-index) :state)))

(defn- vectors-with-eq-lua-values? [rt as bs]
  (let [n (count as)]
    (and (= n (count bs))
         (loop [i 0]
           (cond
             (= i n) true
             (rt/eq? rt (as i) (bs i)) (recur (inc i))
             :else false)))))

(def ^:private use-state-hook
  (let [lua-nil (rt/->lua nil)]
    (letfn [(lua-args->type+dependencies [rt lua-args]
              (let [first-lua-arg (or (first lua-args) lua-nil)]
                (if (rt/coerces-to? rt coerce/function first-lua-arg)
                  (pair :function (vec lua-args))
                  (pair :value first-lua-arg))))
            (type+dependencies->lua-value [rt type+dependencies evaluation-context]
              (case (key type+dependencies)
                :value (val type+dependencies)
                :function (let [lua-fn+lua-args (val type+dependencies)]
                            (apply rt/invoke-immediate-1 rt (lua-fn+lua-args 0) (conj (subvec lua-fn+lua-args 1) evaluation-context)))))
            (eq-type+dependencies? [rt a b]
              (let [t (key a)]
                (and (= t (key b))
                     (case t
                       :value (rt/eq? rt (val a) (val b))
                       :function (vectors-with-eq-lua-values? rt (val a) (val b))))))
            (make-setter [component-atom hook-index]
              (rt/lua-fn set-or-update-state [{:keys [rt evaluation-context]} & lua-args]
                (let [type+dependencies (lua-args->type+dependencies rt lua-args)]
                  (:current
                    (case (key type+dependencies)
                      :value
                      (update-hook-state! component-atom hook-index true assoc :current (val type+dependencies))

                      :function
                      (let [lua-fn+lua-args (val type+dependencies)
                            lua-fn (lua-fn+lua-args 0)
                            lua-args (subvec lua-fn+lua-args 1)]
                        (update-hook-state! component-atom hook-index true update :current
                                            (fn [old-lua-value]
                                              (apply rt/invoke-immediate-1 rt lua-fn
                                                     (conj (into [old-lua-value] lua-args) evaluation-context))))))))))]
      (make-hook
        (:name ui-docs/use-state-doc)
        :create (fn create-use-hook-state [component-atom {:keys [rt current-hook-index]} evaluation-context & lua-args]
                  (let [type+dependencies (lua-args->type+dependencies rt lua-args)]
                    {:dependencies type+dependencies
                     :current (type+dependencies->lua-value rt type+dependencies evaluation-context)
                     :setter (make-setter component-atom current-hook-index)}))
        :advance (fn advance-use-hook-state [{:keys [rt]} {:keys [dependencies] :as hook-state} evaluation-context & lua-args]
                   (let [new-type+dependencies (lua-args->type+dependencies rt lua-args)]
                     (if (eq-type+dependencies? rt dependencies new-type+dependencies)
                       hook-state
                       (assoc hook-state :dependencies new-type+dependencies
                                         :current (type+dependencies->lua-value rt new-type+dependencies evaluation-context)))))
        :return (fn return-use-hook-values [hook-state]
                  (rt/->varargs (:current hook-state) (:setter hook-state)))))))

;; endregion

;; region use_memo

(def ^:private use-memo-hook
  (make-hook
    (:name ui-docs/use-memo-doc)
    :create (fn create-use-memo-state [_ {:keys [rt]} evaluation-context & lua-args]
              (let [deps (vec lua-args)]
                {:dependencies deps
                 :value (apply rt/invoke-immediate rt (conj deps evaluation-context))}))
    :advance (fn advance-use-memo-state [{:keys [rt]} hook-state evaluation-context & lua-args]
               (let [new-dependencies (vec lua-args)]
                 (if (vectors-with-eq-lua-values? rt new-dependencies (:dependencies hook-state))
                   hook-state
                   {:dependencies new-dependencies
                    :value (apply rt/invoke-immediate rt (conj new-dependencies evaluation-context))})))
    :return :value))

;; endregion

(defn- make-hook-lua-fn [hook]
  (DefoldLuaFn.
    (fn hook-fn [& lua-args]
      (let [component-atom (or *current-component-atom*
                               (throw (LuaError. (format "Cannot use '%s' hook outside of component functions" (:name hook)))))
            evaluation-context (lifecycle-evaluation-context)
            {:keys [current-hook-index mode] :as current-state} @component-atom
            new-state (case mode
                        :create
                        (let [hook-state (apply (:create hook) component-atom current-state evaluation-context lua-args)]
                          (swap! component-atom (fn [component-state]
                                                  (-> component-state
                                                      (update :hooks conj {:hook hook :state hook-state})
                                                      (update :current-hook-index inc)))))

                        :advance
                        (if-let [{hook-state :state prev-hook :hook} (get (:hooks current-state) current-hook-index)]
                          (if (identical? prev-hook hook)
                            (let [new-hook-state (apply (:advance hook) current-state hook-state evaluation-context lua-args)]
                              (swap! component-atom (fn [component-state]
                                                      (-> component-state
                                                          (assoc-in [:hooks current-hook-index :state] new-hook-state)
                                                          (update :current-hook-index inc)))))
                            (throw (LuaError. (format "Expected '%s' hook, got '%s'" (:name prev-hook) (:name hook)))))
                          (throw (LuaError. (format "Didn't expect to use '%s' hook since it wasn't used before" (:name hook)))))
                        :delete
                        (throw (LuaError. (format "Cannot use '%s' hook after component is deleted" (:name hook)))))]
        ((:return hook) (-> new-state :hooks (get current-hook-index) :state))))))

(def ^:private use-state-hook-lua-fn
  (make-hook-lua-fn use-state-hook))

(def ^:private use-memo-hook-lua-fn
  (make-hook-lua-fn use-memo-hook))

;; endregion

;; endregion

(defn env [workspace project project-path localization]
  (into {}
        cat
        [(eduction
           (map
             (fn [[k vs]]
               [(ui-docs/->screaming-snake-case k)
                (into {}
                      (map (coll/pair-fn ui-docs/->screaming-snake-case coerce/enum-lua-value-cache))
                      vs)]))
           ui-docs/enums)
         (eduction
           (map (fn [[{:keys [name props]} view-fn]]
                  (pair name (make-component-lua-fn name props view-fn))))
           [[ui-docs/horizontal-component horizontal-view]
            [ui-docs/vertical-component vertical-view]
            [ui-docs/grid-component grid-view]
            [ui-docs/separator-component separator-view]
            [ui-docs/scroll-component scroll-view]
            [ui-docs/label-component label-view]
            [ui-docs/paragraph-component paragraph-view]
            [ui-docs/heading-component heading-view]
            [ui-docs/icon-component icon-view]
            [ui-docs/button-component button-view]
            [ui-docs/check-box-component check-box-view]
            [ui-docs/select-box-component select-box-view]
            ;; We don't need these components for the initial release, but we
            ;; might want to add them later on:
            #_[ui-docs/text-field-component text-field-view]
            #_[ui-docs/value-field-component value-field-view]
            [ui-docs/string-field-component string-field-view]
            [ui-docs/integer-field-component integer-field-view]
            [ui-docs/number-field-component number-field-view]
            [ui-docs/dialog-button-component dialog-button-view]
            [ui-docs/dialog-component dialog-view]])
         (eduction
           (map (fn [[script-doc lua-fn]]
                  [(:name script-doc) lua-fn]))
           [[ui-docs/show-dialog-doc (make-lua-show-dialog-fn localization)]
            [ui-docs/function-component-doc function-component-lua-fn]
            [ui-docs/show-external-file-dialog-doc (make-show-external-file-dialog-lua-fn project-path localization)]
            [ui-docs/show-external-directory-dialog-doc (make-show-external-directory-dialog-lua-fn project-path localization)]
            [ui-docs/show-resource-dialog-doc (make-show-resource-dialog-lua-fn workspace project)]
            [ui-docs/use-state-doc use-state-hook-lua-fn]
            [ui-docs/use-memo-doc use-memo-hook-lua-fn]])]))
